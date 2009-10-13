// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/native_widget_types.h"
#include "base/file_path.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/platform_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_unittest.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

// Delay to let the browser shut down before trying more brutal methods.
static const int kWaitForTerminateMsec = 30000;

class BrowserTest : public UITest {

 protected:
  void TerminateBrowser() {
#if defined(OS_WIN)
    scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
    ASSERT_TRUE(browser->TerminateSession());
#elif defined(OS_POSIX)
    // There's nothing to do here if the browser is not running.
    if (IsBrowserRunning()) {
      automation()->SetFilteredInet(false);

      int window_count = 0;
      EXPECT_TRUE(automation()->GetBrowserWindowCount(&window_count));

      // Now, drop the automation IPC channel so that the automation provider in
      // the browser notices and drops its reference to the browser process.
      automation()->Disconnect();

      EXPECT_EQ(kill(process_, SIGTERM), 0);

      // Wait for the browser process to quit. It should have quit when it got
      // SIGTERM.
      int timeout = kWaitForTerminateMsec;
#ifdef WAIT_FOR_DEBUGGER_ON_OPEN
      timeout = 500000;
#endif
      if (!base::WaitForSingleProcess(process_, timeout)) {
        // We need to force the browser to quit because it didn't quit fast
        // enough. Take no chance and kill every chrome processes.
        CleanupAppProcesses();
      }

      // Don't forget to close the handle
      base::CloseProcessHandle(process_);
      process_ = NULL;
    }
#endif  // OS_POSIX
  }
};

class VisibleBrowserTest : public UITest {
 protected:
  VisibleBrowserTest() : UITest() {
    show_window_ = true;
  }
};

