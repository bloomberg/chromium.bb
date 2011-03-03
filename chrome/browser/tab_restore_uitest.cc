// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/test/test_timeouts.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "net/test/test_server.h"

// http://code.google.com/p/chromium/issues/detail?id=14774
#if (defined(OS_WIN) || defined(OS_CHROMEOS)) && !defined(NDEBUG)
#define MAYBE_BasicRestoreFromClosedWindow DISABLED_BasicRestoreFromClosedWindow
#else
#define MAYBE_BasicRestoreFromClosedWindow BasicRestoreFromClosedWindow
#endif

class TabRestoreUITest : public UITest {
 public:
  TabRestoreUITest() : UITest() {
    FilePath path_prefix(test_data_directory_);
    path_prefix = path_prefix.AppendASCII("session_history");
    url1_ = net::FilePathToFileURL(path_prefix.AppendASCII("bot1.html"));
    url2_ = net::FilePathToFileURL(path_prefix.AppendASCII("bot2.html"));
  }

 protected:
  // Uses the undo-close-tab accelerator to undo a close-tab or close-window
  // operation. The newly restored tab is expected to appear in the
  // window at index |expected_window_index|, at the |expected_tabstrip_index|,
  // and to be active. If |expected_window_index| is equal to the number of
  // current windows, the restored tab is expected to be created in a new
  // window (since the index is 0-based).
  void RestoreTab(int expected_window_index,
                  int expected_tabstrip_index) {
    int tab_count = 0;
    int window_count = 0;

    ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
    ASSERT_GT(window_count, 0);

    bool expect_new_window = (expected_window_index == window_count);
    scoped_refptr<BrowserProxy> browser_proxy;
    if (expect_new_window) {
      browser_proxy = automation()->GetBrowserWindow(0);
    } else {
      ASSERT_GT(window_count, expected_window_index);
      browser_proxy = automation()->GetBrowserWindow(expected_window_index);
    }
    ASSERT_TRUE(browser_proxy.get());
    ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
    ASSERT_GT(tab_count, 0);

    // Restore the tab.
    ASSERT_TRUE(browser_proxy->RunCommand(IDC_RESTORE_TAB));

    if (expect_new_window) {
      int new_window_count = 0;
      ASSERT_TRUE(automation()->GetBrowserWindowCount(&new_window_count));
      EXPECT_EQ(++window_count, new_window_count);
      browser_proxy = automation()->GetBrowserWindow(expected_window_index);
      ASSERT_TRUE(browser_proxy.get());
    } else {
      int new_tab_count = 0;
      ASSERT_TRUE(browser_proxy->GetTabCount(&new_tab_count));
      EXPECT_EQ(++tab_count, new_tab_count);
    }

    // Get a handle to the restored tab.
    ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
    ASSERT_GT(tab_count, expected_tabstrip_index);
    scoped_refptr<TabProxy> restored_tab_proxy(
        browser_proxy->GetTab(expected_tabstrip_index));
    ASSERT_TRUE(restored_tab_proxy.get());
    // Wait for the restored tab to finish loading.
    ASSERT_TRUE(restored_tab_proxy->WaitForTabToBeRestored(
        TestTimeouts::action_max_timeout_ms()));

    // Ensure that the tab and window are active.
    CheckActiveWindow(browser_proxy.get());
    EXPECT_EQ(expected_tabstrip_index,
        GetActiveTabIndex(expected_window_index));
  }

  // Adds tabs to the given browser, all navigated to url1_. Returns
  // the final number of tabs.
  int AddSomeTabs(BrowserProxy* browser, int how_many) {
    int starting_tab_count = -1;
    EXPECT_TRUE(browser->GetTabCount(&starting_tab_count));

    for (int i = 0; i < how_many; ++i) {
      EXPECT_TRUE(browser->AppendTab(url1_));
    }
    int tab_count;
    EXPECT_TRUE(browser->GetTabCount(&tab_count));
    EXPECT_EQ(starting_tab_count + how_many, tab_count);
    return tab_count;
  }

