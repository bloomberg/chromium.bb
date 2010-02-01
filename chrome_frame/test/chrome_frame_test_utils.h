// Copyright (c) 2009 The Chromium Authors. All rights reserved.
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
// Include without path to make GYP build see it.
#include "chrome_tab.h"  // NOLINT

namespace chrome_frame_test {

bool IsTopLevelWindow(HWND window);
int CloseVisibleWindowsOnAllThreads(HANDLE process);
bool ForceSetForegroundWindow(HWND window);
bool EnsureProcessInForeground(base::ProcessId process_id);

// Iterates through all the characters in the string and simulates
// keyboard input.  The input goes to the currently active application.
bool SendString(const wchar_t* s);

// Sends a virtual key such as VK_TAB, VK_RETURN or a character that has been
// translated to a virtual key.
// The extended flag indicates if this is an extended key
void SendVirtualKey(int16 key, bool extended);

// Translates a single char to a virtual key and calls SendVirtualKey.
void SendChar(char c);

// Sends an ascii string, char by char (calls SendChar for each).
void SendString(const char* s);

// Sends a keystroke to the currently active application with optional
// modifiers set.
bool SendMnemonic(WORD mnemonic_char, bool shift_pressed, bool control_pressed,
                  bool alt_pressed);

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
extern const wchar_t kChromeImageName[];

// Displays the chrome frame context menu by posting mouse move messages to
// Chrome
void ShowChromeFrameContextMenu();

// Sends keyboard messages to the chrome frame context menu to select the About
// Chrome frame option.
void SelectAboutChromeFrame();

// Returns a handle to the chrome frame render widget child window.
// Returns NULL on failure.
HWND GetChromeRendererWindow();

// Sends the specified input to the window passed in.
void SendInputToWindow(HWND window, const std::string& input_string);

// Helper function to set keyboard focus to a window. This is achieved by
// sending a mouse move followed by a mouse down/mouse up combination to the
// window.
void SetKeyboardFocusToWindow(HWND window, int x, int y);

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
  void RunFor(int seconds) {
    QuitAfter(seconds);
    loop_.MessageLoop::Run();
  }

  void PostDelayedTask(
    const tracked_objects::Location& from_here, Task* task, int64 delay_ms) {
      loop_.PostDelayedTask(from_here, task, delay_ms);
  }

  void Quit() {
    loop_.PostTask(FROM_HERE, new MessageLoop::QuitTask);
  }

  void QuitAfter(int seconds) {
    loop_.PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask, 1000 * seconds);
  }

  MessageLoopForUI loop_;
};

// Saves typing. It's somewhat hard to create a wrapper around
// testing::InvokeWithoutArgs since it returns a
// non-public (testing::internal) type.
#define QUIT_LOOP(loop) testing::InvokeWithoutArgs(\
  CreateFunctor(&loop, &chrome_frame_test::TimedMsgLoop::Quit))

#define QUIT_LOOP_SOON(loop, seconds) testing::InvokeWithoutArgs(\
  CreateFunctor(&loop, &chrome_frame_test::TimedMsgLoop::QuitAfter, \
  seconds))

// Launches IE as a COM server and returns the corresponding IWebBrowser2
// interface pointer.
// Returns S_OK on success.
HRESULT LaunchIEAsComServer(IWebBrowser2** web_browser);

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
                                  &DIID_DWebBrowserEvents2> {
 public:
  typedef IDispEventSimpleImpl<0, WebBrowserEventSink,
                               &DIID_DWebBrowserEvents2> DispEventsImpl;
  WebBrowserEventSink()
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          onmessage_(this, &WebBrowserEventSink::OnMessageInternal)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          onloaderror_(this, &WebBrowserEventSink::OnLoadErrorInternal)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          onload_(this, &WebBrowserEventSink::OnLoadInternal)) {
  }

  ~WebBrowserEventSink() {
    Uninitialize();
  }

  void Uninitialize();

  // Helper function to launch IE and navigate to a URL.
  // Returns S_OK on success, S_FALSE if the test was not run, other
  // errors on failure.
  HRESULT LaunchIEAndNavigate(const std::wstring& navigate_url);

  virtual HRESULT Navigate(const std::wstring& navigate_url);

  // Set input focus to chrome frame window.
  void SetFocusToChrome();

  // Send keyboard input to the renderer window hosted in chrome using
  // SendInput API
  void SendInputToChrome(const std::string& input_string);

