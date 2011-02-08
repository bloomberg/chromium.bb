// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/test/test_file_util.h"
#include "base/test/test_timeouts.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "net/test/test_server.h"
#include "ui/gfx/native_widget_types.h"

namespace {

class BrowserTest : public UITest {
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
  FilePath test_file(test_data_directory_);
  test_file = test_file.AppendASCII("title1.html");

  NavigateToURL(net::FilePathToFileURL(test_file));
  TerminateBrowser();
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

// Test that scripts can fork a new renderer process for a tab in a particular
// case (which matches following a link in Gmail).  The script must open a new
// tab, set its window.opener to null, and redirect it to a cross-site URL.
// (Bug 1115708)
// This test can only run if V8 is in use, and not KJS, because KJS will not
// set window.opener to null properly.
#ifdef CHROME_V8
TEST_F(BrowserTest, NullOpenerRedirectForksProcess) {
  // This test only works in multi-process mode
  if (ProxyLauncher::in_process_renderer())
    return;

  net::TestServer test_server(net::TestServer::TYPE_HTTP,
                              FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(test_server.Start());

  FilePath test_file(test_data_directory_);
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  scoped_refptr<TabProxy> tab(window->GetActiveTab());
  ASSERT_TRUE(tab.get());

  // Start with a file:// url
  test_file = test_file.AppendASCII("title2.html");
  tab->NavigateToURL(net::FilePathToFileURL(test_file));
  int orig_tab_count = -1;
  ASSERT_TRUE(window->GetTabCount(&orig_tab_count));
  int orig_process_count = 0;
  ASSERT_TRUE(GetBrowserProcessCount(&orig_process_count));
  ASSERT_GE(orig_process_count, 1);

  // Use JavaScript URL to "fork" a new tab, just like Gmail.  (Open tab to a
  // blank page, set its opener to null, and redirect it cross-site.)
  std::wstring url_prefix(L"javascript:(function(){w=window.open();");
  GURL fork_url(url_prefix +
      L"w.opener=null;w.document.location=\"http://localhost:1337\";})()");

  // Make sure that a new tab has been created and that we have a new renderer
  // process for it.
  ASSERT_TRUE(tab->NavigateToURLAsync(fork_url));
  PlatformThread::Sleep(TestTimeouts::action_timeout_ms());
  int process_count = 0;
  ASSERT_TRUE(GetBrowserProcessCount(&process_count));
  ASSERT_EQ(orig_process_count + 1, process_count);
  int new_tab_count = -1;
  ASSERT_TRUE(window->GetTabCount(&new_tab_count));
  ASSERT_EQ(orig_tab_count + 1, new_tab_count);
}
#endif  // CHROME_V8

// This test fails on ChromeOS (it has never been known to work on it).
// Currently flaky on Windows - it has crashed a couple of times.
// http://crbug.com/32799
#if defined(OS_CHROMEOS)
#define MAYBE_OtherRedirectsDontForkProcess DISABLED_OtherRedirectsDontForkProcess
#else
#define MAYBE_OtherRedirectsDontForkProcess FLAKY_OtherRedirectsDontForkProcess
#endif

// Tests that non-Gmail-like script redirects (i.e., non-null window.opener) or
// a same-page-redirect) will not fork a new process.
TEST_F(BrowserTest, MAYBE_OtherRedirectsDontForkProcess) {
  // This test only works in multi-process mode
  if (ProxyLauncher::in_process_renderer())
    return;

  net::TestServer test_server(net::TestServer::TYPE_HTTP,
                              FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(test_server.Start());

  FilePath test_file(test_data_directory_);
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  scoped_refptr<TabProxy> tab(window->GetActiveTab());
  ASSERT_TRUE(tab.get());

  // Start with a file:// url
  test_file = test_file.AppendASCII("title2.html");
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
            tab->NavigateToURL(net::FilePathToFileURL(test_file)));
  int orig_tab_count = -1;
  ASSERT_TRUE(window->GetTabCount(&orig_tab_count));
  int orig_process_count = 0;
  ASSERT_TRUE(GetBrowserProcessCount(&orig_process_count));
  ASSERT_GE(orig_process_count, 1);

  // Use JavaScript URL to almost fork a new tab, but not quite.  (Leave the
  // opener non-null.)  Should not fork a process.
  std::string url_str = "javascript:(function(){w=window.open(); ";
  url_str += "w.document.location=\"";
  url_str += test_server.GetURL("").spec();
  url_str += "\";})()";
  GURL dont_fork_url(url_str);

  // Make sure that a new tab but not new process has been created.
  ASSERT_TRUE(tab->NavigateToURLAsync(dont_fork_url));
  base::PlatformThread::Sleep(TestTimeouts::action_timeout_ms());
  int process_count = 0;
  ASSERT_TRUE(GetBrowserProcessCount(&process_count));
  ASSERT_EQ(orig_process_count, process_count);
  int new_tab_count = -1;
  ASSERT_TRUE(window->GetTabCount(&new_tab_count));
  ASSERT_EQ(orig_tab_count + 1, new_tab_count);

  // Same thing if the current tab tries to redirect itself.
  url_str = "javascript:(function(){w=window.open(); ";
  url_str += "document.location=\"";
  url_str += test_server.GetURL("").spec();
  url_str += "\";})()";
  GURL dont_fork_url2(url_str);

  // Make sure that no new process has been created.
  ASSERT_TRUE(tab->NavigateToURLAsync(dont_fork_url2));
  base::PlatformThread::Sleep(TestTimeouts::action_timeout_ms());
  ASSERT_TRUE(GetBrowserProcessCount(&process_count));
  ASSERT_EQ(orig_process_count, process_count);
}

TEST_F(VisibleBrowserTest, WindowOpenClose) {
  FilePath test_file(test_data_directory_);
  test_file = test_file.AppendASCII("window.close.html");

  NavigateToURLBlockUntilNavigationsComplete(
      net::FilePathToFileURL(test_file), 2);
  EXPECT_EQ(L"Title Of Awesomeness", GetActiveTabTitle());
}

class ShowModalDialogTest : public UITest {
 public:
  ShowModalDialogTest() {
    launch_arguments_.AppendSwitch(switches::kDisablePopupBlocking);
  }
};

// Flakiness returned. Re-opened crbug.com/17806
TEST_F(ShowModalDialogTest, FLAKY_BasicTest) {
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
};

TEST_F(SecurityTest, DisallowFileUrlUniversalAccessTest) {
  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());

  FilePath test_file(test_data_directory_);
  test_file = test_file.AppendASCII("fileurl_universalaccess.html");

  GURL url = net::FilePathToFileURL(test_file);
  ASSERT_TRUE(tab->NavigateToURL(url));

  std::string value = WaitUntilCookieNonEmpty(tab.get(), url,
        "status", TestTimeouts::action_max_timeout_ms());
  ASSERT_STREQ("Disallowed", value.c_str());
}

#if !defined(OS_MACOSX)
class KioskModeTest : public UITest {
 public:
  KioskModeTest() {
    launch_arguments_.AppendSwitch(switches::kKioskMode);
  }
};

TEST_F(KioskModeTest, EnableKioskModeTest) {
  // Load a local file.
  FilePath test_file(test_data_directory_);
  test_file = test_file.AppendASCII("title1.html");

  // Verify that the window is present.
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  // Check if browser is in fullscreen mode.
  bool is_visible;
  ASSERT_TRUE(browser->IsFullscreen(&is_visible));
  EXPECT_TRUE(is_visible);
  ASSERT_TRUE(browser->IsFullscreenBubbleVisible(&is_visible));
  EXPECT_FALSE(is_visible);
}
#endif  // !defined(OS_MACOSX)

#if defined(OS_WIN)
// This test verifies that Chrome can be launched with a user-data-dir path
// which contains non ASCII characters.
class LaunchBrowserWithNonAsciiUserDatadir : public UITest {
public:
  void SetUp() {
    PathService::Get(base::DIR_TEMP, &tmp_profile_);
    tmp_profile_ = tmp_profile_.AppendASCII("tmp_profile");
    tmp_profile_ = tmp_profile_.Append(L"Test Chrome G�raldine");

    // Create a fresh, empty copy of this directory.
    file_util::Delete(tmp_profile_, true);
    file_util::CreateDirectory(tmp_profile_);

    launch_arguments_.AppendSwitchPath(switches::kUserDataDir, tmp_profile_);
  }