  // Ensure that the given browser occupies the currently active window.
  void CheckActiveWindow(const BrowserProxy* browser) {
    // This entire check is disabled because even the IsActive() call
    // sporadically fails to complete successfully. See http://crbug.com/10916.
    // TODO(pamg): Investigate and re-enable. Also find a way to have the
    // calling location reported in the gtest error, by inlining this again if
    // nothing else.
    return;

    bool is_active = false;
    scoped_refptr<WindowProxy> window_proxy(browser->GetWindow());
    ASSERT_TRUE(window_proxy.get());
    ASSERT_TRUE(window_proxy->IsActive(&is_active));
    // The check for is_active may fail if other apps are active while running
    // the tests, because Chromium won't be the foremost application at all.
    // So we'll have it log an error, but not report one through gtest, to
    // keep the test result deterministic and the buildbots happy.
    if (!is_active)
      LOG(ERROR) << "WARNING: is_active was false, expected true. (This may "
                    "be simply because Chromium isn't the front application.)";
  }

  GURL url1_;
  GURL url2_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabRestoreUITest);
};

// Close the end tab in the current window, then restore it. The tab should be
// in its original position, and active.
TEST_F(TabRestoreUITest, Basic) {
  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy.get());

  int starting_tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&starting_tab_count));
  int tab_count = AddSomeTabs(browser_proxy.get(), 1);

  int closed_tab_index = tab_count - 1;
  scoped_refptr<TabProxy> new_tab(browser_proxy->GetTab(closed_tab_index));
  ASSERT_TRUE(new_tab.get());
  // Make sure we're at url.
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, new_tab->NavigateToURL(url1_));
  // Close the tab.
  ASSERT_TRUE(new_tab->Close(true));
  new_tab = NULL;
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  EXPECT_EQ(starting_tab_count, tab_count);

  RestoreTab(0, closed_tab_index);

  // And make sure everything looks right.
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  EXPECT_EQ(starting_tab_count + 1, tab_count);
  EXPECT_EQ(closed_tab_index, GetActiveTabIndex());
  EXPECT_EQ(url1_, GetActiveTabURL());
}

// Close a tab not at the end of the current window, then restore it. The tab
// should be in its original position, and active.
TEST_F(TabRestoreUITest, MiddleTab) {
  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy.get());

  int starting_tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&starting_tab_count));
  int tab_count = AddSomeTabs(browser_proxy.get(), 3);

  // Close one in the middle
  int closed_tab_index = starting_tab_count + 1;
  scoped_refptr<TabProxy> new_tab(browser_proxy->GetTab(closed_tab_index));
  ASSERT_TRUE(new_tab.get());
  // Make sure we're at url.
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, new_tab->NavigateToURL(url1_));
  // Close the tab.
  ASSERT_TRUE(new_tab->Close(true));
  new_tab = NULL;
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  EXPECT_EQ(starting_tab_count + 2, tab_count);

  RestoreTab(0, closed_tab_index);

  // And make sure everything looks right.
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  EXPECT_EQ(starting_tab_count + 3, tab_count);
  EXPECT_EQ(closed_tab_index, GetActiveTabIndex());
  EXPECT_EQ(url1_, GetActiveTabURL());
}

// Close a tab, switch windows, then restore the tab. The tab should be in its
// original window and position, and active.
TEST_F(TabRestoreUITest, RestoreToDifferentWindow) {
  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy.get());

  // This call is virtually guaranteed to pass, assuming that Chromium is the
  // active application, which will establish a baseline for later calls to
  // CheckActiveWindow(). See comments in that function.
  CheckActiveWindow(browser_proxy.get());

  int starting_tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&starting_tab_count));
  int tab_count = AddSomeTabs(browser_proxy.get(), 3);

  // Close one in the middle
  int closed_tab_index = starting_tab_count + 1;
  scoped_refptr<TabProxy> new_tab(browser_proxy->GetTab(closed_tab_index));
  ASSERT_TRUE(new_tab.get());
  // Make sure we're at url.
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, new_tab->NavigateToURL(url1_));
  // Close the tab.
  ASSERT_TRUE(new_tab->Close(true));
  new_tab = NULL;
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  EXPECT_EQ(starting_tab_count + 2, tab_count);

  // Create a new browser.
  ASSERT_TRUE(automation()->OpenNewBrowserWindow(Browser::TYPE_NORMAL, false));
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  EXPECT_EQ(2, window_count);

  CheckActiveWindow(automation()->GetBrowserWindow(1));

  // Restore tab into original browser.
  RestoreTab(0, closed_tab_index);

  // And make sure everything looks right.
  CheckActiveWindow(browser_proxy.get());
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  EXPECT_EQ(starting_tab_count + 3, tab_count);
  EXPECT_EQ(closed_tab_index, GetActiveTabIndex(0));
  EXPECT_EQ(url1_, GetActiveTabURL(0));
}

