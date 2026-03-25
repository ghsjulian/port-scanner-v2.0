#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <queue>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ncurses.h>
#include <atomic>
#include <chrono>
#include <fstream>

using namespace std;

mutex mtx;
queue<int> portQueue;
atomic<int> scannedPorts(0);
vector<int> openPorts;

void scanPort(const string &ip) {
    while (true) {
        mtx.lock();
        if (portQueue.empty()) {
            mtx.unlock();
            return;
        }
        int port = portQueue.front();
        portQueue.pop();
        mtx.unlock();

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

        struct timeval timeout{};
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            mtx.lock();
            openPorts.push_back(port);
            attron(COLOR_PAIR(2));
            printw("[OPEN] Port %d\n", port);
            attroff(COLOR_PAIR(2));
            refresh();
            mtx.unlock();
        }
        close(sock);

        scannedPorts++;
    }
}

int main() {
    initscr();
    start_color();
    use_default_colors();
    init_pair(1, COLOR_GREEN, -1); // header
    init_pair(2, COLOR_CYAN, -1);  // open ports
    init_pair(3, COLOR_RED, -1);   // footer/progress

    attron(COLOR_PAIR(1));
    printw("\n=== Advanced Port Scanner ===\n");
    attroff(COLOR_PAIR(1));
    printw("\n[+] Enter Target IP : ");
    char ip[50];
    getstr(ip);
    string target(ip);

    int startPort, endPort;
    printw("\n[+] Enter Start Port(0) : ");
    scanw("%d", &startPort);
    printw("\n[+] Enter End Port(56000) : ");
    scanw("%d", &endPort);

    // Fill port queue
    for (int p = startPort; p <= endPort; p++) portQueue.push(p);

    printw("\n[+] Starting Scan...\n");
    refresh();

    int numThreads = 200; // safe for Termux
    vector<thread> threads;
    for (int i = 0; i < numThreads; i++) threads.push_back(thread(scanPort, target));

    // Progress monitor
    while (scannedPorts < (endPort - startPort + 1)) {
        mtx.lock();
        attron(COLOR_PAIR(3));
        printw("\r[!] Progress : %d/%d", scannedPorts.load(), endPort - startPort + 1);
        attroff(COLOR_PAIR(3));
        refresh();
        mtx.unlock();
        this_thread::sleep_for(chrono::milliseconds(100));
    }

    for (auto &t : threads) t.join();

    // Save results
    ofstream file("open_ports.txt");
    for (int port : openPorts) file << "[+] OPEN PORT : " << port << "\n";
    file.close();

    attron(COLOR_PAIR(1));
    printw("\n=== Scanning Completed ===\n");
    attroff(COLOR_PAIR(1));
    printw("\n[+] Open Ports Saved To 'open_ports.txt' File.\n");
    printw("\nPress any key to exit...\n");
    refresh();
    getch();
    endwin();
    
    return 0;
}

