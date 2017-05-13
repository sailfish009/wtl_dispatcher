// MainDlg.h : interface of the CMainDlg class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include <iostream>
#include <string>
#include <thread>
#include <future>
#include <mutex>
#include <queue>

bool ready = false;

std::thread worker;

void test_func()
{
  if(ready)
  std::cout << "send signal \n";

  ready = false;
}

struct dispatcher
{
  dispatcher()
  {
    _thread = std::move(std::thread(&dispatcher::loop, this));
  }
  void post(std::function<void()> job)
  {
    std::unique_lock<std::mutex> l(_mtx);
    _jobs.push(job);
    _cnd.notify_one();
  }
private:
  void loop()
  {
    for (;;)
    {
      std::function<void()> job;
      {
        std::unique_lock<std::mutex> l(_mtx);
        while (_jobs.empty())
          _cnd.wait(l);
        job.swap(_jobs.front());
        _jobs.pop();
      }
      job();
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
  }
  std::thread                       _thread;
  std::mutex                        _mtx;
  std::condition_variable           _cnd;
  std::queue<std::function<void()>> _jobs;
};
//-------------------------------------------------------------

struct synchronous_job
{
  synchronous_job(std::function<void()> job, dispatcher& d)
    : _job(job)
    , _d(d)
  {
  }
  void run() {
    std::promise<void> p;
    std::future<void> f = p.get_future();

    _d.post(
      [&] {
      cb(std::move(p));
    });

    f.wait();
  }
private:
  void cb(std::promise<void> p)
  {
    _job();
    p.set_value();
  }
  std::function<void()> _job;
  dispatcher&           _d;
};
//-------------------------------------------------------------

static void(*fp_cb)();
struct test
{
  test()
  {
  }
  void run()
  {
    synchronous_job job(fp_cb, _d);
    job.run();
  }
  void set_callback(void(*fp)())
  {
    fp_cb = fp;
  }
private:
  dispatcher _d;
};

void worker_thread()
{
  test t;
  t.set_callback(test_func);

  for (;;)
  {
    t.run();
  }

}


class CMainDlg : public CDialogImpl<CMainDlg>, public CUpdateUI<CMainDlg>,
		public CMessageFilter, public CIdleHandler
{
public:
	enum { IDD = IDD_MAINDLG };

	virtual BOOL PreTranslateMessage(MSG* pMsg)
	{
		return CWindow::IsDialogMessage(pMsg);
	}

	virtual BOOL OnIdle()
	{
		UIUpdateChildWindows();
		return FALSE;
	}

	BEGIN_UPDATE_UI_MAP(CMainDlg)
	END_UPDATE_UI_MAP()

	BEGIN_MSG_MAP(CMainDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAppAbout)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		// center the dialog on the screen
		CenterWindow();

		// set icons
		HICON hIcon = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON));
		SetIcon(hIcon, TRUE);
		HICON hIconSmall = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON));
		SetIcon(hIconSmall, FALSE);

		// register object for message filtering and idle updates
		CMessageLoop* pLoop = _Module.GetMessageLoop();
		ATLASSERT(pLoop != NULL);
		pLoop->AddMessageFilter(this);
		pLoop->AddIdleHandler(this);

		UIAddChildWindowContainer(m_hWnd);

        worker = std::thread([=] {worker_thread(); });
        worker.detach();

		return TRUE;
	}

	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
      worker.~thread();

		// unregister message filtering and idle updates
		CMessageLoop* pLoop = _Module.GetMessageLoop();
		ATLASSERT(pLoop != NULL);
		pLoop->RemoveMessageFilter(this);
		pLoop->RemoveIdleHandler(this);

		return 0;
	}

	LRESULT OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CAboutDlg dlg;
		dlg.DoModal();
		return 0;
	}

	LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
        ready = true;
		return 0;
	}

	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CloseDialog(wID);
		return 0;
	}

	void CloseDialog(int nVal)
	{
		DestroyWindow();
		::PostQuitMessage(nVal);
	}
};