// Close a tab, open a new window, close the first window, then restore the
// tab. It should be in a new window.
TEST_F(TabRestoreUITest, MAYBE_BasicRestoreFromClosedWindow) {
  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy.get());
  CheckActiveWindow(browser_proxy.get());

  int tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));

  // Close tabs until we only have one open.
  while (tab_count > 1) {
    scoped_refptr<TabProxy> tab_to_close(browser_proxy->GetTab(0));
    ASSERT_TRUE(tab_to_close.get());
    ASSERT_TRUE(tab_to_close->Close(true));
    ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  }

  // Navigate to url1 then url2.
  scoped_refptr<TabProxy> tab_proxy(browser_proxy->GetTab(0));
  ASSERT_TRUE(tab_proxy.get());
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab_proxy->NavigateToURL(url1_));
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab_proxy->NavigateToURL(url2_));

  // Create a new browser.
  ASSERT_TRUE(automation()->OpenNewBrowserWindow(Browser::TYPE_NORMAL, false));
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  EXPECT_EQ(2, window_count);
  CheckActiveWindow(automation()->GetBrowserWindow(1));

  // Close the final tab in the first browser.
  EXPECT_TRUE(tab_proxy->Close(true));
  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(1));

  // Tab and browser are no longer valid.
  tab_proxy = NULL;
  browser_proxy = NULL;

  RestoreTab(1, 0);

  // Tab should be in a new window.
  browser_proxy = automation()->GetBrowserWindow(1);
  ASSERT_TRUE(browser_proxy.get());
  CheckActiveWindow(browser_proxy.get());
  tab_proxy = browser_proxy->GetActiveTab();
  ASSERT_TRUE(tab_proxy.get());
  // And make sure the URLs matches.
  EXPECT_EQ(url2_, GetActiveTabURL(1));
  EXPECT_TRUE(tab_proxy->GoBack());
  EXPECT_EQ(url1_, GetActiveTabURL(1));
}

// Restore a tab then make sure it doesn't restore again.
TEST_F(TabRestoreUITest, DontLoadRestoredTab) {
  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy.get());
  CheckActiveWindow(browser_proxy.get());

  // Add two tabs
  int starting_tab_count = 0;
  ASSERT_TRUE(browser_proxy->GetTabCount(&starting_tab_count));
  AddSomeTabs(browser_proxy.get(), 2);
  int current_tab_count = 0;
  ASSERT_TRUE(browser_proxy->GetTabCount(&current_tab_count));
  ASSERT_EQ(current_tab_count, starting_tab_count + 2);

  // Close one of them.
  scoped_refptr<TabProxy> tab_to_close(browser_proxy->GetTab(0));
  ASSERT_TRUE(tab_to_close.get());
  ASSERT_TRUE(tab_to_close->Close(true));
  ASSERT_TRUE(browser_proxy->GetTabCount(&current_tab_count));
  ASSERT_EQ(current_tab_count, starting_tab_count + 1);

  // Restore it.
  RestoreTab(0, 0);
  ASSERT_TRUE(browser_proxy->GetTabCount(&current_tab_count));
  ASSERT_EQ(current_tab_count, starting_tab_count + 2);

  // Make sure that there's nothing else to restore.
  bool enabled;
  ASSERT_TRUE(browser_proxy->IsMenuCommandEnabled(IDC_RESTORE_TAB, &enabled));
  EXPECT_FALSE(enabled);
}