#if defined(OS_WIN)
// The browser should quit quickly if it receives a WM_ENDSESSION message.
TEST_F(BrowserTest, WindowsSessionEnd) {
#elif defined(OS_POSIX)
// The browser should quit gracefully and quickly if it receives a SIGTERM.
TEST_F(BrowserTest, PosixSessionEnd) {
#endif
#if defined(OS_WIN) || defined(OS_POSIX)
  FilePath test_file(test_data_directory_);
  test_file = test_file.AppendASCII("title1.html");

  NavigateToURL(net::FilePathToFileURL(test_file));
  PlatformThread::Sleep(action_timeout_ms());

  TerminateBrowser();

  PlatformThread::Sleep(action_timeout_ms());
  ASSERT_FALSE(IsBrowserRunning());

  // Make sure the UMA metrics say we didn't crash.
  scoped_ptr<DictionaryValue> local_prefs(GetLocalState());
  bool exited_cleanly;
  ASSERT_TRUE(local_prefs.get());
  ASSERT_TRUE(local_prefs->GetBoolean(prefs::kStabilityExitedCleanly,
                                      &exited_cleanly));
  ASSERT_TRUE(exited_cleanly);

  // And that session end was successful.
  bool session_end_completed;
  ASSERT_TRUE(local_prefs->GetBoolean(prefs::kStabilitySessionEndCompleted,
                                      &session_end_completed));
  ASSERT_TRUE(session_end_completed);

  // Make sure session restore says we didn't crash.
  scoped_ptr<DictionaryValue> profile_prefs(GetDefaultProfilePreferences());
  ASSERT_TRUE(profile_prefs.get());
  ASSERT_TRUE(profile_prefs->GetBoolean(prefs::kSessionExitedCleanly,
                                        &exited_cleanly));
  ASSERT_TRUE(exited_cleanly);
}
#endif  // OS_WIN || OS_POSIX

// Test that scripts can fork a new renderer process for a tab in a particular
// case (which matches following a link in Gmail).  The script must open a new
// tab, set its window.opener to null, and redirect it to a cross-site URL.
// (Bug 1115708)
// This test can only run if V8 is in use, and not KJS, because KJS will not
// set window.opener to null properly.
#ifdef CHROME_V8
TEST_F(BrowserTest, NullOpenerRedirectForksProcess) {
  // This test only works in multi-process mode
  if (in_process_renderer())
    return;

  const wchar_t kDocRoot[] = L"chrome/test/data";
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  FilePath test_file(test_data_directory_);
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  scoped_refptr<TabProxy> tab(window->GetActiveTab());
  ASSERT_TRUE(tab.get());

  // Start with a file:// url
  test_file = test_file.AppendASCII("title2.html");
  tab->NavigateToURL(net::FilePathToFileURL(test_file));
  int orig_tab_count = -1;
  ASSERT_TRUE(window->GetTabCount(&orig_tab_count));
  int orig_process_count = GetBrowserProcessCount();
  ASSERT_GE(orig_process_count, 1);

  // Use JavaScript URL to "fork" a new tab, just like Gmail.  (Open tab to a
  // blank page, set its opener to null, and redirect it cross-site.)
  std::wstring url_prefix(L"javascript:(function(){w=window.open();");
  GURL fork_url(url_prefix +
      L"w.opener=null;w.document.location=\"http://localhost:1337\";})()");

  // Make sure that a new tab has been created and that we have a new renderer
  // process for it.
  ASSERT_TRUE(tab->NavigateToURLAsync(fork_url));
  PlatformThread::Sleep(action_timeout_ms());
  ASSERT_EQ(orig_process_count + 1, GetBrowserProcessCount());
  int new_tab_count = -1;
  ASSERT_TRUE(window->GetTabCount(&new_tab_count));
  ASSERT_EQ(orig_tab_count + 1, new_tab_count);
}
#endif

#if !defined(OS_LINUX)
// TODO(port): This passes on linux locally, but fails on the try bot.
// Tests that non-Gmail-like script redirects (i.e., non-null window.opener) or
// a same-page-redirect) will not fork a new process.
TEST_F(BrowserTest, OtherRedirectsDontForkProcess) {
  // This test only works in multi-process mode
  if (in_process_renderer())
    return;

  const wchar_t kDocRoot[] = L"chrome/test/data";
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  FilePath test_file(test_data_directory_);
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  scoped_refptr<TabProxy> tab(window->GetActiveTab());
  ASSERT_TRUE(tab.get());

  // Start with a file:// url
  test_file = test_file.AppendASCII("title2.html");
  tab->NavigateToURL(net::FilePathToFileURL(test_file));
  int orig_tab_count = -1;
  ASSERT_TRUE(window->GetTabCount(&orig_tab_count));
  int orig_process_count = GetBrowserProcessCount();
  ASSERT_GE(orig_process_count, 1);

  // Use JavaScript URL to almost fork a new tab, but not quite.  (Leave the
  // opener non-null.)  Should not fork a process.
  std::string url_prefix("javascript:(function(){w=window.open();");
  GURL dont_fork_url(url_prefix +
      "w.document.location=\"http://localhost:1337\";})()");

  // Make sure that a new tab but not new process has been created.
  ASSERT_TRUE(tab->NavigateToURLAsync(dont_fork_url));
  PlatformThread::Sleep(action_timeout_ms());
  ASSERT_EQ(orig_process_count, GetBrowserProcessCount());
  int new_tab_count = -1;
  ASSERT_TRUE(window->GetTabCount(&new_tab_count));
  ASSERT_EQ(orig_tab_count + 1, new_tab_count);

  // Same thing if the current tab tries to redirect itself.
  GURL dont_fork_url2(url_prefix +
      "document.location=\"http://localhost:1337\";})()");

  // Make sure that no new process has been created.
  ASSERT_TRUE(tab->NavigateToURLAsync(dont_fork_url2));
  PlatformThread::Sleep(action_timeout_ms());
  ASSERT_EQ(orig_process_count, GetBrowserProcessCount());
}
#endif

#if defined(OS_WIN)
// TODO(estade): need to port GetActiveTabTitle().
TEST_F(VisibleBrowserTest, WindowOpenClose) {
  FilePath test_file(test_data_directory_);
  test_file = test_file.AppendASCII("window.close.html");

  NavigateToURL(net::FilePathToFileURL(test_file));

  int i;
  for (i = 0; i < 10; ++i) {
    PlatformThread::Sleep(action_max_timeout_ms() / 10);
    std::wstring title = GetActiveTabTitle();
    if (title == L"PASSED") {
      // Success, bail out.
      break;
    }
  }

  if (i == 10)
    FAIL() << "failed to get error page title";
}
#endif

class ShowModalDialogTest : public UITest {
 public:
  ShowModalDialogTest() {
    launch_arguments_.AppendSwitch(switches::kDisablePopupBlocking);
  }
};

TEST_F(ShowModalDialogTest, BasicTest) {
  FilePath test_file(test_data_directory_);
  test_file = test_file.AppendASCII("showmodaldialog.html");

  // This navigation should show a modal dialog that will be immediately
  // closed, but the fact that it was shown should be recorded.
  NavigateToURL(net::FilePathToFileURL(test_file));

  // At this point the modal dialog should not be showing.
  int window_count = 0;
  EXPECT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  EXPECT_EQ(1, window_count);

  // Verify that we set a mark on successful dialog show.
  scoped_refptr<BrowserProxy> browser = automation()->GetBrowserWindow(0);
  ASSERT_TRUE(browser.get());
  scoped_refptr<TabProxy> tab = browser->GetActiveTab();
  ASSERT_TRUE(tab.get());
  std::wstring title;
  ASSERT_TRUE(tab->GetTabTitle(&title));
  ASSERT_EQ(L"SUCCESS", title);
}

class SecurityTest : public UITest {
 protected:
  static const int kTestIntervalMs = 250;
  static const int kTestWaitTimeoutMs = 60 * 1000;
};

TEST_F(SecurityTest, DisallowFileUrlUniversalAccessTest) {
  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());

  FilePath test_file(test_data_directory_);
  test_file = test_file.AppendASCII("fileurl_universalaccess.html");

  GURL url = net::FilePathToFileURL(test_file);
  ASSERT_TRUE(tab->NavigateToURL(url));

  std::string value = WaitUntilCookieNonEmpty(tab.get(), url,
        "status", kTestIntervalMs, kTestWaitTimeoutMs);
  ASSERT_STREQ("Disallowed", value.c_str());
}

}  // namespace
