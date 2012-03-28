// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sync/one_click_signin_bubble_view.h"

#include "base/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/page_transition_types.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/events/event.h"

namespace {

void OnClickLink(Browser* browser) {
  browser->AddSelectedTabWithURL(GURL("http://www.example.com"),
                                 content::PAGE_TRANSITION_AUTO_BOOKMARK);
}

}  // namespace

class OneClickSigninBubbleViewBrowserTest : public InProcessBrowserTest {
 public:
  OneClickSigninBubbleViewBrowserTest() {}

  void ShowOneClickSigninBubble() {
    base::Closure on_click_link_callback =
        base::Bind(&OnClickLink, base::Unretained(browser()));
    browser()->window()->ShowOneClickSigninBubble(on_click_link_callback,
                                                  on_click_link_callback);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OneClickSigninBubbleViewBrowserTest);
};

IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleViewBrowserTest, Show) {
  ShowOneClickSigninBubble();
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_TRUE(OneClickSigninBubbleView::IsShowing());

  OneClickSigninBubbleView::Hide();
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(OneClickSigninBubbleView::IsShowing());
}

IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleViewBrowserTest, CloseButton) {
  int initial_tab_count = browser()->tab_count();

  ShowOneClickSigninBubble();
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_TRUE(OneClickSigninBubbleView::IsShowing());

  OneClickSigninBubbleView* view = OneClickSigninBubbleView::view_for_testing();
  EXPECT_TRUE(view != NULL);
  EXPECT_EQ(initial_tab_count, browser()->tab_count());

  // Simulate pressing the OK button.  Set the message loop in the bubble
  // view so that it can be quit once the bubble is hidden.
  view->set_message_loop_for_testing(MessageLoop::current());
  views::ButtonListener* listener = view;
  views::MouseEvent event(ui::ET_MOUSE_PRESSED, 0, 0, 0);
  listener->ButtonPressed(view->close_button_for_testing(), event);

  // View should no longer be showing.  The message loop will exit once the
  // fade animation of the bubble is done.
  ui_test_utils::RunMessageLoop();
  EXPECT_FALSE(OneClickSigninBubbleView::IsShowing());
  EXPECT_EQ(initial_tab_count, browser()->tab_count());
}

IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleViewBrowserTest, ViewLink) {
  int initial_tab_count = browser()->tab_count();

  ShowOneClickSigninBubble();
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_TRUE(OneClickSigninBubbleView::IsShowing());

  OneClickSigninBubbleView* view = OneClickSigninBubbleView::view_for_testing();
  EXPECT_TRUE(view != NULL);
  EXPECT_EQ(initial_tab_count, browser()->tab_count());

  // Simulate pressing a link in the bubble.  This should open a new tab.
  views::LinkListener* listener = view;
  listener->LinkClicked(view->learn_more_link_for_testing(), 0);

  // View should no longer be showing and a new tab should be opened.
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(OneClickSigninBubbleView::IsShowing());
  EXPECT_EQ(initial_tab_count + 1, browser()->tab_count());
}