BEGIN_COM_MAP(WebBrowserEventSink)
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
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_NEWWINDOW3,
                  OnNewWindow3, &kNewWindow3Info)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_DOCUMENTCOMPLETE,
                  OnDocumentCompleteInternal, &kDocumentCompleteInfo)
END_SINK_MAP()

  STDMETHOD_(void, OnNavigateError)(IDispatch* dispatch, VARIANT* url,
                                    VARIANT* frame_name, VARIANT* status_code,
                                    VARIANT* cancel) {
    DLOG(INFO) << __FUNCTION__;
  }

  STDMETHOD(OnBeforeNavigate2)(IDispatch* dispatch, VARIANT* url, VARIANT*
                               flags, VARIANT* target_frame_name,
                               VARIANT* post_data, VARIANT* headers,
                               VARIANT_BOOL* cancel) {
    return S_OK;
  }

  STDMETHOD(OnBeforeNavigate2Internal)(IDispatch* dispatch, VARIANT* url,
                                       VARIANT* flags,
                                       VARIANT* target_frame_name,
                                       VARIANT* post_data, VARIANT* headers,
                                       VARIANT_BOOL* cancel);
  STDMETHOD_(void, OnDownloadBegin)() {}
  STDMETHOD_(void, OnNavigateComplete2Internal)(IDispatch* dispatch,
                                                VARIANT* url);
  STDMETHOD_(void, OnNavigateComplete2)(IDispatch* dispatch, VARIANT* url) {}
  STDMETHOD_(void, OnNewWindow3)(IDispatch** dispatch, VARIANT_BOOL* Cancel,
                                 DWORD flags, BSTR url_context, BSTR url) {}

  STDMETHOD_(void, OnDocumentCompleteInternal)(IDispatch* dispatch,
                                               VARIANT* url);

  STDMETHOD_(void, OnDocumentComplete)(IDispatch* dispatch,
                                       VARIANT* url) {}
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
  virtual void OnMessage(const wchar_t* message) {}

  IWebBrowser2* web_browser2() {
    return web_browser2_.get();
  }

  HRESULT SetWebBrowser(IWebBrowser2* web_browser2);

 protected:
  // IChromeFrame callbacks
  HRESULT OnLoadInternal(const VARIANT* param);
  HRESULT OnLoadErrorInternal(const VARIANT* param);
  HRESULT OnMessageInternal(const VARIANT* param);

  void ConnectToChromeFrame();
  void DisconnectFromChromeFrame();
  HWND GetAttachedChromeRendererWindow();

 public:
  ScopedComPtr<IWebBrowser2> web_browser2_;
  ScopedComPtr<IChromeFrame> chrome_frame_;
  DispCallback<WebBrowserEventSink> onmessage_;
  DispCallback<WebBrowserEventSink> onloaderror_;
  DispCallback<WebBrowserEventSink> onload_;

 protected:
  static _ATL_FUNC_INFO kBeforeNavigate2Info;
  static _ATL_FUNC_INFO kNavigateComplete2Info;
  static _ATL_FUNC_INFO kNavigateErrorInfo;
  static _ATL_FUNC_INFO kNewWindow3Info;
  static _ATL_FUNC_INFO kVoidMethodInfo;
  static _ATL_FUNC_INFO kDocumentCompleteInfo;
};

}  // namespace chrome_frame_test

#endif  // CHROME_FRAME_TEST_CHROME_FRAME_TEST_UTILS_H_