  bool LaunchAppWithProfile() {
    UITest::SetUp();
    return true;
  }

  void TearDown() {
    UITest::TearDown();
    EXPECT_TRUE(file_util::DieFileDie(tmp_profile_, true));
  }

public:
  FilePath tmp_profile_;
};

TEST_F(LaunchBrowserWithNonAsciiUserDatadir, TestNonAsciiUserDataDir) {
  ASSERT_TRUE(LaunchAppWithProfile());
  // Verify that the window is present.
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
}
#endif  // defined(OS_WIN)

class AppModeTest : public UITest {
 public:
  AppModeTest() {
    // Load a local file.
    FilePath test_file(test_data_directory_);
    test_file = test_file.AppendASCII("title1.html");
    GURL test_file_url(net::FilePathToFileURL(test_file));

    launch_arguments_.AppendSwitchASCII(switches::kApp, test_file_url.spec());
  }
};

TEST_F(AppModeTest, EnableAppModeTest) {
  // Test that an application browser window loads correctly.

  // Verify that the window is present.
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  // Verify the browser is an application.
  Browser::Type type;
  ASSERT_TRUE(browser->GetType(&type));
  EXPECT_EQ(Browser::TYPE_APP, type);
}

// Tests to ensure that the browser continues running in the background after
// the last window closes.
class RunInBackgroundTest : public UITest {
 public:
  RunInBackgroundTest() {
    launch_arguments_.AppendSwitch(switches::kKeepAliveForTest);
  }
};

TEST_F(RunInBackgroundTest, RunInBackgroundBasicTest) {
  // Close the browser window, then open a new one - the browser should keep
  // running.
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  EXPECT_EQ(1, window_count);
  ASSERT_TRUE(browser->RunCommand(IDC_CLOSE_WINDOW));
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  EXPECT_EQ(0, window_count);
  ASSERT_TRUE(IsBrowserRunning());
  ASSERT_TRUE(automation()->OpenNewBrowserWindow(Browser::TYPE_NORMAL, true));
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  EXPECT_EQ(1, window_count);
}

// Tests to ensure that the browser continues running in the background after
// the last window closes.
class NoStartupWindowTest : public UITest {
 public:
  NoStartupWindowTest() {
    launch_arguments_.AppendSwitch(switches::kNoStartupWindow);
    launch_arguments_.AppendSwitch(switches::kKeepAliveForTest);
  }
};

TEST_F(NoStartupWindowTest, NoStartupWindowBasicTest) {
  // No browser window should be started by default.
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  EXPECT_EQ(0, window_count);

  // Starting a browser window should work just fine.
  ASSERT_TRUE(IsBrowserRunning());
  ASSERT_TRUE(automation()->OpenNewBrowserWindow(Browser::TYPE_NORMAL, true));
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  EXPECT_EQ(1, window_count);
}

}  // namespace
