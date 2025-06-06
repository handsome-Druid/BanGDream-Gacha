// BanGDream Gacha.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "BanGDream Gacha.h"
#include <shellapi.h>

#include <thread>
#include <vector>
#include <set>
#include <random>
#include <algorithm>
#include <chrono>
#include <mutex>
#include <numeric>
#include <string>

#define MAX_LOADSTRING 100

// 全局变量:
HINSTANCE hInst;                                // 当前实例
WCHAR szTitle[MAX_LOADSTRING];                  // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING];            // 主窗口类名

// 控件句柄
HWND hEdit5StarTotal, hEdit5StarWant, hEdit4StarWant, hEditSimCount, hEditThreadCount;
HWND hButtonStart, hStaticResult, hCheckNormal;
std::thread g_simThread;
std::mutex g_resultMutex;
bool g_isSimulating = false;

// 此代码模块中包含的函数的前向声明:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

// 抽卡随机数类
class GachaRandom {
private:
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_real_distribution<> dis;
    std::uniform_int_distribution<> dis_5star;
public:
    GachaRandom(int total_5star)
        : gen(rd()), dis(0.0, 1.0), dis_5star(1, total_5star > 0 ? total_5star : 1) {
    }
    double get_random() { return dis(gen); }
    int get_5star_random() { return dis_5star(gen); }
};

// 抽卡模拟器类
class GachaSimulator {
public:
    static int simulate_one_round(int total_5star, int want_5star, int total_4star, int want_4star, int normal, GachaRandom& random) {
        std::set<int> cards_5star, cards_4star;
        int draws = 0, choose_times_have = 0;
        while (true) {
            draws++;
            if (draws % 50 == 0 && normal == 1) {
                if (want_5star > 0) {
                    int roll = random.get_5star_random();
                    if (roll <= want_5star) cards_5star.insert(roll);
                }
                goto next_draw;
            }
            else {
                double rand = random.get_random();
                if (want_5star > 0) {
                    for (int i = 1; i <= want_5star; i++) {
                        if (rand < 0.005 * total_5star * (i / (double)total_5star)) {
                            cards_5star.insert(i);
                            goto next_draw;
                        }
                    }
                }
                else {
                    rand = random.get_random();
                    for (int i = 1; i <= want_4star; i++) {
                        if (rand < 0.0075 * total_4star * (i / (double)total_4star)) {
                            cards_4star.insert(i);
                            break;
                        }
                    }
                    goto next_draw;
                }
                if (total_4star > 0 && want_4star > 0) {
                    rand = random.get_random();
                    for (int i = 1; i <= want_4star; i++) {
                        if (rand < 0.0075 * total_4star * (i / (double)total_4star)) {
                            cards_4star.insert(i);
                            break;
                        }
                    }
                }
            }
        next_draw:
            choose_times_have = (draws + 100) / 300;
            if (cards_5star.size() + cards_4star.size() + choose_times_have >= want_5star + want_4star) break;
        }
        return draws;
    }

