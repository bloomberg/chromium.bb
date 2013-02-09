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

class OneClickSigninBubbleViewBrowserTest : public InProcessBrowserTest {
 public:
  OneClickSigninBubbleViewBrowserTest()
      : on_start_sync_called_(false),
        mode_(OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST) {
  }

  OneClickSigninBubbleView* ShowOneClickSigninBubble() {
    browser()->window()->ShowOneClickSigninBubble(
        BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE,
        base::Bind(&OneClickSigninBubbleViewBrowserTest::OnStartSync, this));

    content::RunAllPendingInMessageLoop();
    EXPECT_TRUE(OneClickSigninBubbleView::IsShowing());

    OneClickSigninBubbleView* view =
        OneClickSigninBubbleView::view_for_testing();
    EXPECT_TRUE(view != NULL);

    // Simulate pressing a link in the bubble.  This should open a new tab.
    view->message_loop_for_testing_ = MessageLoop::current();
    return view;
  }

  void OnStartSync(OneClickSigninSyncStarter::StartSyncMode mode) {
    on_start_sync_called_ = true;
    mode_ = mode;
  }

 protected:
  bool on_start_sync_called_;
  OneClickSigninSyncStarter::StartSyncMode mode_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OneClickSigninBubbleViewBrowserTest);
};

// Disabled. See http://crbug.com/132348
IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleViewBrowserTest, DISABLED_Show) {
  ShowOneClickSigninBubble();
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(OneClickSigninBubbleView::IsShowing());

  OneClickSigninBubbleView::Hide();
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(on_start_sync_called_);
  EXPECT_EQ(OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS, mode_);
  EXPECT_FALSE(OneClickSigninBubbleView::IsShowing());
}

// Disabled. See http://crbug.com/132348
IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleViewBrowserTest, DISABLED_OkButton) {
  OneClickSigninBubbleView* view = ShowOneClickSigninBubble();

  // Simulate pressing the OK button.  Set the message loop in the bubble
  // view so that it can be quit once the bubble is hidden.
  views::ButtonListener* listener = view;
  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED,
                             gfx::Point(), gfx::Point(),
                             0);
  listener->ButtonPressed(view->ok_button_, event);

  // View should no longer be showing.  The message loop will exit once the
  // fade animation of the bubble is done.
  content::RunMessageLoop();
  EXPECT_TRUE(on_start_sync_called_);
  EXPECT_EQ(OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS, mode_);
  EXPECT_FALSE(OneClickSigninBubbleView::IsShowing());
}

// Disabled. See http://crbug.com/132348
IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleViewBrowserTest,
                       DISABLED_UndoButton) {
  OneClickSigninBubbleView* view = ShowOneClickSigninBubble();

  // Simulate pressing the undo button.  Set the message loop in the bubble
  // view so that it can be quit once the bubble is hidden.
  views::ButtonListener* listener = view;
  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED,
                             gfx::Point(), gfx::Point(),
                             0);
  listener->ButtonPressed(view->undo_button_, event);

  // View should no longer be showing.  The message loop will exit once the
  // fade animation of the bubble is done.
  content::RunMessageLoop();
  EXPECT_TRUE(on_start_sync_called_);
  EXPECT_EQ(OneClickSigninSyncStarter::UNDO_SYNC, mode_);
  EXPECT_FALSE(OneClickSigninBubbleView::IsShowing());
}

// Disabled. See http://crbug.com/132348
IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleViewBrowserTest,
                       DISABLED_AdvancedLink) {
  OneClickSigninBubbleView* view = ShowOneClickSigninBubble();

  // Simulate pressing a link in the bubble.  This should open a new tab.
  views::LinkListener* listener = view;
  listener->LinkClicked(view->advanced_link_, 0);

  // View should no longer be showing and a new tab should be opened.
  content::RunMessageLoop();
  EXPECT_TRUE(on_start_sync_called_);
  EXPECT_EQ(OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST, mode_);
  EXPECT_FALSE(OneClickSigninBubbleView::IsShowing());
}

// Disabled. See http://crbug.com/132348
IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleViewBrowserTest,
                       DISABLED_PressEnterKey) {
  OneClickSigninBubbleView* one_click_view = ShowOneClickSigninBubble();

  // Simulate pressing the Enter key.
  views::View* view = one_click_view;
  const ui::Accelerator accelerator(ui::VKEY_RETURN, 0);
  view->AcceleratorPressed(accelerator);

  // View should no longer be showing.  The message loop will exit once the
  // fade animation of the bubble is done.
  content::RunMessageLoop();
  EXPECT_TRUE(on_start_sync_called_);
  EXPECT_EQ(OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS, mode_);
  EXPECT_FALSE(OneClickSigninBubbleView::IsShowing());
}

// Disabled. See http://crbug.com/132348
IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleViewBrowserTest,
                       DISABLED_PressEscapeKey) {
  OneClickSigninBubbleView* one_click_view = ShowOneClickSigninBubble();

  // Simulate pressing the Escape key.
  views::View* view = one_click_view;
  const ui::Accelerator accelerator(ui::VKEY_ESCAPE, 0);
  view->AcceleratorPressed(accelerator);

  // View should no longer be showing.  The message loop will exit once the
  // fade animation of the bubble is done.
  content::RunMessageLoop();
  EXPECT_FALSE(on_start_sync_called_);
  EXPECT_FALSE(OneClickSigninBubbleView::IsShowing());
}
