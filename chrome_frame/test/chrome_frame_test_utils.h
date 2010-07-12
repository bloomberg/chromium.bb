// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_CHROME_FRAME_TEST_UTILS_H_
#define CHROME_FRAME_TEST_CHROME_FRAME_TEST_UTILS_H_

#include <atlbase.h>
#include <atlcom.h>
#include <string>
#include <exdisp.h>
#include <exdispid.h>
#include <mshtml.h>
#include <shlguid.h>
#include <shobjidl.h>
#include <windows.h>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/scoped_comptr_win.h"
#include "base/scoped_variant_win.h"

#include "chrome_frame/test_utils.h"
#include "chrome_frame/test/simulate_input.h"
#include "chrome_frame/test/window_watchdog.h"
#include "chrome_frame/utils.h"

// Include without path to make GYP build see it.
#include "chrome_tab.h"  // NOLINT

namespace chrome_frame_test {

int CloseVisibleWindowsOnAllThreads(HANDLE process);

base::ProcessHandle LaunchFirefox(const std::wstring& url);
base::ProcessHandle LaunchOpera(const std::wstring& url);
base::ProcessHandle LaunchIE(const std::wstring& url);
base::ProcessHandle LaunchSafari(const std::wstring& url);
base::ProcessHandle LaunchChrome(const std::wstring& url);

// Attempts to close all open IE windows.
// The return value is the number of windows closed.
// @note: this function requires COM to be initialized on the calling thread.
// Since the caller might be running in either MTA or STA, the function does
// not perform this initialization itself.
int CloseAllIEWindows();

extern const wchar_t kIEImageName[];
extern const wchar_t kIEBrokerImageName[];
extern const wchar_t kFirefoxImageName[];
extern const wchar_t kOperaImageName[];
extern const wchar_t kSafariImageName[];
extern const char kChromeImageName[];
extern const wchar_t kChromeLauncher[];
extern const int kChromeFrameLongNavigationTimeoutInSeconds;

// Temporarily impersonate the current thread to low integrity for the lifetime
// of the object. Destructor will automatically revert integrity level.
class LowIntegrityToken {
 public:
  LowIntegrityToken();
  ~LowIntegrityToken();
  BOOL Impersonate();
  BOOL RevertToSelf();
 protected:
  static bool IsImpersonated();
  bool impersonated_;
};

// MessageLoopForUI wrapper that runs only for a limited time.
// We need a UI message loop in the main thread.
class TimedMsgLoop {
 public:
  TimedMsgLoop() : quit_loop_invoked_(false) {}

  void RunFor(int seconds) {
    QuitAfter(seconds);
    quit_loop_invoked_ = false;
    loop_.MessageLoop::Run();
  }

  void PostDelayedTask(
    const tracked_objects::Location& from_here, Task* task, int64 delay_ms) {
      loop_.PostDelayedTask(from_here, task, delay_ms);
  }

  void Quit() {
    QuitAfter(0);
  }

  void QuitAfter(int seconds) {
    quit_loop_invoked_ = true;
    loop_.PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask, 1000 * seconds);
  }

  bool WasTimedOut() const {
    return !quit_loop_invoked_;
  }

 private:
  MessageLoopForUI loop_;
  bool quit_loop_invoked_;
};

// Saves typing. It's somewhat hard to create a wrapper around
// testing::InvokeWithoutArgs since it returns a
// non-public (testing::internal) type.
#define QUIT_LOOP(loop) testing::InvokeWithoutArgs(\
  testing::CreateFunctor(&loop, &chrome_frame_test::TimedMsgLoop::Quit))

#define QUIT_LOOP_SOON(loop, seconds) testing::InvokeWithoutArgs(\
  testing::CreateFunctor(&loop, &chrome_frame_test::TimedMsgLoop::QuitAfter, \
  seconds))

// Launches IE as a COM server and returns the corresponding IWebBrowser2
// interface pointer.
// Returns S_OK on success.
HRESULT LaunchIEAsComServer(IWebBrowser2** web_browser);

