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
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/automation/automation_proxy.h"
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

// WindowOpenClose is flaky on ChromeOS and fails consistently on linux views.
// See http://crbug.com/85763.
#if defined (OS_CHROMEOS)
#define MAYBE_WindowOpenClose FLAKY_WindowOpenClose
#elif defined(OS_LINUX) && defined(TOOLKIT_VIEWS)
#define MAYBE_WindowOpenClose FAILS_WindowOpenClose
#else
#define MAYBE_WindowOpenClose WindowOpenClose
#endif

TEST_F(VisibleBrowserTest, MAYBE_WindowOpenClose) {
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
// TODO(estade): remove flaky label if prospective fix works.
TEST_F(ShowModalDialogTest, FLAKY_BasicTest) {
  FilePath test_file(test_data_directory_);
  test_file = test_file.AppendASCII("showmodaldialog.html");

  // This navigation should show a modal dialog that will be immediately
  // closed, but the fact that it was shown should be recorded.
  NavigateToURL(net::FilePathToFileURL(test_file));
  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(1));

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
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    FilePath tmp_profile = temp_dir_.path().AppendASCII("tmp_profile");
    tmp_profile = tmp_profile.Append(L"Test Chrome G�raldine");

    ASSERT_TRUE(file_util::CreateDirectory(tmp_profile));

    launch_arguments_.AppendSwitchPath(switches::kUserDataDir, tmp_profile);
  }

  bool LaunchAppWithProfile() {
    UITest::SetUp();
    return true;
  }

public:
  ScopedTempDir temp_dir_;
};

TEST_F(LaunchBrowserWithNonAsciiUserDatadir, TestNonAsciiUserDataDir) {
  ASSERT_TRUE(LaunchAppWithProfile());
  // Verify that the window is present.
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
}
#endif  // defined(OS_WIN)

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
  ASSERT_TRUE(automation()->OpenNewBrowserWindow(Browser::TYPE_TABBED, true));
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  EXPECT_EQ(1, window_count);
  // Set the shutdown type to 'SESSION_ENDING' since we are running in
  // background mode and neither closing all the windows nor quitting will
  // shut down the browser.
  set_shutdown_type(ProxyLauncher::SESSION_ENDING);
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
  ASSERT_TRUE(automation()->OpenNewBrowserWindow(Browser::TYPE_TABBED, true));
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  EXPECT_EQ(1, window_count);
}

}  // namespace

// This test needs to be placed outside the anonymouse namespace because we
// need to access private type of Browser.
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

  // Verify the browser is in application mode.
  bool is_application;
  ASSERT_TRUE(browser->IsApplication(&is_application));
  EXPECT_TRUE(is_application);
}
