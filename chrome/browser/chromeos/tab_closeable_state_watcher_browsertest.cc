// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/tab_closeable_state_watcher.h"

#include "base/file_path.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog.h"
#include "chrome/browser/ui/app_modal_dialogs/native_app_modal_dialog.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class TabCloseableStateWatcherTest : public InProcessBrowserTest {
 public:
  TabCloseableStateWatcherTest() {
    // This test is testing TabCloseStateWatcher, so enable it.
    EnableTabCloseableStateWatcher();
    blank_url_ = GURL(chrome::kAboutBlankURL);
    ntp_url_ = GURL(chrome::kChromeUINewTabURL);
    other_url_ = ui_test_utils::GetTestUrl(
        FilePath(FilePath::kCurrentDirectory),
        FilePath(FILE_PATH_LITERAL("title2.html")));
  }

 protected:
  // Wrapper for Browser::AddTabWithURL
  void AddTabWithURL(Browser* browser, const GURL& url) {
    AddTabAtIndexToBrowser(browser, 0, url, PageTransition::TYPED);
    // Wait for page to finish loading.
    ui_test_utils::WaitForNavigation(
        &browser->GetSelectedTabContents()->controller());
  }

  // Wrapper for TabCloseableStateWatcher::CanCloseTab
  bool CanCloseTab(const Browser* browser) {
    return browser->tabstrip_model()->delegate()->CanCloseTab();
  }

  // Create popup browser.
  Browser* CreatePopupBrowser() {
    // This is mostly duplicated from InPocessBrowserTest::CreateBrowser,
    // except that a popup browser is created here.
    Browser* popup_browser = Browser::CreateForType(Browser::TYPE_POPUP,
                                                    browser()->profile());
    AddTabWithURL(popup_browser, ntp_url_);
    popup_browser->window()->Show();
    return popup_browser;
  }

  // Create incognito browser.
  Browser* CreateIncognitoBrowser() {
    // This is mostly duplicated from InPocessBrowserTest::CreateBrowser,
    // except that an incognito browser is created here.
    Browser* incognito_browser =
        Browser::Create(browser()->profile()->GetOffTheRecordProfile());
    AddTabWithURL(incognito_browser, ntp_url_);
    incognito_browser->window()->Show();
    return incognito_browser;
  }

  void NavigateToURL(const GURL& url) {
    ui_test_utils::NavigateToURL(browser(), url);
    ui_test_utils::RunAllPendingInMessageLoop();
  }

  // Navigate to URL with BeforeUnload handler.
  void NavigateToBeforeUnloadURL() {
    const std::string kBeforeUnloadHtml =
        "<html><head><title>beforeunload</title></head><body>"
        "<script>window.onbeforeunload=function(e){return 'foo'}</script>"
        "</body></html>";
    NavigateToURL(GURL("data:text/html," + kBeforeUnloadHtml));
  }

  // Data members.
  GURL blank_url_;
  GURL ntp_url_;
  GURL other_url_;
};

// This is used to block until a new tab in the specified browser is inserted.
class NewTabObserver : public TabStripModelObserver {
 public:
  explicit NewTabObserver(Browser* browser) : browser_(browser) {
    browser_->tabstrip_model()->AddObserver(this);
    ui_test_utils::RunMessageLoop();
  }
  virtual ~NewTabObserver() {
    browser_->tabstrip_model()->RemoveObserver(this);
  }

 private:
  virtual void TabInsertedAt(TabContentsWrapper* contents,
                             int index,
                             bool foreground) {
    MessageLoopForUI::current()->Quit();
  }

  Browser* browser_;
};

// Tests with the only tab in the only normal browser:
// - if tab is about:blank, it is closeable
// - if tab is NewTabPage, it is not closeable
// - if tab is other url, it is closeable
IN_PROC_BROWSER_TEST_F(TabCloseableStateWatcherTest,
                       OneNormalBrowserWithOneTab) {
  // Check that default about::blank tab is closeable.
  ASSERT_EQ(1, browser()->tab_count());
  EXPECT_TRUE(CanCloseTab(browser()));

  // Naviate tab to NewTabPage, and check that it's not closeable.
  NavigateToURL(ntp_url_);
  EXPECT_FALSE(CanCloseTab(browser()));

  // Navigate tab to any other URL, and check that it's closeable.
  NavigateToURL(other_url_);
  EXPECT_TRUE(CanCloseTab(browser()));
}