    static std::wstring calculate_statistics(
        int total_5star, int want_5star, int total_4star, int want_4star, int normal,
        int simulations, unsigned int thread_count)
    {
        auto start_time = std::chrono::high_resolution_clock::now();
        std::vector<std::vector<int>> all_draw_counts(thread_count);
        long long total_draws = 0;
        std::vector<std::thread> threads;
        int sims_per_thread = simulations / thread_count;

        for (unsigned int i = 0; i < thread_count; ++i) {
            threads.emplace_back([&, i]() {
                GachaRandom random(total_5star);
                int start = i * sims_per_thread;
                int end = (i == thread_count - 1) ? simulations : (i + 1) * sims_per_thread;
                all_draw_counts[i].reserve(end - start);
                for (int j = start; j < end; ++j) {
                    int draws = simulate_one_round(total_5star, want_5star, total_4star, want_4star, normal, random);
                    all_draw_counts[i].push_back(draws);
                }
                });
        }
        for (auto& thread : threads) thread.join();

        // 合并所有线程的结果
        std::vector<int> draw_counts;
        draw_counts.reserve(simulations);
        for (auto& v : all_draw_counts) {
            draw_counts.insert(draw_counts.end(), v.begin(), v.end());
        }
        total_draws = std::accumulate(draw_counts.begin(), draw_counts.end(), 0LL);
        double expected_draws = static_cast<double>(total_draws) / simulations;
        std::sort(draw_counts.begin(), draw_counts.end());
        int percentile_50 = draw_counts[simulations / 2];
        int percentile_90 = draw_counts[static_cast<int>(simulations * 0.9)];
        int neineigeneinei = draw_counts.back();
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        double seconds = duration.count() / 1000.0;

        wchar_t buf[1024];
        swprintf(buf, 1024,
            L"期望抽卡次数: %.2f\n中位数: %d\n90%%玩家≤: %d\n最大抽数: %d\n耗时: %.2f秒",
            expected_draws, percentile_50, percentile_90, neineigeneinei, seconds);
        return buf;
    }

};

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // 初始化全局字符串
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_BANGDREAMGACHA, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // 执行应用程序初始化:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_BANGDREAMGACHA));

    MSG msg;

    // 主消息循环:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_BANGDREAMGACHA));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_BANGDREAMGACHA);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // 将实例句柄存储在全局变量中

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 600, 400, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // 创建控件
    CreateWindowW(L"STATIC", L"5星总数:", WS_VISIBLE | WS_CHILD, 20, 20, 80, 24, hWnd, NULL, hInst, NULL);
    hEdit5StarTotal = CreateWindowW(L"EDIT", L"3", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER, 110, 20, 60, 24, hWnd, NULL, hInst, NULL);

    CreateWindowW(L"STATIC", L"想要5星:", WS_VISIBLE | WS_CHILD, 20, 60, 80, 24, hWnd, NULL, hInst, NULL);
    hEdit5StarWant = CreateWindowW(L"EDIT", L"1", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER, 110, 60, 60, 24, hWnd, NULL, hInst, NULL);

    CreateWindowW(L"STATIC", L"想要4星:", WS_VISIBLE | WS_CHILD, 20, 100, 80, 24, hWnd, NULL, hInst, NULL);
    hEdit4StarWant = CreateWindowW(L"EDIT", L"0", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER, 110, 100, 60, 24, hWnd, NULL, hInst, NULL);

    CreateWindowW(L"STATIC", L"模拟次数:", WS_VISIBLE | WS_CHILD, 20, 140, 80, 24, hWnd, NULL, hInst, NULL);
    hEditSimCount = CreateWindowW(L"EDIT", L"10000", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER, 110, 140, 80, 24, hWnd, NULL, hInst, NULL);

    CreateWindowW(L"STATIC", L"线程数:", WS_VISIBLE | WS_CHILD, 20, 180, 80, 24, hWnd, NULL, hInst, NULL);
    hEditThreadCount = CreateWindowW(L"EDIT", L"4", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER, 110, 180, 60, 24, hWnd, NULL, hInst, NULL);

    hCheckNormal = CreateWindowW(L"BUTTON", L"50小保底", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 20, 220, 100, 24, hWnd, (HMENU)1001, hInst, NULL);

    hButtonStart = CreateWindowW(L"BUTTON", L"开始模拟", WS_VISIBLE | WS_CHILD, 20, 260, 150, 32, hWnd, (HMENU)1002, hInst, NULL);

    hStaticResult = CreateWindowW(L"STATIC", L"", WS_VISIBLE | WS_CHILD | SS_LEFT, 200, 20, 370, 300, hWnd, NULL, hInst, NULL);

    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        if ((HWND)lParam == hButtonStart || wmId == 1002) {
            if (g_isSimulating) break;
            g_isSimulating = true;
            SetWindowTextW(hStaticResult, L"计算中...");
            // 读取参数
            wchar_t buf[32];
            GetWindowTextW(hEdit5StarTotal, buf, 32);
            int total_5star = _wtoi(buf);
            GetWindowTextW(hEdit5StarWant, buf, 32);
            int want_5star = _wtoi(buf);
            GetWindowTextW(hEdit4StarWant, buf, 32);
            int want_4star = _wtoi(buf);
            GetWindowTextW(hEditSimCount, buf, 32);
            int sim_count = _wtoi(buf);
            GetWindowTextW(hEditThreadCount, buf, 32);
            int thread_count = _wtoi(buf);
            int normal = (SendMessageW(hCheckNormal, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
            // 参数校验
            if (total_5star < 0 || want_5star < 0 || want_4star < 0 || sim_count < 100 || thread_count < 1) {
                SetWindowTextW(hStaticResult, L"参数有误，请检查输入！");
                g_isSimulating = false;
                break;
            }
            if (want_5star > total_5star) {
                SetWindowTextW(hStaticResult, L"想要的5星数量不能大于总数！");
                g_isSimulating = false;
                break;
            }
            // 启动后台线程
            g_simThread = std::thread([=]() {
                std::wstring result = GachaSimulator::calculate_statistics(
                    total_5star, want_5star, want_4star, want_4star, normal, sim_count, thread_count);
                {
                    std::lock_guard<std::mutex> lock(g_resultMutex);
                    SetWindowTextW(hStaticResult, result.c_str());
                    g_isSimulating = false;
                }
                });
            g_simThread.detach();
            break;
        }
        // 支持菜单
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;
    case WM_KEYDOWN:
        if (wParam == VK_RETURN && !g_isSimulating) {
            SendMessageW(hWnd, WM_COMMAND, 1002, (LPARAM)hButtonStart);
        }
        break;
    case WM_LBUTTONDOWN:
        SetFocus(hWnd);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// “关于”框的消息处理程序。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        // 可选：设置字体为下划线蓝色
    {
        HWND hLink = GetDlgItem(hDlg, IDC_STATIC_LINK);
        HFONT hFont = (HFONT)SendMessage(hLink, WM_GETFONT, 0, 0);
        LOGFONT lf;
        GetObject(hFont, sizeof(lf), &lf);
        lf.lfUnderline = TRUE;
        lf.lfWeight = FW_NORMAL;
        wcscpy_s(lf.lfFaceName, L"MS Shell Dlg");
        HFONT hFontUnderline = CreateFontIndirect(&lf);
        SendMessage(hLink, WM_SETFONT, (WPARAM)hFontUnderline, TRUE);
        SetTextColor(GetDC(hLink), RGB(0, 0, 255));
    }
    return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_STATIC_LINK && HIWORD(wParam) == STN_CLICKED)
        {
            ShellExecute(NULL, L"open", L"https://github.com/handsome-Druid/BanGDream-Gacha", NULL, NULL, SW_SHOWNORMAL);
            return (INT_PTR)TRUE;
        }
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

