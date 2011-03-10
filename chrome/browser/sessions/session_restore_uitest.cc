// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/test/test_timeouts.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/defaults.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "net/test/test_server.h"

namespace {

class SessionRestoreUITest : public UITest {
 protected:
  SessionRestoreUITest() : UITest() {
    FilePath path_prefix = test_data_directory_.AppendASCII("session_history");

    url1_ = net::FilePathToFileURL(path_prefix.AppendASCII("bot1.html"));
    url2_ = net::FilePathToFileURL(path_prefix.AppendASCII("bot2.html"));
    url3_ = net::FilePathToFileURL(path_prefix.AppendASCII("bot3.html"));
  }

  virtual void QuitBrowserAndRestore(int expected_tab_count) {
#if defined(OS_MACOSX)
    set_shutdown_type(ProxyLauncher::USER_QUIT);
#endif
    UITest::TearDown();

    clear_profile_ = false;

    launch_arguments_.AppendSwitchASCII(switches::kRestoreLastSession,
                                        base::IntToString(expected_tab_count));
    UITest::SetUp();
  }

  void CloseWindow(int window_index, int initial_count) {
    scoped_refptr<BrowserProxy> browser_proxy(
        automation()->GetBrowserWindow(window_index));
    ASSERT_TRUE(browser_proxy.get());
    ASSERT_TRUE(browser_proxy->RunCommand(IDC_CLOSE_WINDOW));
    int window_count;
    ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
    ASSERT_EQ(initial_count - 1, window_count);
  }

  void AssertOneWindowWithOneTab() {
    int window_count;
    ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
    ASSERT_EQ(1, window_count);
    GURL url;
    AssertWindowHasOneTab(0, &url);
  }

  void AssertWindowHasOneTab(int window_index, GURL* url) {
    scoped_refptr<BrowserProxy> browser_proxy(
        automation()->GetBrowserWindow(window_index));
    ASSERT_TRUE(browser_proxy.get());

    int tab_count;
    ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
    ASSERT_EQ(1, tab_count);

    int active_tab_index;
    ASSERT_TRUE(browser_proxy->GetActiveTabIndex(&active_tab_index));
    ASSERT_EQ(0, active_tab_index);

    scoped_refptr<TabProxy> tab_proxy(browser_proxy->GetActiveTab());
    ASSERT_TRUE(tab_proxy.get());
    ASSERT_TRUE(tab_proxy->WaitForTabToBeRestored(
        TestTimeouts::action_max_timeout_ms()));

    ASSERT_TRUE(tab_proxy->GetCurrentURL(url));
  }

  GURL url1_;
  GURL url2_;
  GURL url3_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionRestoreUITest);
};

TEST_F(SessionRestoreUITest, Basic) {
  NavigateToURL(url1_);
  NavigateToURL(url2_);

  QuitBrowserAndRestore(1);

  // NOTE: Don't use GetActiveWindow here, when run with the screen locked
  // active windows returns NULL.
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_EQ(1, window_count);
  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy.get());
  scoped_refptr<TabProxy> tab_proxy(browser_proxy->GetTab(0));
  ASSERT_TRUE(tab_proxy->WaitForTabToBeRestored(
      TestTimeouts::action_max_timeout_ms()));

  ASSERT_EQ(url2_, GetActiveTabURL());
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab_proxy->GoBack());
  ASSERT_EQ(url1_, GetActiveTabURL());
}

TEST_F(SessionRestoreUITest, RestoresForwardAndBackwardNavs) {
  NavigateToURL(url1_);
  NavigateToURL(url2_);
  NavigateToURL(url3_);

  scoped_refptr<TabProxy> active_tab(GetActiveTab());
  ASSERT_TRUE(active_tab.get());
  ASSERT_TRUE(active_tab->GoBack());

  QuitBrowserAndRestore(1);

  // NOTE: Don't use GetActiveWindow here, when run with the screen locked
  // active windows returns NULL.
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_EQ(1, window_count);
  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy.get());
  scoped_refptr<TabProxy> tab_proxy(browser_proxy->GetTab(0));
  ASSERT_TRUE(tab_proxy->WaitForTabToBeRestored(
      TestTimeouts::action_max_timeout_ms()));

  ASSERT_TRUE(GetActiveTabURL() == url2_);
  ASSERT_TRUE(tab_proxy->GoForward());
  ASSERT_TRUE(GetActiveTabURL() == url3_);
  ASSERT_TRUE(tab_proxy->GoBack());
  ASSERT_TRUE(GetActiveTabURL() == url2_);
  ASSERT_TRUE(tab_proxy->GoBack());
  ASSERT_TRUE(GetActiveTabURL() == url1_);
}