// Open a window with multiple tabs, close a tab, then close the window.
// Restore both and make sure the tab goes back into the window.
TEST_F(TabRestoreUITest, RestoreWindowAndTab) {
  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy.get());
  CheckActiveWindow(browser_proxy.get());

  int starting_tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&starting_tab_count));
  int tab_count = AddSomeTabs(browser_proxy.get(), 3);

  // Close one in the middle
  int closed_tab_index = starting_tab_count + 1;
  scoped_refptr<TabProxy> new_tab(browser_proxy->GetTab(closed_tab_index));
  ASSERT_TRUE(new_tab.get());
  // Make sure we're at url.
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, new_tab->NavigateToURL(url1_));
  // Close the tab.
  ASSERT_TRUE(new_tab->Close(true));
  new_tab = NULL;
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  EXPECT_EQ(starting_tab_count + 2, tab_count);

  // Create a new browser.
  ASSERT_TRUE(automation()->OpenNewBrowserWindow(Browser::TYPE_NORMAL, false));
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  EXPECT_EQ(2, window_count);
  CheckActiveWindow(automation()->GetBrowserWindow(1));

  // Close the first browser.
  bool application_closing;
  EXPECT_TRUE(CloseBrowser(browser_proxy.get(), &application_closing));
  EXPECT_FALSE(application_closing);
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  EXPECT_EQ(1, window_count);

  // Browser is no longer valid.
  browser_proxy = NULL;

  // Restore the first window. The expected_tabstrip_index (second argument)
  // indicates the expected active tab.
  RestoreTab(1, starting_tab_count + 1);
  browser_proxy = automation()->GetBrowserWindow(1);
  ASSERT_TRUE(browser_proxy.get());
  CheckActiveWindow(browser_proxy.get());
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  EXPECT_EQ(starting_tab_count + 2, tab_count);

  // Restore the closed tab.
  RestoreTab(1, closed_tab_index);
  CheckActiveWindow(browser_proxy.get());
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  EXPECT_EQ(starting_tab_count + 3, tab_count);
  EXPECT_EQ(url1_, GetActiveTabURL(1));
}

// Open a window with two tabs, close both (closing the window), then restore
// both. Make sure both restored tabs are in the same window.
TEST_F(TabRestoreUITest, RestoreIntoSameWindow) {
  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy.get());
  CheckActiveWindow(browser_proxy.get());

  int starting_tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&starting_tab_count));
  int tab_count = AddSomeTabs(browser_proxy.get(), 2);

  // Navigate the rightmost one to url2_ for easier identification.
  scoped_refptr<TabProxy> tab_proxy(browser_proxy->GetTab(tab_count - 1));
  ASSERT_TRUE(tab_proxy.get());
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab_proxy->NavigateToURL(url2_));

  // Create a new browser.
  ASSERT_TRUE(automation()->OpenNewBrowserWindow(Browser::TYPE_NORMAL, false));
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  EXPECT_EQ(2, window_count);
  CheckActiveWindow(automation()->GetBrowserWindow(1));

  // Close all but one tab in the first browser, left to right.
  while (tab_count > 1) {
    scoped_refptr<TabProxy> tab_to_close(browser_proxy->GetTab(0));
    ASSERT_TRUE(tab_to_close.get());
    ASSERT_TRUE(tab_to_close->Close(true));
    ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  }

  // Close the last tab, closing the browser.
  tab_proxy = browser_proxy->GetTab(0);
  ASSERT_TRUE(tab_proxy.get());
  EXPECT_TRUE(tab_proxy->Close(true));
  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(1));
  browser_proxy = NULL;
  tab_proxy = NULL;

  // Restore the last-closed tab into a new window.
  RestoreTab(1, 0);
  browser_proxy = automation()->GetBrowserWindow(1);
  ASSERT_TRUE(browser_proxy.get());
  CheckActiveWindow(browser_proxy.get());
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  EXPECT_EQ(1, tab_count);
  EXPECT_EQ(url2_, GetActiveTabURL(1));

  // Restore the next-to-last-closed tab into the same window.
  RestoreTab(1, 0);
  CheckActiveWindow(browser_proxy.get());
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));
  EXPECT_EQ(2, tab_count);
  EXPECT_EQ(url1_, GetActiveTabURL(1));
}

