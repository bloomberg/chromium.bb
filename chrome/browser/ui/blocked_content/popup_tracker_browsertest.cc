// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/popup_tracker.h"

#include <string>

#include "base/supports_user_data.h"
#include "base/test/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"

const char kPopupEngagement[] =
    "ContentSettings.Popups.FirstDocumentEngagementTime2";

class PopupTrackerBrowserTest : public InProcessBrowserTest {
 public:
  PopupTrackerBrowserTest() {}
  ~PopupTrackerBrowserTest() override {}

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

IN_PROC_BROWSER_TEST_F(PopupTrackerBrowserTest, NoPopup_NoTracker) {
  base::HistogramTester tester;
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  EXPECT_FALSE(PopupTracker::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents()));

  tester.ExpectTotalCount(kPopupEngagement, 0);
}

IN_PROC_BROWSER_TEST_F(PopupTrackerBrowserTest, WindowOpenPopup_HasTracker) {
  base::HistogramTester tester;
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));

  content::TestNavigationObserver navigation_observer(nullptr, 1);
  navigation_observer.StartWatchingNewWebContents();

  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.open('/title1.html')"));
  navigation_observer.Wait();

  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_TRUE(PopupTracker::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents()));

  // Close the popup and check metric.
  content::WebContentsDestroyedWatcher destroyed_watcher(
      browser()->tab_strip_model()->GetActiveWebContents());
  browser()->tab_strip_model()->CloseAllTabs();
  destroyed_watcher.Wait();

  tester.ExpectTotalCount(kPopupEngagement, 1);
}

// OpenURLFromTab goes through a different code path than traditional popups
// that use window.open(). Make sure the tracker is created in those cases.
IN_PROC_BROWSER_TEST_F(PopupTrackerBrowserTest, ControlClick_HasTracker) {
  base::HistogramTester tester;
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/popup_blocker/popup-simulated-click-on-anchor.html"));

  // Mac uses command instead of control for the new tab action.
  bool is_mac = false;
#if defined(OS_MACOSX)
  is_mac = true;
#endif

  content::TestNavigationObserver navigation_observer(nullptr, 1);
  navigation_observer.StartWatchingNewWebContents();

  SimulateKeyPress(browser()->tab_strip_model()->GetActiveWebContents(),
                   ui::DomKey::ENTER, ui::DomCode::ENTER, ui::VKEY_RETURN,
                   !is_mac /* control */, false /* shift */, false /* alt */,
                   is_mac /* command */);
  navigation_observer.Wait();

  EXPECT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  content::WebContents* new_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_TRUE(PopupTracker::FromWebContents(new_contents));

  // Close the popup and check metric.
  content::WebContentsDestroyedWatcher destroyed_watcher(new_contents);
  BrowserList::CloseAllBrowsersWithProfile(
      Profile::FromBrowserContext(new_contents->GetBrowserContext()));
  destroyed_watcher.Wait();

  tester.ExpectTotalCount(kPopupEngagement, 1);
}

IN_PROC_BROWSER_TEST_F(PopupTrackerBrowserTest, ShiftClick_HasTracker) {
  base::HistogramTester tester;
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/popup_blocker/popup-simulated-click-on-anchor.html"));

  content::TestNavigationObserver navigation_observer(nullptr, 1);
  navigation_observer.StartWatchingNewWebContents();

  SimulateKeyPress(browser()->tab_strip_model()->GetActiveWebContents(),
                   ui::DomKey::ENTER, ui::DomCode::ENTER, ui::VKEY_RETURN,
                   false /* control */, true /* shift */, false /* alt */,
                   false /* command */);
  navigation_observer.Wait();

  EXPECT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  content::WebContents* new_contents = BrowserList::GetInstance()
                                           ->GetLastActive()
                                           ->tab_strip_model()
                                           ->GetActiveWebContents();
  EXPECT_TRUE(PopupTracker::FromWebContents(new_contents));

  // Close the popup and check metric.
  content::WebContentsDestroyedWatcher destroyed_watcher(new_contents);
  BrowserList::CloseAllBrowsersWithProfile(
      Profile::FromBrowserContext(new_contents->GetBrowserContext()));
  destroyed_watcher.Wait();

  tester.ExpectTotalCount(kPopupEngagement, 1);
}

IN_PROC_BROWSER_TEST_F(PopupTrackerBrowserTest, WhitelistedPopup_HasTracker) {
  base::HistogramTester tester;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Is blocked by the popup blocker.
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/popup_blocker/popup-window-open.html"));
  EXPECT_TRUE(TabSpecificContentSettings::FromWebContents(web_contents)
                  ->IsContentBlocked(CONTENT_SETTINGS_TYPE_POPUPS));

  // Click through to open the popup.
  content::TestNavigationObserver navigation_observer(nullptr, 1);
  navigation_observer.StartWatchingNewWebContents();
  auto* popup_blocker = PopupBlockerTabHelper::FromWebContents(web_contents);
  popup_blocker->ShowBlockedPopup(
      popup_blocker->GetBlockedPopupRequests().begin()->first,
      WindowOpenDisposition::NEW_FOREGROUND_TAB);
  navigation_observer.Wait();

  // Close the popup and check metric.
  content::WebContentsDestroyedWatcher destroyed_watcher(
      browser()->tab_strip_model()->GetActiveWebContents());
  browser()->tab_strip_model()->CloseAllTabs();
  destroyed_watcher.Wait();

  tester.ExpectTotalCount(kPopupEngagement, 1);
}

IN_PROC_BROWSER_TEST_F(PopupTrackerBrowserTest, NoOpener_NoTracker) {
  const GURL url = embedded_test_server()->GetURL("/title1.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  content::WebContents* new_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);

  EXPECT_FALSE(PopupTracker::FromWebContents(new_contents));
}