// Tests that the SiteInstances used for entries in a restored tab's history
// are given appropriate max page IDs, so that going back to a restored
// cross-site page and then forward again works.  (Bug 1204135)
TEST_F(SessionRestoreUITest, RestoresCrossSiteForwardAndBackwardNavs) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP,
                              FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(test_server.Start());

  GURL cross_site_url(test_server.GetURL("files/title2.html"));

  // Visit URLs on different sites.
  NavigateToURL(url1_);
  NavigateToURL(cross_site_url);
  NavigateToURL(url2_);

  scoped_refptr<TabProxy> active_tab(GetActiveTab());
  ASSERT_TRUE(active_tab.get());
  ASSERT_TRUE(active_tab->GoBack());

  QuitBrowserAndRestore(1);

  // NOTE: Don't use GetActiveWindow here, when run with the screen locked
  // active windows returns NULL.
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_EQ(1, window_count);
  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy.get());
  int tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  ASSERT_EQ(1, tab_count);
  scoped_refptr<TabProxy> tab_proxy(browser_proxy->GetTab(0));
  ASSERT_TRUE(tab_proxy.get());
  ASSERT_TRUE(tab_proxy->WaitForTabToBeRestored(
      TestTimeouts::action_max_timeout_ms()));

  // Check that back and forward work as expected.
  GURL url;
  ASSERT_TRUE(tab_proxy->GetCurrentURL(&url));
  ASSERT_EQ(cross_site_url, url);

  ASSERT_TRUE(tab_proxy->GoBack());
  ASSERT_TRUE(tab_proxy->GetCurrentURL(&url));
  ASSERT_EQ(url1_, url);

  ASSERT_TRUE(tab_proxy->GoForward());
  ASSERT_TRUE(tab_proxy->GetCurrentURL(&url));
  ASSERT_EQ(cross_site_url, url);

  ASSERT_TRUE(tab_proxy->GoForward());
  ASSERT_TRUE(tab_proxy->GetCurrentURL(&url));
  ASSERT_EQ(url2_, url);
}

TEST_F(SessionRestoreUITest, TwoTabsSecondSelected) {
  NavigateToURL(url1_);

  // NOTE: Don't use GetActiveWindow here, when run with the screen locked
  // active windows returns NULL.
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_EQ(1, window_count);
  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy.get());

  ASSERT_TRUE(browser_proxy->AppendTab(url2_));

  QuitBrowserAndRestore(2);
  browser_proxy = NULL;

  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_EQ(1, window_count);
  browser_proxy = automation()->GetBrowserWindow(0);

  int tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  ASSERT_EQ(2, tab_count);

  int active_tab_index;
  ASSERT_TRUE(browser_proxy->GetActiveTabIndex(&active_tab_index));
  ASSERT_EQ(1, active_tab_index);

  scoped_refptr<TabProxy> tab_proxy(browser_proxy->GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());
  ASSERT_TRUE(tab_proxy->WaitForTabToBeRestored(
      TestTimeouts::action_max_timeout_ms()));

  ASSERT_EQ(url2_, GetActiveTabURL());

  ASSERT_TRUE(browser_proxy->ActivateTab(0));
  tab_proxy = browser_proxy->GetActiveTab();
  ASSERT_TRUE(tab_proxy.get());
  ASSERT_TRUE(tab_proxy->WaitForTabToBeRestored(
      TestTimeouts::action_max_timeout_ms()));

  ASSERT_EQ(url1_, GetActiveTabURL());
}