FilePath GetProfilePath(const std::wstring& suffix);

#ifndef DISPID_NEWPROCESS
#define DISPID_NEWPROCESS  284
#endif  // DISPID_NEWPROCESS

// This class sets up event sinks to the IWebBrowser interface. Currently it
// subscribes to the following events:-
// 1. DISPID_BEFORENAVIGATE2
// 2. DISPID_NAVIGATEERROR
// 3. DISPID_NAVIGATECOMPLETE2
// 4. DISPID_NEWWINDOW3
// 5. DISPID_DOCUMENTCOMPLETE
// Other events can be subscribed to on an if needed basis.
class WebBrowserEventSink
    : public CComObjectRootEx<CComMultiThreadModel>,
      public IDispEventSimpleImpl<0, WebBrowserEventSink,
                                  &DIID_DWebBrowserEvents2>,
      public WindowObserver,
      public IUnknown {
 public:
  typedef IDispEventSimpleImpl<0, WebBrowserEventSink,
                               &DIID_DWebBrowserEvents2> DispEventsImpl;
  WebBrowserEventSink()
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          onmessage_(this, &WebBrowserEventSink::OnMessageInternal)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          onloaderror_(this, &WebBrowserEventSink::OnLoadErrorInternal)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          onload_(this, &WebBrowserEventSink::OnLoadInternal)),
      process_id_to_wait_for_(0),
      is_main_browser_object_(false) {
  }

  ~WebBrowserEventSink() {
    Uninitialize();
  }

BEGIN_COM_MAP(WebBrowserEventSink)
  COM_INTERFACE_ENTRY(IUnknown)
END_COM_MAP()

BEGIN_SINK_MAP(WebBrowserEventSink)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_BEFORENAVIGATE2,
                  OnBeforeNavigate2Internal, &kBeforeNavigate2Info)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_DOWNLOADBEGIN,
                  OnDownloadBegin, &kVoidMethodInfo)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_NAVIGATECOMPLETE2,
                  OnNavigateComplete2Internal, &kNavigateComplete2Info)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_NAVIGATEERROR,
                  OnNavigateError, &kNavigateErrorInfo)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_NEWWINDOW2,
                  OnNewWindow2, &kNewWindow2Info)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_NEWWINDOW3,
                  OnNewWindow3Internal, &kNewWindow3Info)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_DOCUMENTCOMPLETE,
                  OnDocumentCompleteInternal, &kDocumentCompleteInfo)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_FILEDOWNLOAD,
                  OnFileDownloadInternal, &kFileDownloadInfo)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_ONQUIT,
                  OnQuitInternal, &kVoidMethodInfo)
