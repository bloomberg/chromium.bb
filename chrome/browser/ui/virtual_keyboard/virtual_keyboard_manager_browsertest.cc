  // Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/virtual_keyboard/virtual_keyboard_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "net/base/mock_host_resolver.h"
#include "views/widget/widget.h"

class VirtualKeyboardManagerTest : public InProcessBrowserTest,
                            public content::NotificationObserver {
 public:
  VirtualKeyboardManagerTest()
      : InProcessBrowserTest(),
        keyboard_visible_(false) {
  }

  bool keyboard_visible() const { return keyboard_visible_; }

  void SetupNotificationListener() {
    registrar_.Add(this,
                   chrome::NOTIFICATION_KEYBOARD_VISIBILITY_CHANGED,
                   content::NotificationService::AllSources());
  }

 private:
  virtual void TearDown() {
    registrar_.RemoveAll();
    InProcessBrowserTest::TearDown();
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    DCHECK_EQ(chrome::NOTIFICATION_KEYBOARD_VISIBILITY_CHANGED, type);
    keyboard_visible_ = *content::Details<bool>(details).ptr();
  }

  bool keyboard_visible_;
  content::NotificationRegistrar registrar_;
};

IN_PROC_BROWSER_TEST_F(VirtualKeyboardManagerTest, TestVisibility) {
  SetupNotificationListener();
  browser()->window()->Show();

  // Move focus between the omnibox and the wrench menu a few times. Note that
  // it is necessary to RunAllPendingInMessageLoop each time after moving
  // focus between the omnibox and the wrench menu because of the task posted in
  // AccessiblePaneView::FocusWillChange

  browser()->FocusAppMenu();
  EXPECT_FALSE(keyboard_visible());
  ui_test_utils::RunAllPendingInMessageLoop();

  browser()->FocusLocationBar();
  EXPECT_TRUE(keyboard_visible());
  ui_test_utils::RunAllPendingInMessageLoop();

  browser()->FocusAppMenu();
  EXPECT_FALSE(keyboard_visible());
  ui_test_utils::RunAllPendingInMessageLoop();

  browser()->FocusLocationBar();
  EXPECT_TRUE(keyboard_visible());
  ui_test_utils::RunAllPendingInMessageLoop();

  // Test with some tabs now
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());
  GURL base_url = test_server()->GetURL("files/keyboard/");

  // Go to a page that gives focus to a textfield onload.
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("focus.html"));
  EXPECT_TRUE(keyboard_visible());

  // Open a new tab that does not give focus to a textfield onload.
  ui_test_utils::WindowedNotificationObserver load_stop_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  browser()->AddSelectedTabWithURL(base_url.Resolve("blank.html"),
                                   content::PAGE_TRANSITION_LINK);
  load_stop_observer.Wait();

  // Focus the first tab where the textfield has the focus.
  browser()->SelectNextTab();
  EXPECT_TRUE(keyboard_visible());

  // Focus the next tab again.
  browser()->SelectNextTab();
  EXPECT_FALSE(keyboard_visible());
}