// Tests that a duplicate history entry is not created when we restore a page
// to an existing SiteInstance.  (Bug 1230446)
TEST_F(TabRestoreUITest, RestoreWithExistingSiteInstance) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP,
                              FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(test_server.Start());

  GURL http_url1(test_server.GetURL("files/title1.html"));
  GURL http_url2(test_server.GetURL("files/title2.html"));

  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy.get());
  int tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));

  // Add a tab
  ASSERT_TRUE(browser_proxy->AppendTab(http_url1));
  int new_tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&new_tab_count));
  EXPECT_EQ(++tab_count, new_tab_count);
  scoped_refptr<TabProxy> tab(browser_proxy->GetTab(tab_count - 1));
  ASSERT_TRUE(tab.get());

  // Navigate to another same-site URL.
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab->NavigateToURL(http_url2));

  // Close the tab.
  ASSERT_TRUE(tab->Close(true));
  tab = NULL;

  // Create a new tab to the original site.  Assuming process-per-site is
  // enabled, this will ensure that the SiteInstance used by the restored tab
  // will already exist when the restore happens.
  ASSERT_TRUE(browser_proxy->AppendTab(http_url2));

  // Restore the closed tab.
  RestoreTab(0, tab_count - 1);
  tab = browser_proxy->GetActiveTab();
  ASSERT_TRUE(tab.get());

  // And make sure the URLs match.
  EXPECT_EQ(http_url2, GetActiveTabURL());
  EXPECT_TRUE(tab->GoBack());
  EXPECT_EQ(http_url1, GetActiveTabURL());
}

// Tests that the SiteInstances used for entries in a restored tab's history
// are given appropriate max page IDs, even if the renderer for the entry
// already exists.  (Bug 1204135)
TEST_F(TabRestoreUITest, RestoreCrossSiteWithExistingSiteInstance) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP,
                              FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(test_server.Start());

  GURL http_url1(test_server.GetURL("files/title1.html"));
  GURL http_url2(test_server.GetURL("files/title2.html"));

  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy.get());
  int tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&tab_count));

  // Add a tab
  ASSERT_TRUE(browser_proxy->AppendTab(http_url1));
  int new_tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&new_tab_count));
  EXPECT_EQ(++tab_count, new_tab_count);
  scoped_refptr<TabProxy> tab(browser_proxy->GetTab(tab_count - 1));
  ASSERT_TRUE(tab.get());

  // Navigate to more URLs, then a cross-site URL.
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab->NavigateToURL(http_url2));
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab->NavigateToURL(http_url1));
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab->NavigateToURL(url1_));

  // Close the tab.
  ASSERT_TRUE(tab->Close(true));
  tab = NULL;

  // Create a new tab to the original site.  Assuming process-per-site is
  // enabled, this will ensure that the SiteInstance will already exist when
  // the user clicks Back in the restored tab.
  ASSERT_TRUE(browser_proxy->AppendTab(http_url2));

  // Restore the closed tab.
  RestoreTab(0, tab_count - 1);
  tab = browser_proxy->GetActiveTab();
  ASSERT_TRUE(tab.get());

  // And make sure the URLs match.
  EXPECT_EQ(url1_, GetActiveTabURL());
  EXPECT_TRUE(tab->GoBack());
  EXPECT_EQ(http_url1, GetActiveTabURL());

  // Navigating to a new URL should clear the forward list, because the max
  // page ID of the renderer should have been updated when we restored the tab.
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab->NavigateToURL(http_url2));
  EXPECT_FALSE(tab->GoForward());
  EXPECT_EQ(http_url2, GetActiveTabURL());
}

