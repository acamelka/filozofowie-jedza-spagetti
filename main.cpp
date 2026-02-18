#include <thread>
#include <mutex>
#include <vector>
#include <ncursesw/ncurses.h>
#include <atomic>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <iostream>
#include <condition_variable>

using namespace std;

enum Stan { MYSLI, GLODNY, JE };

struct Filozof {
    int id;
    Stan stan;
    int progres_ms;
    int czynnosc_ms;
    int ile_glodny_ms;
};

struct Widelec {
    int id_fil;
    bool uzycie;
};

int N;
vector<Filozof> filozofowie;
vector<Widelec> widelce;

mutex mutex_widelec;
condition_variable cv_widelce;

atomic<bool> running(true);

int random_czas(int min_czas, int max_czas) {
    return min_czas + rand() % (max_czas - min_czas + 1);
}

void f_run(Filozof* p) {
    const int krok = 100;
    int id = p->id;

    while (running) {

        p->stan = MYSLI;
        int mysli_ms = random_czas(1500, 3000);
        p->czynnosc_ms = mysli_ms;
        p->progres_ms = 0;

        for (int t = 0;running&&t <= mysli_ms; t += krok) {
            usleep(krok * 1000);
            p->progres_ms = t;
        }

        p->stan = GLODNY;
        p->czynnosc_ms = 0;
        p->progres_ms = 0;

        int lewy = id;
        int prawy = (id + 1) % N;

{
    unique_lock<mutex> lk(mutex_widelec);

    while ((widelce[lewy].uzycie || widelce[prawy].uzycie)&&running) {

        cv_widelce.wait_for(lk, std::chrono::milliseconds(100));

        p->ile_glodny_ms += 100;
    }

    widelce[lewy].uzycie = true;
    widelce[prawy].uzycie = true;
    widelce[lewy].id_fil = p->id;
    widelce[prawy].id_fil = p->id;

    p->ile_glodny_ms = 0;
}

        p->stan = JE;
        int jedzenie_ms = random_czas(1000, 3000);
        p->czynnosc_ms = jedzenie_ms;

        for (int t = 0; running&& t <=jedzenie_ms; t += krok) {
            usleep(krok * 1000);
            p->progres_ms = t;
        }

        {
            lock_guard<mutex> lk(mutex_widelec);
            widelce[lewy].uzycie = false;
            widelce[prawy].uzycie = false;
        }

        cv_widelce.notify_all();
    }
}

void obraz() {
    clear();
    mvprintw(0, 0, "%d Filozofow", N);
    mvprintw(2, 0, "Filozof  id | stan | czas glodu | progres:");

    for (int i = 0; i < N; ++i) {
        int dl = 20;
        int postep = 0;
        string pasek(dl, ' ');
        Filozof& p = filozofowie[i];

        const char* st = (p.stan == MYSLI ? "MYSLI" :
                          (p.stan == GLODNY ? "GLODNY" : "JE"));

        if (p.czynnosc_ms > 0) {
            double postep_ulamek = (double)p.progres_ms / p.czynnosc_ms;
            if (postep_ulamek < 0) postep_ulamek = 0;
            else if (postep_ulamek > 1) postep_ulamek = 1;
            postep = (int)(postep_ulamek * dl);
        }

        for (int b=0;b<postep;++b) pasek[b]='#';
        mvprintw(4+i,0,"Filozof %2d | %-6s | %4d ms | %4d/%4d ms  | [%s]",
                 i,st,p.ile_glodny_ms,p.progres_ms,p.czynnosc_ms, pasek.c_str());
    }

   
    int wiersz=6+N;
    mvprintw(wiersz,0,"Widelec  id | id_fil | uzycie:");

    {
        lock_guard<mutex> lk(mutex_widelec);
        for (int i = 0; i < N; ++i) {
            mvprintw(wiersz + i + 1, 0, "Widelec %2d | %2d | %-7s",
                     i, widelce[i].id_fil, widelce[i].uzycie ? "uzywany" : "wolny");
        }
    }

    refresh();
}

void start() {
    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, TRUE);
    curs_set(0);

    while (running) {
        int ch = getch();
        if (ch == 'x' || ch == 'X') {
            running = false;
            break;
        }
        obraz();
        usleep(250000);
    }

    endwin();
}

int main(int argc, char** argv) {

    N = atoi(argv[1]);
    if (N < 5) {
        cerr << "BLAD";
        return 1;
    }

    srand(time(NULL));

    filozofowie.resize(N);
    for (int i = 0; i < N; ++i) {
        filozofowie[i] = { i, MYSLI, 0, 0, 0 };
    }

    widelce.resize(N);
    for (int i = 0; i < N; ++i) {
        widelce[i].id_fil = -1;
        widelce[i].uzycie = false;
    }

    vector<thread> threads;
    thread ui(start);

    for (int i = 0; i < N; ++i) {
        threads.emplace_back(f_run, &filozofowie[i]);
    }

    for (auto& t : threads) t.join();
    ui.join();

    return 0;
}