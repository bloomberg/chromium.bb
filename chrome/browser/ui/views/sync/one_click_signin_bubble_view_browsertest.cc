// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sync/one_click_signin_bubble_view.h"

#include "base/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/page_transition_types.h"
#include "ui/views/controls/button/label_button.h"

class OneClickSigninBubbleViewBrowserTest : public InProcessBrowserTest {
 public:
  OneClickSigninBubbleViewBrowserTest()
      : on_start_sync_called_(false),
        mode_(OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST) {
  }

  OneClickSigninBubbleView* ShowOneClickSigninBubble(
    BrowserWindow::OneClickSigninBubbleType bubble_type) {
    browser()->window()->ShowOneClickSigninBubble(
        bubble_type,
        string16(),
        string16(),
        base::Bind(&OneClickSigninBubbleViewBrowserTest::OnStartSync, this));

    OneClickSigninBubbleView* view =
        OneClickSigninBubbleView::view_for_testing();
    EXPECT_TRUE(view != NULL);
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

IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleViewBrowserTest, ShowBubble) {
  ShowOneClickSigninBubble(BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE);
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(OneClickSigninBubbleView::IsShowing());
}

IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleViewBrowserTest, ShowDialog) {
  ShowOneClickSigninBubble(
    BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_MODAL_DIALOG);
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(OneClickSigninBubbleView::IsShowing());
}

IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleViewBrowserTest, HideBubble) {
  ShowOneClickSigninBubble(BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE);

  OneClickSigninBubbleView::Hide();
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(OneClickSigninBubbleView::IsShowing());
}

IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleViewBrowserTest, HideDialog) {
  ShowOneClickSigninBubble(
    BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_MODAL_DIALOG);

  OneClickSigninBubbleView::Hide();
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(OneClickSigninBubbleView::IsShowing());
  EXPECT_TRUE(on_start_sync_called_);
  EXPECT_EQ(OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS, mode_);
}

IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleViewBrowserTest, BubbleOkButton) {
  OneClickSigninBubbleView* view =
    ShowOneClickSigninBubble(
      BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE);

  // Simulate pressing the OK button.  Set the message loop in the bubble
  // view so that it can be quit once the bubble is hidden.
  views::ButtonListener* listener = view;
  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED,
                             gfx::Point(), gfx::Point(),
                             0);
  listener->ButtonPressed(view->ok_button_, event);

  // View should no longer be showing.  The message loop will exit once the
  // fade animation of the bubble is done.
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(OneClickSigninBubbleView::IsShowing());
}

IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleViewBrowserTest, DialogOkButton) {
  OneClickSigninBubbleView* view = ShowOneClickSigninBubble(
      BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_MODAL_DIALOG);

  // Simulate pressing the OK button.  Set the message loop in the bubble
  // view so that it can be quit once the bubble is hidden.
  views::ButtonListener* listener = view;
  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED,
                             gfx::Point(), gfx::Point(),
                             0);
  listener->ButtonPressed(view->ok_button_, event);

  // View should no longer be showing and sync should start
  // The message loop will exit once the fade animation of the dialog is done.
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(OneClickSigninBubbleView::IsShowing());
  EXPECT_TRUE(on_start_sync_called_);
  EXPECT_EQ(OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS, mode_);
}

IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleViewBrowserTest,
                       DialogUndoButton) {
  OneClickSigninBubbleView* view = ShowOneClickSigninBubble(
    BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_MODAL_DIALOG);

  // Simulate pressing the undo button.  Set the message loop in the bubble
  // view so that it can be quit once the bubble is hidden.
  views::ButtonListener* listener = view;
  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED,
                             gfx::Point(), gfx::Point(),
                             0);
  listener->ButtonPressed(view->undo_button_, event);

  // View should no longer be showing.  The message loop will exit once the
  // fade animation of the bubble is done.
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(OneClickSigninBubbleView::IsShowing());
  EXPECT_TRUE(on_start_sync_called_);
  EXPECT_EQ(OneClickSigninSyncStarter::UNDO_SYNC, mode_);
}

IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleViewBrowserTest,
                       BubbleAdvancedLink) {
  int starting_tab_count = browser()->tab_strip_model()->count();

  OneClickSigninBubbleView* view = ShowOneClickSigninBubble(
    BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE);

  // Simulate pressing a link in the bubble.This should replace the current tab.
  views::LinkListener* listener = view;
  listener->LinkClicked(view->advanced_link_, 0);

  // View should no longer be showing and the current tab should be replaced.
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(OneClickSigninBubbleView::IsShowing());
  int tab_count = browser()->tab_strip_model()->count();
  EXPECT_EQ(starting_tab_count, tab_count);
}

IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleViewBrowserTest,
                       DialogAdvancedLink) {
  int starting_tab_count = browser()->tab_strip_model()->count();

  OneClickSigninBubbleView* view = ShowOneClickSigninBubble(
    BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_MODAL_DIALOG);

  // Simulate pressing a link in the bubble.  This should open a new tab.
  views::LinkListener* listener = view;
  listener->LinkClicked(view->advanced_link_, 0);

  // View should no longer be showing and a new tab should be opened.
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(on_start_sync_called_);
  EXPECT_EQ(OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST, mode_);
  EXPECT_FALSE(OneClickSigninBubbleView::IsShowing());
  int tab_count = browser()->tab_strip_model()->count();
  EXPECT_EQ(starting_tab_count, tab_count);
}

IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleViewBrowserTest,
                       BubbleLearnMoreLink) {
  int starting_tab_count = browser()->tab_strip_model()->count();

  OneClickSigninBubbleView* view = ShowOneClickSigninBubble(
    BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE);

  // View should no longer be showing and a new tab should be added.
  views::LinkListener* listener = view;
  listener->LinkClicked(view->learn_more_link_, 0);

  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(OneClickSigninBubbleView::IsShowing());
  int tab_count = browser()->tab_strip_model()->count();
  EXPECT_EQ(starting_tab_count + 1, tab_count);
}

IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleViewBrowserTest,
                       BubblePressEnterKey) {
  OneClickSigninBubbleView* one_click_view = ShowOneClickSigninBubble(
    BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE);

  // Simulate pressing the Enter key.
  views::View* view = one_click_view;
  const ui::Accelerator accelerator(ui::VKEY_RETURN, 0);
  view->AcceleratorPressed(accelerator);

  // View should no longer be showing.  The message loop will exit once the
  // fade animation of the bubble is done.
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(OneClickSigninBubbleView::IsShowing());
}

IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleViewBrowserTest,
                       DialogPressEnterKey) {
  OneClickSigninBubbleView* one_click_view = ShowOneClickSigninBubble(
    BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_MODAL_DIALOG);

  // Simulate pressing the Enter key.
  views::View* view = one_click_view;
  const ui::Accelerator accelerator(ui::VKEY_RETURN, 0);
  view->AcceleratorPressed(accelerator);

  // View should no longer be showing.  The message loop will exit once the
  // fade animation of the bubble is done.
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(OneClickSigninBubbleView::IsShowing());
  EXPECT_TRUE(on_start_sync_called_);
  EXPECT_EQ(OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS, mode_);
}

IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleViewBrowserTest,
                       BubblePressEscapeKey) {
  OneClickSigninBubbleView* one_click_view = ShowOneClickSigninBubble(
    BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE);

  // Simulate pressing the Escape key.
  views::View* view = one_click_view;
  const ui::Accelerator accelerator(ui::VKEY_ESCAPE, 0);
  view->AcceleratorPressed(accelerator);

  // View should no longer be showing.  The message loop will exit once the
  // fade animation of the bubble is done.
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(OneClickSigninBubbleView::IsShowing());
}

IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleViewBrowserTest,
                       DialogPressEscapeKey) {
  OneClickSigninBubbleView* one_click_view = ShowOneClickSigninBubble(
    BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_MODAL_DIALOG);

  // Simulate pressing the Escape key.
  views::View* view = one_click_view;
  const ui::Accelerator accelerator(ui::VKEY_ESCAPE, 0);
  view->AcceleratorPressed(accelerator);

  // View should no longer be showing.  The message loop will exit once the
  // fade animation of the bubble is done.
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(OneClickSigninBubbleView::IsShowing());
  EXPECT_FALSE(on_start_sync_called_);
}
