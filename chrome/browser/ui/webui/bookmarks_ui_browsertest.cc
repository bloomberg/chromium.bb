// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/md_bookmarks/md_bookmarks_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"

class BookmarksTest : public InProcessBrowserTest {
 public:
  BookmarksTest() {}

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Re-enable accessibility checks when audit failures are resolved.
    // AX_TEXT_01: http://crbug.com/559201
    // AX_ARIA_08: http://crbug.com/559202
    // EnableAccessibilityChecksForTestCase(true);
  }

  void OpenBookmarksManager() {
    content::TestNavigationObserver navigation_observer(
        browser()->tab_strip_model()->GetActiveWebContents(),
        MdBookmarksUI::IsEnabled() ? 1 : 2);
    navigation_observer.StartWatchingNewWebContents();

    // Bring up the bookmarks manager tab.
    chrome::ShowBookmarkManager(browser());
    navigation_observer.Wait();
  }

  void AssertIsBookmarksPage(content::WebContents* tab) {
    GURL url;
    std::string out;

    if (MdBookmarksUI::IsEnabled()) {
      ASSERT_TRUE(content::ExecuteScriptAndExtractString(
          tab, "domAutomationController.send(location.href)", &out));
      ASSERT_EQ("chrome://bookmarks/?id=1", out);
      return;
    }

    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        tab,
        "domAutomationController.send(location.protocol)",
        &out));
    ASSERT_EQ("chrome-extension:", out);
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        tab,
        "domAutomationController.send(location.pathname)",
        &out));
    ASSERT_EQ("/main.html", out);
  }
};

IN_PROC_BROWSER_TEST_F(BookmarksTest, CommandOpensBookmarksTab) {
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  // Bring up the bookmarks manager tab.
  OpenBookmarksManager();
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  AssertIsBookmarksPage(browser()->tab_strip_model()->GetActiveWebContents());
}

// Flaky on Mac: https://crbug.com/524216
#if defined(OS_MACOSX) || defined(OS_LINUX)
#define MAYBE_CommandAgainGoesBackToBookmarksTab \
  DISABLED_CommandAgainGoesBackToBookmarksTab
#else
#define MAYBE_CommandAgainGoesBackToBookmarksTab \
  CommandAgainGoesBackToBookmarksTab
#endif

IN_PROC_BROWSER_TEST_F(BookmarksTest,
                       MAYBE_CommandAgainGoesBackToBookmarksTab) {
  ui_test_utils::NavigateToURL(
      browser(),
      ui_test_utils::GetTestUrl(base::FilePath(),
                                base::FilePath().AppendASCII("simple.html")));
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  // Bring up the bookmarks manager tab.
  OpenBookmarksManager();
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  AssertIsBookmarksPage(browser()->tab_strip_model()->GetActiveWebContents());

  // Switch to first tab and run command again.
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  chrome::ShowBookmarkManager(browser());

  // Ensure the bookmarks ui tab is active.
  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(BookmarksTest, TwoCommandsOneTab) {
  content::TestNavigationObserver navigation_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  chrome::ShowBookmarkManager(browser());
  chrome::ShowBookmarkManager(browser());
  navigation_observer.Wait();

  ASSERT_EQ(1, browser()->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(BookmarksTest, BookmarksLoaded) {
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIBookmarksURL));
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  AssertIsBookmarksPage(browser()->tab_strip_model()->GetActiveWebContents());
}