// Tests with 2 tabs in the only normal browser
// - as long as there's > 1 tab, all tabs in the browser are always closeable
IN_PROC_BROWSER_TEST_F(TabCloseableStateWatcherTest,
                       OneNormalBrowserWithTwoTabs) {
  // 1 NewTabPage with any other tab are closeable.
  // First, set up the first NewTabPage.
  NavigateToURL(ntp_url_);
  EXPECT_FALSE(CanCloseTab(browser()));

  // Add the 2nd tab with blank page.
  AddTabWithURL(browser(), blank_url_);
  ASSERT_EQ(2, browser()->tab_count());
  EXPECT_TRUE(CanCloseTab(browser()));

  // Navigate 2nd tab to other URL.
  NavigateToURL(other_url_);
  EXPECT_TRUE(CanCloseTab(browser()));

  // Navigate 2nd tab to NewTabPage.
  NavigateToURL(ntp_url_);
  EXPECT_TRUE(CanCloseTab(browser()));

  // Close 1st NewTabPage.
  browser()->tabstrip_model()->CloseTabContentsAt(0,
      TabStripModel::CLOSE_CREATE_HISTORICAL_TAB);
  EXPECT_FALSE(CanCloseTab(browser()));
}

// Tests with one tab in one normal browser and another non-normal browser.
// - non-normal browser with any tab(s) is always closeable.
// - non-normal browser does not affect closeable state of tab(s) in normal
// browser(s).
IN_PROC_BROWSER_TEST_F(TabCloseableStateWatcherTest, SecondNonNormalBrowser) {
  // Open non-normal browser.
  Browser* popup_browser = CreatePopupBrowser();
  EXPECT_TRUE(CanCloseTab(browser()));
  EXPECT_TRUE(CanCloseTab(popup_browser));

  // Navigate to NewTabPage for 1st browser.
  NavigateToURL(ntp_url_);
  EXPECT_FALSE(CanCloseTab(browser()));
  EXPECT_TRUE(CanCloseTab(popup_browser));

  // Close non-normal browser.
  popup_browser->CloseWindow();
  EXPECT_FALSE(CanCloseTab(browser()));
}

// Tests closing a closeable tab - tab should be closed, browser should remain
// opened with a NewTabPage.
IN_PROC_BROWSER_TEST_F(TabCloseableStateWatcherTest, CloseCloseableTab) {
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_TRUE(CanCloseTab(browser()));
  browser()->CloseTab();
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(ntp_url_, browser()->GetSelectedTabContents()->GetURL());
}

// Tests closing a closeable browser - all tabs in browser should be closed,
// browser should remain opened with a NewTabPage.
IN_PROC_BROWSER_TEST_F(TabCloseableStateWatcherTest, CloseCloseableBrowser) {
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_TRUE(CanCloseTab(browser()));
  browser()->CloseWindow();
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(browser(), *(BrowserList::begin()));
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(ntp_url_, browser()->GetSelectedTabContents()->GetURL());
}

// Tests closing a non-closeable tab and hence non-closeable browser - tab and
// browser should remain opened.
IN_PROC_BROWSER_TEST_F(TabCloseableStateWatcherTest,
                       CloseNonCloseableTabAndBrowser) {
  // Close non-closeable tab.
  EXPECT_EQ(1, browser()->tab_count());
  NavigateToURL(ntp_url_);
  EXPECT_FALSE(CanCloseTab(browser()));
  TabContents* tab_contents = browser()->GetSelectedTabContents();
  browser()->CloseTab();
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(tab_contents, browser()->GetSelectedTabContents());

  // Close browser with non-closeable tab.
  browser()->CloseWindow();
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(browser(), *(BrowserList::begin()));
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(tab_contents, browser()->GetSelectedTabContents());
}