// Creates two tabs, closes one, quits and makes sure only one tab is restored.
TEST_F(SessionRestoreUITest, ClosedTabStaysClosed) {
  NavigateToURL(url1_);

  // NOTE: Don't use GetActiveWindow here, when run with the screen locked
  // active windows returns NULL.
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_EQ(1, window_count);
  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy.get());
  scoped_refptr<TabProxy> tab_proxy(browser_proxy->GetTab(0));
  ASSERT_TRUE(tab_proxy.get());

  ASSERT_TRUE(browser_proxy->AppendTab(url2_));

  scoped_refptr<TabProxy> active_tab(browser_proxy->GetActiveTab());
  ASSERT_TRUE(active_tab.get());
  ASSERT_TRUE(active_tab->Close(true));

  QuitBrowserAndRestore(1);
  browser_proxy = NULL;
  tab_proxy = NULL;

  AssertOneWindowWithOneTab();

  ASSERT_EQ(url1_, GetActiveTabURL());
}

// Creates a tabbed browser and popup and makes sure we restore both.
TEST_F(SessionRestoreUITest, NormalAndPopup) {
  if (!browser_defaults::kRestorePopups)
    return;  // Test only applicable if restoring popups.

  NavigateToURL(url1_);

  // Make sure we have one window.
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_EQ(1, window_count);

  // Open a popup.
  ASSERT_TRUE(automation()->OpenNewBrowserWindow(Browser::TYPE_POPUP,
                                                 true));
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_EQ(2, window_count);

  scoped_refptr<BrowserProxy> popup(automation()->GetBrowserWindow(1));
  ASSERT_TRUE(popup.get());

  scoped_refptr<TabProxy> tab(popup->GetTab(0));
  ASSERT_TRUE(tab.get());

  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab->NavigateToURL(url1_));

  // Simulate an exit by shuting down the session service. If we don't do this
  // the first window close is treated as though the user closed the window
  // and won't be restored.
  ASSERT_TRUE(popup->ShutdownSessionService());

  tab = NULL;
  popup = NULL;

  // Restart and make sure we have only one window with one tab and the url
  // is url1_.
  QuitBrowserAndRestore(1);

  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_EQ(2, window_count);

  scoped_refptr<BrowserProxy> browser_proxy1(
      automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy1.get());

  scoped_refptr<BrowserProxy> browser_proxy2(
      automation()->GetBrowserWindow(1));
  ASSERT_TRUE(browser_proxy2.get());

  Browser::Type type1, type2;
  ASSERT_TRUE(browser_proxy1->GetType(&type1));
  ASSERT_TRUE(browser_proxy2->GetType(&type2));

  // The order of whether the normal window or popup is first depends upon
  // activation order, which is not necessarily consistant across runs.
  if (type1 == Browser::TYPE_NORMAL) {
    EXPECT_EQ(type2, Browser::TYPE_POPUP);
  } else {
    EXPECT_EQ(type1, Browser::TYPE_POPUP);
    EXPECT_EQ(type2, Browser::TYPE_NORMAL);
  }
}

#if !defined(OS_MACOSX)
// These tests don't apply to the Mac version; see
// LaunchAnotherBrowserBlockUntilClosed for details.

// Creates a browser, goes incognito, closes browser, launches and make sure
// we don't restore.
//
TEST_F(SessionRestoreUITest, DontRestoreWhileIncognito) {
  NavigateToURL(url1_);

  // Make sure we have one window.
  int initial_window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&initial_window_count));
  ASSERT_EQ(1, initial_window_count);

  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy.get());

  // Create an incognito window.
  ASSERT_TRUE(browser_proxy->RunCommand(IDC_NEW_INCOGNITO_WINDOW));
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_EQ(2, window_count);

  // Close the first window.
  CloseWindow(0, 2);
  browser_proxy = NULL;

  // Launch the browser again. Note, this doesn't spawn a new process, instead
  // it attaches to the current process.
  include_testing_id_ = false;
  clear_profile_ = false;
  launch_arguments_.AppendSwitch(switches::kRestoreLastSession);
  LaunchAnotherBrowserBlockUntilClosed(launch_arguments_);

  // A new window should appear;
  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(2));

  // And it shouldn't have url1_ in it.
  browser_proxy = automation()->GetBrowserWindow(1);
  ASSERT_TRUE(browser_proxy.get());
  scoped_refptr<TabProxy> tab_proxy(browser_proxy->GetTab(0));
  ASSERT_TRUE(tab_proxy.get());
  ASSERT_TRUE(tab_proxy->WaitForTabToBeRestored(
      TestTimeouts::action_max_timeout_ms()));
  GURL url;
  ASSERT_TRUE(tab_proxy->GetCurrentURL(&url));
  ASSERT_TRUE(url != url1_);
}

