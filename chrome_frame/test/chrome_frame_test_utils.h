// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_CHROME_FRAME_TEST_UTILS_H_
#define CHROME_FRAME_TEST_CHROME_FRAME_TEST_UTILS_H_

#include <windows.h>

#include <atlbase.h>
#include <atlwin.h>

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/scoped_comptr_win.h"
#include "base/scoped_ptr.h"
#include "base/win/registry.h"

#include "chrome_frame/test_utils.h"
#include "chrome_frame/test/simulate_input.h"
#include "chrome_frame/utils.h"

// Include without path to make GYP build see it.
#include "chrome_tab.h"  // NOLINT

#include "gtest/gtest.h"

// Needed for CreateFunctor.
#define GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
#include "testing/gmock_mutant.h"

interface IWebBrowser2;

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

// This class implements the COM IMessageFilter interface and is used to detect
// hung outgoing COM calls which are typically to IE. If IE is hung for any
// reason the chrome frame tests should not hang indefinitely. This class
// basically cancels the outgoing call if the WM_TIMER which is set by the
// TimedMsgLoop object is posted to the message queue.
class HungCOMCallDetector
    : public CComObjectRootEx<CComMultiThreadModel>,
      public IMessageFilter,
      public CWindowImpl<HungCOMCallDetector> {
 public:
  HungCOMCallDetector()
      : is_hung_(false) {
    LOG(INFO) << __FUNCTION__;
  }

  ~HungCOMCallDetector() {
    LOG(INFO) << __FUNCTION__;
  }

  BEGIN_MSG_MAP(HungCOMCallDetector)
    MESSAGE_HANDLER(WM_TIMER, OnTimer)
  END_MSG_MAP()

  BEGIN_COM_MAP(HungCOMCallDetector)
    COM_INTERFACE_ENTRY(IMessageFilter)
  END_COM_MAP()

  static CComObject<HungCOMCallDetector>* Setup(int timeout_seconds) {
    CComObject<HungCOMCallDetector>* this_instance  = NULL;
    CComObject<HungCOMCallDetector>::CreateInstance(&this_instance);
    EXPECT_TRUE(this_instance != NULL);
    scoped_refptr<HungCOMCallDetector> ref_instance = this_instance;

    HRESULT hr = ref_instance->Initialize(timeout_seconds);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to Initialize Hung COM call detector. Error:"
                 << hr;
      return NULL;
    }
    return this_instance;
  }

  void TearDown() {
    ScopedComPtr<IMessageFilter> prev_filter;
    CoRegisterMessageFilter(prev_filter_.get(), prev_filter.Receive());
    DestroyWindow();
    m_hWnd = NULL;
  }

  STDMETHOD_(DWORD, HandleInComingCall)(DWORD call_type,
                                        HTASK caller,
                                        DWORD tick_count,
                                        LPINTERFACEINFO interface_info) {
    return SERVERCALL_ISHANDLED;
  }

  STDMETHOD_(DWORD, RetryRejectedCall)(HTASK callee,
                                       DWORD tick_count,
                                       DWORD reject_type) {
    return -1;
  }

  STDMETHOD_(DWORD, MessagePending)(HTASK callee,
                                    DWORD tick_count,
                                    DWORD pending_type) {
    MSG msg = {0};
    if (PeekMessage(&msg, m_hWnd, WM_TIMER, WM_TIMER, PM_NOREMOVE)) {
      is_hung_ = true;
      return PENDINGMSG_CANCELCALL;
    }
    return PENDINGMSG_WAITDEFPROCESS;
  }

  bool is_hung() const {
    return is_hung_;
  }

  LRESULT OnTimer(UINT msg, WPARAM wp, LPARAM lp, BOOL& handled) {  // NOLINT
    return 1;
  }

 private:
  HRESULT Initialize(int timeout_seconds) {
    // Create a window for the purpose of setting a windows timer for
    // detecting whether any outgoing COM call issued in this context
    // hangs.
    // We then register a COM message filter which basically looks for the
    // timer posted to this window and cancels the outgoing call.
    Create(HWND_MESSAGE);
    if (!IsWindow()) {
      LOG(ERROR) << "Failed to create window. Error:" << GetLastError();
      return E_FAIL;
    }

    HRESULT hr = CoRegisterMessageFilter(this, prev_filter_.Receive());
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to register message filter. Error:" << hr;
      return hr;
    }

    static const int kHungDetectTimerId = 0x0000baba;
    SetTimer(kHungDetectTimerId, 1000 * (timeout_seconds + 2), NULL);
    return S_OK;
  }

  // used to detect if outgoing COM calls hung.
  bool is_hung_;
  ScopedComPtr<IMessageFilter> prev_filter_;
};

// MessageLoopForUI wrapper that runs only for a limited time.
// We need a UI message loop in the main thread.
class TimedMsgLoop {
 public:
  TimedMsgLoop() : quit_loop_invoked_(false) {
  }

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

// Returns the folder for Selenium core.
FilePath GetSeleniumTestFolder();

// Returns the path portion of the url.
std::wstring GetPathFromUrl(const std::wstring& url);

// Returns the path and query portion of the url.
std::wstring GetPathAndQueryFromUrl(const std::wstring& url);

// Adds the CF meta tag to the html page. Returns true if successful.
bool AddCFMetaTag(std::string* html_data);

// Get text data from the clipboard.
std::wstring GetClipboardText();

// Puts the given text data on the clipboard. All previous items on the
// clipboard are removed.
void SetClipboardText(const std::wstring& text);

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
  ~CloseIeAtEndOfScope();
};

// Starts the Chrome crash service which enables us to gather crash dumps
// during test runs.
base::ProcessHandle StartCrashService();

class TempRegKeyOverride {
 public:
  static const wchar_t kTempTestKeyPath[];

  TempRegKeyOverride(HKEY override, const wchar_t* temp_name);
  ~TempRegKeyOverride();

  static void DeleteAllTempKeys();

 protected:
  HKEY override_;
  base::win::RegKey temp_key_;
  std::wstring temp_name_;
};

// Used in tests where we reference the registry and don't want to run into
// problems where existing registry settings might conflict with the
// expectations of the test.
class ScopedVirtualizeHklmAndHkcu {
 public:
  ScopedVirtualizeHklmAndHkcu();
  ~ScopedVirtualizeHklmAndHkcu();

 protected:
  scoped_ptr<TempRegKeyOverride> hklm_;
  scoped_ptr<TempRegKeyOverride> hkcu_;
};

// Attempts to kill all the processes on the current machine that were launched
// from the given executable name, ending them with the given exit code.  If
// filter is non-null, then only processes selected by the filter are killed.
// Returns true if all processes were able to be killed off, false if at least
// one couldn't be killed.
// NOTE: This function is based on the base\process_util.h helper function
// KillProcesses. Takes in the wait flag as a parameter.
bool KillProcesses(const std::wstring& executable_name, int exit_code,
                   bool wait);

// Returns the type of test bed, PER_USER or SYSTEM_LEVEL.
ScopedChromeFrameRegistrar::RegistrationType GetTestBedType();

}  // namespace chrome_frame_test

// TODO(tommi): This is a temporary workaround while we're getting our
// Singleton story straight.  Ideally each test should clear up any singletons
// it might have created, but test cases do not implicitly have their own
// AtExitManager, so we have this workaround method for tests that depend on
// "fresh" singletons.  The implementation is in chrome_frame_unittest_main.cc.
void DeleteAllSingletons();

#endif  // CHROME_FRAME_TEST_CHROME_FRAME_TEST_UTILS_H_
