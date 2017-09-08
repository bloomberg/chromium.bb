// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/popup_tracker.h"

#include <string>

#include "base/supports_user_data.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"

class PopupTrackerBrowserTest : public InProcessBrowserTest {
 public:
  PopupTrackerBrowserTest() {}
  ~PopupTrackerBrowserTest() override {}

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

IN_PROC_BROWSER_TEST_F(PopupTrackerBrowserTest, NoPopup_NoTracker) {
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  EXPECT_FALSE(PopupTracker::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_F(PopupTrackerBrowserTest, WindowOpenPopup_HasTracker) {
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  std::string script = R"(
    var opened = !!window.open();
    window.domAutomationController.send(opened);
  )";
  bool opened_window = true;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(), script,
      &opened_window));
  EXPECT_TRUE(opened_window);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_TRUE(PopupTracker::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents()));
}

// OpenURLFromTab goes through a different code path than traditional popups
// that use window.open(). Make sure the tracker is created in those cases.
IN_PROC_BROWSER_TEST_F(PopupTrackerBrowserTest, OpenURLPopup_HasTracker) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/popup_blocker/popup-simulated-click-on-anchor.html"));

  content::WindowedNotificationObserver wait_for_new_tab(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());

  SimulateKeyPress(browser()->tab_strip_model()->GetActiveWebContents(),
                   ui::DomKey::ENTER, ui::DomCode::ENTER, ui::VKEY_RETURN,
                   false /* control */, true /* shift */, false /* alt */,
                   false /* command */);
  wait_for_new_tab.Wait();

  EXPECT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  EXPECT_TRUE(PopupTracker::FromWebContents(BrowserList::GetInstance()
                                                ->GetLastActive()
                                                ->tab_strip_model()
                                                ->GetActiveWebContents()));
}