// Launches an app window, closes tabbed browser, launches and makes sure
// we restore the tabbed browser url.
// Flaky: http://crbug.com/29110
TEST_F(SessionRestoreUITest,
       FLAKY_RestoreAfterClosingTabbedBrowserWithAppAndLaunching) {
  NavigateToURL(url1_);

  // Launch an app.

  bool include_testing_id_orig = include_testing_id_;
  include_testing_id_ = false;
  clear_profile_ = false;
  CommandLine app_launch_arguments = launch_arguments_;
  app_launch_arguments.AppendSwitchASCII(switches::kApp, url2_.spec());
  LaunchAnotherBrowserBlockUntilClosed(app_launch_arguments);
  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(2));

  // Close the first window. The only window left is the App window.
  CloseWindow(0, 2);

  // Restore the session, which should bring back the first window with url1_.
  // First restore the settings so we can connect to the browser.
  include_testing_id_ = include_testing_id_orig;
  // Restore the session with 1 tab.
  QuitBrowserAndRestore(1);

  AssertOneWindowWithOneTab();

  ASSERT_EQ(url1_, GetActiveTabURL());
}

#endif  // !OS_MACOSX

// Creates two windows, closes one, restores, make sure only one window open.
TEST_F(SessionRestoreUITest, TwoWindowsCloseOneRestoreOnlyOne) {
  NavigateToURL(url1_);

  // Make sure we have one window.
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_EQ(1, window_count);

  // Open a second window.
  ASSERT_TRUE(automation()->OpenNewBrowserWindow(Browser::TYPE_NORMAL,
                                                 true));
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_EQ(2, window_count);

  // Close it.
  CloseWindow(1, 2);

  // Restart and make sure we have only one window with one tab and the url
  // is url1_.
  QuitBrowserAndRestore(1);

  AssertOneWindowWithOneTab();

  ASSERT_EQ(url1_, GetActiveTabURL());
}

// Make sure after a restore the number of processes matches that of the number
// of processes running before the restore. This creates a new tab so that
// we should have two new tabs running.  (This test will pass in both
// process-per-site and process-per-site-instance, because we treat the new tab
// as a special case in process-per-site-instance so that it only ever uses one
// process.)
//
// Flaky:  http://http://code.google.com/p/chromium/issues/detail?id=52022
// Unfortunately, the fix at http://http://codereview.chromium.org/6546078
// breaks NTP background image refreshing, so ThemeSource had to revert to
// replacing the existing data source.
TEST_F(SessionRestoreUITest, FLAKY_ShareProcessesOnRestore) {
  if (ProxyLauncher::in_process_renderer()) {
    // No point in running this test in single process mode.
    return;
  }

  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy.get() != NULL);
  int tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));

  // Create two new tabs.
  ASSERT_TRUE(browser_proxy->RunCommand(IDC_NEW_TAB));
  ASSERT_TRUE(browser_proxy->RunCommand(IDC_NEW_TAB));
  int new_tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&new_tab_count));
  ASSERT_EQ(tab_count + 2, new_tab_count);

  int expected_process_count = 0;
  ASSERT_TRUE(GetBrowserProcessCount(&expected_process_count));
  int expected_tab_count = new_tab_count;

  // Restart.
  browser_proxy = NULL;
  QuitBrowserAndRestore(3);

  // Wait for each tab to finish being restored, then make sure the process
  // count matches.
  browser_proxy = automation()->GetBrowserWindow(0);
  ASSERT_TRUE(browser_proxy.get() != NULL);
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  ASSERT_EQ(expected_tab_count, tab_count);

  for (int i = 0; i < expected_tab_count; ++i) {
    scoped_refptr<TabProxy> tab_proxy(browser_proxy->GetTab(i));
    ASSERT_TRUE(tab_proxy.get() != NULL);
    ASSERT_TRUE(tab_proxy->WaitForTabToBeRestored(
                    TestTimeouts::action_max_timeout_ms()));
  }

  int process_count = 0;
  ASSERT_TRUE(GetBrowserProcessCount(&process_count));
  ASSERT_EQ(expected_process_count, process_count);
}

}  // namespace