TEST_F(TabRestoreUITest, RestoreWindow) {
  // Create a new window.
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_TRUE(automation()->OpenNewBrowserWindow(Browser::TYPE_NORMAL, false));
  int new_window_count = 0;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&new_window_count));
  EXPECT_EQ(++window_count, new_window_count);

  // Create two more tabs, one with url1, the other url2.
  scoped_refptr<BrowserProxy> browser_proxy(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy.get());
  int initial_tab_count;
  ASSERT_TRUE(browser_proxy->GetTabCount(&initial_tab_count));
  ASSERT_TRUE(browser_proxy->AppendTab(url1_));
  ASSERT_TRUE(browser_proxy->WaitForTabCountToBecome(initial_tab_count + 1));
  scoped_refptr<TabProxy> new_tab(browser_proxy->GetTab(initial_tab_count));
  ASSERT_TRUE(new_tab.get());
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, new_tab->NavigateToURL(url1_));
  ASSERT_TRUE(browser_proxy->AppendTab(url2_));
  ASSERT_TRUE(browser_proxy->WaitForTabCountToBecome(initial_tab_count + 2));
  new_tab = browser_proxy->GetTab(initial_tab_count + 1);
  ASSERT_TRUE(new_tab.get());
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, new_tab->NavigateToURL(url2_));

  // Close the window.
  ASSERT_TRUE(browser_proxy->RunCommand(IDC_CLOSE_WINDOW));
  browser_proxy = NULL;
  new_tab = NULL;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&new_window_count));
  EXPECT_EQ(window_count - 1, new_window_count);

  // Restore the window.
  browser_proxy = automation()->GetBrowserWindow(0);
  ASSERT_TRUE(browser_proxy.get());
  ASSERT_TRUE(browser_proxy->RunCommand(IDC_RESTORE_TAB));
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&new_window_count));
  EXPECT_EQ(window_count, new_window_count);

  browser_proxy = automation()->GetBrowserWindow(1);
  int tab_count;
  EXPECT_TRUE(browser_proxy->GetTabCount(&tab_count));
  EXPECT_EQ(initial_tab_count + 2, tab_count);

  scoped_refptr<TabProxy> restored_tab_proxy(
        browser_proxy->GetTab(initial_tab_count));
  ASSERT_TRUE(restored_tab_proxy.get());
  ASSERT_TRUE(restored_tab_proxy->WaitForTabToBeRestored(
      TestTimeouts::action_timeout_ms()));
  GURL url;
  ASSERT_TRUE(restored_tab_proxy->GetCurrentURL(&url));
  EXPECT_TRUE(url == url1_);

  restored_tab_proxy = browser_proxy->GetTab(initial_tab_count + 1);
  ASSERT_TRUE(restored_tab_proxy.get());
  ASSERT_TRUE(restored_tab_proxy->WaitForTabToBeRestored(
      TestTimeouts::action_timeout_ms()));
  ASSERT_TRUE(restored_tab_proxy->GetCurrentURL(&url));
  EXPECT_TRUE(url == url2_);
}

// Restore tab with special URL about:credits and make sure the page loads
// properly after restore. See http://crbug.com/31905.
TEST_F(TabRestoreUITest, RestoreTabWithSpecialURL) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  CheckActiveWindow(browser.get());

  // Navigate new tab to a special URL.
  const GURL special_url(chrome::kAboutCreditsURL);
  ASSERT_TRUE(browser->AppendTab(special_url));
  scoped_refptr<TabProxy> tab(browser->GetActiveTab());
  ASSERT_TRUE(tab.get());

  // Close the tab.
  ASSERT_TRUE(tab->Close(true));

  // Restore the closed tab.
  RestoreTab(0, 1);
  tab = browser->GetTab(1);
  ASSERT_TRUE(tab.get());
  ASSERT_TRUE(tab->WaitForTabToBeRestored(TestTimeouts::action_timeout_ms()));

  // See if content is as expected.
  EXPECT_TRUE(tab->FindInPage(std::wstring(L"webkit"), FWD, IGNORE_CASE, false,
                              NULL));
}

// Restore tab with special URL in its navigation history, go back to that
// entry and see that it loads properly. See http://crbug.com/31905
TEST_F(TabRestoreUITest, RestoreTabWithSpecialURLOnBack) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP,
                              FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(test_server.Start());

  const GURL http_url(test_server.GetURL("files/title1.html"));

  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  CheckActiveWindow(browser.get());

  // Navigate new tab to a special URL.
  const GURL special_url(chrome::kAboutCreditsURL);
  ASSERT_TRUE(browser->AppendTab(special_url));
  scoped_refptr<TabProxy> tab(browser->GetActiveTab());
  ASSERT_TRUE(tab.get());

  // Then navigate to a normal URL.
  ASSERT_TRUE(tab->NavigateToURL(http_url));

  // Close the tab.
  ASSERT_TRUE(tab->Close(true));

  // Restore the closed tab.
  RestoreTab(0, 1);
  tab = browser->GetTab(1);
  ASSERT_TRUE(tab.get());
  ASSERT_TRUE(tab->WaitForTabToBeRestored(TestTimeouts::action_timeout_ms()));
  GURL url;
  ASSERT_TRUE(tab->GetCurrentURL(&url));
  ASSERT_EQ(http_url, url);

  // Go back, and see if content is as expected.
  ASSERT_TRUE(tab->GoBack());
  EXPECT_TRUE(tab->FindInPage(std::wstring(L"webkit"), FWD, IGNORE_CASE, false,
                              NULL));
}