// Tests an incognito browsr with a normal browser.
// - when incognito browser is opened, all browsers (including previously
//   non-clsoeable normal browsers) become closeable.
// - when incognito browser is closed, normal browsers return to adhering to the
//   original closebable rules.
IN_PROC_BROWSER_TEST_F(TabCloseableStateWatcherTest, SecondIncognitoBrowser) {
  NavigateToURL(ntp_url_);
  EXPECT_FALSE(CanCloseTab(browser()));

  // Open an incognito browser.
  Browser* incognito_browser = CreateIncognitoBrowser();
  EXPECT_TRUE(incognito_browser->profile()->IsOffTheRecord());
  EXPECT_EQ(2u, BrowserList::size());
  EXPECT_TRUE(CanCloseTab(browser()));
  EXPECT_TRUE(CanCloseTab(incognito_browser));

  // Close incognito browser.
  incognito_browser->CloseWindow();
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(browser(), *(BrowserList::begin()));
  EXPECT_FALSE(CanCloseTab(browser()));
}

// Tests closing an incognito browser - the incognito browser should close,
// and a new normal browser opened with a NewTabPage (which is not closeable).
IN_PROC_BROWSER_TEST_F(TabCloseableStateWatcherTest, CloseIncognitoBrowser) {
  // Open an incognito browser.
  Browser* incognito_browser = CreateIncognitoBrowser();
  EXPECT_TRUE(incognito_browser->profile()->IsOffTheRecord());
  EXPECT_EQ(2u, BrowserList::size());

  // Close 1st normal browser.
  browser()->CloseWindow();
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(incognito_browser, *(BrowserList::begin()));
  EXPECT_TRUE(CanCloseTab(incognito_browser));

  // Close incognito browser.
  incognito_browser->CloseWindow();
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(1u, BrowserList::size());
  Browser* new_browser = *(BrowserList::begin());
  EXPECT_FALSE(new_browser->profile()->IsOffTheRecord());
  EXPECT_EQ(1, new_browser->tab_count());
  EXPECT_EQ(ntp_url_, new_browser->GetSelectedTabContents()->GetURL());
}

// Tests closing of browser with BeforeUnload handler where user clicks cancel
// (i.e. stay on the page and cancel closing) - browser and its tabs should stay
// the same.
IN_PROC_BROWSER_TEST_F(TabCloseableStateWatcherTest,
                       CloseBrowserWithBeforeUnloadHandlerCancel) {
  // Navigate to URL with BeforeUnload handler.
  NavigateToBeforeUnloadURL();
  EXPECT_TRUE(CanCloseTab(browser()));

  // Close browser, click Cancel in BeforeUnload confirm dialog.
  TabContents* tab_contents = browser()->GetSelectedTabContents();
  browser()->CloseWindow();
  AppModalDialog* confirm = ui_test_utils::WaitForAppModalDialog();
  confirm->native_dialog()->CancelAppModalDialog();
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(browser(), *(BrowserList::begin()));
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(tab_contents, browser()->GetSelectedTabContents());

  // Close the browser.
  browser()->CloseWindow();
  confirm = ui_test_utils::WaitForAppModalDialog();
  confirm->native_dialog()->AcceptAppModalDialog();
  ui_test_utils::RunAllPendingInMessageLoop();
}

// Tests closing of browser with BeforeUnload handler where user clicks OK (i.e.
// leave the page and proceed with closing), all tabs in browser should close,
// browser remains opened with a NewTabPage.
IN_PROC_BROWSER_TEST_F(TabCloseableStateWatcherTest,
                       CloseBrowserWithBeforeUnloadHandlerOK) {
  // Navigate to URL with BeforeUnload handler.
  NavigateToBeforeUnloadURL();
  EXPECT_TRUE(CanCloseTab(browser()));

  // Close browser, click OK in BeforeUnload confirm dialog.
  browser()->CloseWindow();
  AppModalDialog* confirm = ui_test_utils::WaitForAppModalDialog();
  confirm->native_dialog()->AcceptAppModalDialog();
  NewTabObserver new_tab_observer(browser());
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(browser(), *(BrowserList::begin()));
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(ntp_url_, browser()->GetSelectedTabContents()->GetURL());
}

}  // namespace chromeos
