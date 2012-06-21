// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/ui_test.h"

#include "base/test/test_timeouts.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_navigation_observer.h"

class BookmarksTest : public InProcessBrowserTest {
 public:
  BookmarksTest() {
    EnableDOMAutomation();
  }

  void OpenBookmarksManager() {
    content::TestNavigationObserver navigation_observer(
        content::NotificationService::AllSources(), NULL, 2);

    // Bring up the bookmarks manager tab.
    chrome::ShowBookmarkManager(browser());
    navigation_observer.Wait();
  }

  void AssertIsBookmarksPage(content::WebContents* tab) {
    GURL url;
    std::string out;
    ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
        tab->GetRenderViewHost(), L"",
        L"domAutomationController.send(location.protocol)", &out));
    ASSERT_EQ("chrome-extension:", out);
    ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
        tab->GetRenderViewHost(), L"",
        L"domAutomationController.send(location.pathname)", &out));
    ASSERT_EQ("/main.html", out);
  }
};

IN_PROC_BROWSER_TEST_F(BookmarksTest, ShouldRedirectToExtension) {
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIBookmarksURL));
  AssertIsBookmarksPage(browser()->GetActiveWebContents());
}

IN_PROC_BROWSER_TEST_F(BookmarksTest, CommandOpensBookmarksTab) {
  ASSERT_EQ(1, browser()->tab_count());

  // Bring up the bookmarks manager tab.
  OpenBookmarksManager();
  ASSERT_EQ(1, browser()->tab_count());
  AssertIsBookmarksPage(browser()->GetActiveWebContents());
}

// If this flakes on Mac, use: http://crbug.com/87200
IN_PROC_BROWSER_TEST_F(BookmarksTest, CommandAgainGoesBackToBookmarksTab) {
  ui_test_utils::NavigateToURL(
      browser(),
      ui_test_utils::GetTestUrl(FilePath(),
                                FilePath().AppendASCII("simple.html")));
  ASSERT_EQ(1, browser()->tab_count());

  // Bring up the bookmarks manager tab.
  OpenBookmarksManager();
  ASSERT_EQ(2, browser()->tab_count());

  AssertIsBookmarksPage(browser()->GetActiveWebContents());

  // Switch to first tab and run command again.
  browser()->ActivateTabAt(0, true);
  chrome::ShowBookmarkManager(browser());

  // Ensure the bookmarks ui tab is active.
  ASSERT_EQ(1, browser()->active_index());
  ASSERT_EQ(2, browser()->tab_count());
}

IN_PROC_BROWSER_TEST_F(BookmarksTest, TwoCommandsOneTab) {
  content::TestNavigationObserver navigation_observer(
      content::NotificationService::AllSources());
  chrome::ShowBookmarkManager(browser());
  chrome::ShowBookmarkManager(browser());
  navigation_observer.Wait();

  ASSERT_EQ(1, browser()->tab_count());
}

IN_PROC_BROWSER_TEST_F(BookmarksTest, BookmarksLoaded) {
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIBookmarksURL));
  ASSERT_EQ(1, browser()->tab_count());
  AssertIsBookmarksPage(browser()->GetActiveWebContents());
}