END_SINK_MAP()

  void Attach(IDispatch* browser_disp);
  void Uninitialize();

  // Helper function to launch IE and navigate to a URL.
  // Returns S_OK on success, S_FALSE if the test was not run, other
  // errors on failure.
  HRESULT LaunchIEAndNavigate(const std::wstring& navigate_url);

  virtual HRESULT Navigate(const std::wstring& navigate_url);

  // Set input focus to chrome frame window.
  void SetFocusToChrome();

  // Send keyboard input to the renderer window hosted in chrome using
  // SendInput API.
  void SendKeys(const wchar_t* input_string);

  // Send mouse click to the renderer window hosted in chrome using
  // SendInput API.
  void SendMouseClick(int x, int y, simulate_input::MouseButton button);

  // Send mouse click to the renderer window hosted in IE using
  // SendInput API.
  void SendMouseClickToIE(int x, int y, simulate_input::MouseButton button);

  void Exec(const GUID* cmd_group_guid, DWORD command_id,
            DWORD cmd_exec_opt, VARIANT* in_args, VARIANT* out_args);

  // Watch for new window created.
  void WatchChromeWindow(const wchar_t* window_class);
  void StopWatching();

  // Overridable methods for the mock.
  STDMETHOD_(void, OnNavigateError)(IDispatch* dispatch, VARIANT* url,
                                    VARIANT* frame_name, VARIANT* status_code,
                                    VARIANT* cancel) {
    DLOG(INFO) << __FUNCTION__;
  }

  STDMETHOD_(void, OnBeforeNavigate2)(IDispatch* dispatch, VARIANT* url,
                               VARIANT* flags, VARIANT* target_frame_name,
                               VARIANT* post_data, VARIANT* headers,
                               VARIANT_BOOL* cancel) {}
  STDMETHOD_(void, OnDownloadBegin)() {}
  STDMETHOD_(void, OnNavigateComplete2)(IDispatch* dispatch, VARIANT* url) {}
  STDMETHOD_(void, OnNewWindow2)(IDispatch** dispatch, VARIANT_BOOL* cancel) {}
  STDMETHOD_(void, OnNewWindow3)(IDispatch** dispatch, VARIANT_BOOL* cancel,
                                 DWORD flags, BSTR url_context, BSTR url) {}
  STDMETHOD_(void, OnDocumentComplete)(IDispatch* dispatch,
                                       VARIANT* url) {}

  STDMETHOD_(void, OnFileDownloadInternal)(VARIANT_BOOL active_doc,
                                           VARIANT_BOOL* cancel);
  STDMETHOD_(void, OnFileDownload)(VARIANT_BOOL active_doc,
                                   VARIANT_BOOL* cancel) {}
  STDMETHOD_(void, OnQuit)() {}
  STDMETHOD_(void, OnQuitInternal)() {
    DLOG(INFO) << __FUNCTION__;
    // Grab the process id here in case it will be too late to do it
    // in Uninitialize.
    HWND hwnd = NULL;
    web_browser2_->get_HWND(reinterpret_cast<SHANDLE_PTR*>(&hwnd));
    if (::IsWindow(hwnd))
      ::GetWindowThreadProcessId(hwnd, &process_id_to_wait_for_);

    OnQuit();
    CoDisconnectObject(this, 0);
    DispEventUnadvise(web_browser2_);
  }
#ifdef _DEBUG
  STDMETHOD(Invoke)(DISPID dispid, REFIID riid,
    LCID lcid, WORD flags, DISPPARAMS* params, VARIANT* result,
    EXCEPINFO* except_info, UINT* arg_error) {
    DLOG(INFO) << __FUNCTION__ << L" disp id :"  << dispid;
    return DispEventsImpl::Invoke(dispid, riid, lcid, flags, params, result,
                                  except_info, arg_error);
  }
