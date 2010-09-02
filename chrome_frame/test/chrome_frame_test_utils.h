// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_CHROME_FRAME_TEST_UTILS_H_
#define CHROME_FRAME_TEST_CHROME_FRAME_TEST_UTILS_H_

#include <atlbase.h>
#include <atlcom.h>
#include <exdisp.h>
#include <exdispid.h>
#include <mshtml.h>
#include <shlguid.h>
#include <shobjidl.h>
#include <windows.h>

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/scoped_comptr_win.h"

#include "chrome_frame/test_utils.h"
#include "chrome_frame/test/simulate_input.h"
#include "chrome_frame/utils.h"

// Include without path to make GYP build see it.
#include "chrome_tab.h"  // NOLINT

// Needed for CreateFunctor.
#define GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
#include "testing/gmock_mutant.h"

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

}  // namespace chrome_frame_test

#endif  // CHROME_FRAME_TEST_CHROME_FRAME_TEST_UTILS_H_