#endif  // _DEBUG

  // Chrome frame callbacks
  virtual void OnLoad(const wchar_t* url) {}
  virtual void OnLoadError(const wchar_t* url) {}
  virtual void OnMessage(const wchar_t* message, const wchar_t* origin,
                         const wchar_t* source) {}
  virtual void OnNewBrowserWindow(IDispatch* new_window, const wchar_t* url) {}

  // Window watchdog override
  virtual void OnWindowDetected(HWND hwnd, const std::string& caption) {}

  IWebBrowser2* web_browser2() {
    return web_browser2_.get();
  }

  HRESULT SetWebBrowser(IWebBrowser2* web_browser2);
  void ExpectRendererWindowHasFocus();
  void ExpectIERendererWindowHasFocus();
  void ExpectAddressBarUrl(const std::wstring& url);

  // Closes the web browser in such a way that the OnQuit notification will
  // be fired when the window closes (async).
  HRESULT CloseWebBrowser();

 protected:
  STDMETHOD(OnBeforeNavigate2Internal)(IDispatch* dispatch, VARIANT* url,
                                       VARIANT* flags,
                                       VARIANT* target_frame_name,
                                       VARIANT* post_data, VARIANT* headers,
                                       VARIANT_BOOL* cancel);
  STDMETHOD_(void, OnNavigateComplete2Internal)(IDispatch* dispatch,
                                                VARIANT* url);
  STDMETHOD_(void, OnDocumentCompleteInternal)(IDispatch* dispatch,
                                               VARIANT* url);
  STDMETHOD_(void, OnNewWindow3Internal)(IDispatch** dispatch,
                                         VARIANT_BOOL* cancel, DWORD flags,
                                         BSTR url_context, BSTR url);

  // IChromeFrame callbacks
  HRESULT OnLoadInternal(const VARIANT* param);
  HRESULT OnLoadErrorInternal(const VARIANT* param);
  HRESULT OnMessageInternal(const VARIANT* param);

  void ConnectToChromeFrame();
  void DisconnectFromChromeFrame();
  HWND GetRendererWindow();
  HWND GetIERendererWindow();

 public:
  ScopedComPtr<IWebBrowser2> web_browser2_;
  ScopedComPtr<IChromeFrame> chrome_frame_;
  DispCallback<WebBrowserEventSink> onmessage_;
  DispCallback<WebBrowserEventSink> onloaderror_;
  DispCallback<WebBrowserEventSink> onload_;
  base::ProcessId process_id_to_wait_for_;

  // Set to true if this instance was used to launch the browser process.
  // For instances used to connect to popup windows etc, this will be
  // set to false.
  bool is_main_browser_object_;

 protected:
  static _ATL_FUNC_INFO kBeforeNavigate2Info;
  static _ATL_FUNC_INFO kNavigateComplete2Info;
  static _ATL_FUNC_INFO kNavigateErrorInfo;
  static _ATL_FUNC_INFO kNewWindow2Info;
  static _ATL_FUNC_INFO kNewWindow3Info;
  static _ATL_FUNC_INFO kVoidMethodInfo;
  static _ATL_FUNC_INFO kDocumentCompleteInfo;
  static _ATL_FUNC_INFO kFileDownloadInfo;

  WindowWatchdog window_watcher_;
};

// Returns the path of the exe passed in.
std::wstring GetExecutableAppPath(const std::wstring& file);

// Returns the profile path to be used for IE. This varies as per version.
FilePath GetProfilePathForIE();

// Returns the version of the exe passed in.
std::wstring GetExeVersion(const std::wstring& exe_path);

// Returns the version of Internet Explorer on the machine.
IEVersion GetInstalledIEVersion();

// Returns the folder for CF test data.
FilePath GetTestDataFolder();

// Returns the path portion of the url.
std::wstring GetPathFromUrl(const std::wstring& url);

// Returns the path and query portion of the url.
std::wstring GetPathAndQueryFromUrl(const std::wstring& url);

// Adds the CF meta tag to the html page. Returns true if successful.
bool AddCFMetaTag(std::string* html_data);

// Posts a delayed task to send an extended keystroke |repeat| times with an
// optional modifier via SendInput. Following this, the enter key is sent.
void DelaySendExtendedKeysEnter(TimedMsgLoop* loop, int delay, char c,
                                int repeat, simulate_input::Modifier mod);

// A convenience class to close all open IE windows at the end
// of a scope.  It's more convenient to do it this way than to
// explicitly call chrome_frame_test::CloseAllIEWindows at the
// end of a test since part of the test's cleanup code may be
// in object destructors that would run after CloseAllIEWindows
// would get called.
// Ideally all IE windows should be closed when this happens so
// if the test ran normally, we should not have any windows to
// close at this point.
class CloseIeAtEndOfScope {
 public:
  CloseIeAtEndOfScope() {}
  ~CloseIeAtEndOfScope() {
    int closed = CloseAllIEWindows();
    DLOG_IF(ERROR, closed != 0)
        << StringPrintf("Closed %i windows forcefully", closed);
  }
};

// Starts the Chrome crash service which enables us to gather crash dumps
// during test runs.
base::ProcessHandle StartCrashService();

}  // namespace chrome_frame_test

#endif  // CHROME_FRAME_TEST_CHROME_FRAME_TEST_UTILS_H_
