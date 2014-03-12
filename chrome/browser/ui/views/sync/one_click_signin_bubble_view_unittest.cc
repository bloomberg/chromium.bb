// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sync/one_click_signin_bubble_view.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/sync/one_click_signin_bubble_delegate.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/page_transition_types.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

class OneClickSigninBubbleViewTest : public views::ViewsTestBase {
 public:
  OneClickSigninBubbleViewTest()
      : on_start_sync_called_(false),
        mode_(OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST),
        bubble_learn_more_click_count_(0),
        dialog_learn_more_click_count_(0),
        advanced_click_count_(0),
        anchor_widget_(NULL) {
  }

  virtual void SetUp() OVERRIDE {
    views::ViewsTestBase::SetUp();

    // Create a widget to host the anchor view.
    anchor_widget_ = new views::Widget;
    views::Widget::InitParams widget_params = CreateParams(
        views::Widget::InitParams::TYPE_WINDOW);
    anchor_widget_->Init(widget_params);
    anchor_widget_->Show();
  }

  virtual void TearDown() OVERRIDE {
    OneClickSigninBubbleView::Hide();
    anchor_widget_->Close();
    anchor_widget_ = NULL;
    views::ViewsTestBase::TearDown();
  }

 protected:
  OneClickSigninBubbleView* ShowOneClickSigninBubble(
    BrowserWindow::OneClickSigninBubbleType bubble_type) {

    scoped_ptr<OneClickSigninBubbleDelegate> delegate;
    delegate.reset(new OneClickSigninBubbleTestDelegate(this));

    OneClickSigninBubbleView::ShowBubble(
        bubble_type,
        base::string16(),
        base::string16(),
        delegate.Pass(),
        anchor_widget_->GetContentsView(),
        base::Bind(&OneClickSigninBubbleViewTest::OnStartSync,
                   base::Unretained(this)));

    OneClickSigninBubbleView* view =
        OneClickSigninBubbleView::view_for_testing();
    EXPECT_TRUE(view != NULL);
    return view;
  }

  void OnStartSync(OneClickSigninSyncStarter::StartSyncMode mode) {
    on_start_sync_called_ = true;
    mode_ = mode;
  }

  bool on_start_sync_called_;
  OneClickSigninSyncStarter::StartSyncMode mode_;
  int bubble_learn_more_click_count_;
  int dialog_learn_more_click_count_;
  int advanced_click_count_;

 private:
  friend class OneClickSigninBubbleTestDelegate;

  class OneClickSigninBubbleTestDelegate
      : public OneClickSigninBubbleDelegate {
   public:
    // |test| is not owned by this object.
    explicit OneClickSigninBubbleTestDelegate(
        OneClickSigninBubbleViewTest* test) : test_(test) {}

    // OneClickSigninBubbleDelegate:
    virtual void OnLearnMoreLinkClicked(bool is_dialog) OVERRIDE {
      if (is_dialog)
        ++test_->dialog_learn_more_click_count_;
      else
        ++test_->bubble_learn_more_click_count_;
    }
    virtual void OnAdvancedLinkClicked() OVERRIDE {
      ++test_->advanced_click_count_;
    }

   private:
    OneClickSigninBubbleViewTest* test_;

    DISALLOW_COPY_AND_ASSIGN(OneClickSigninBubbleTestDelegate);
  };

  // Widget to host the anchor view of the bubble. Destroys itself when closed.
  views::Widget* anchor_widget_;

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninBubbleViewTest);
};

TEST_F(OneClickSigninBubbleViewTest, ShowBubble) {
  ShowOneClickSigninBubble(BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE);
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(OneClickSigninBubbleView::IsShowing());
}

TEST_F(OneClickSigninBubbleViewTest, ShowDialog) {
  ShowOneClickSigninBubble(
    BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_MODAL_DIALOG);
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(OneClickSigninBubbleView::IsShowing());
}

TEST_F(OneClickSigninBubbleViewTest, HideBubble) {
  ShowOneClickSigninBubble(BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE);

  OneClickSigninBubbleView::Hide();
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(OneClickSigninBubbleView::IsShowing());
}

TEST_F(OneClickSigninBubbleViewTest, HideDialog) {
  ShowOneClickSigninBubble(
    BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_MODAL_DIALOG);

  OneClickSigninBubbleView::Hide();
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(OneClickSigninBubbleView::IsShowing());
  EXPECT_TRUE(on_start_sync_called_);
  EXPECT_EQ(OneClickSigninSyncStarter::UNDO_SYNC, mode_);
}

TEST_F(OneClickSigninBubbleViewTest, BubbleOkButton) {
  OneClickSigninBubbleView* view =
    ShowOneClickSigninBubble(
      BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE);

  // Simulate pressing the OK button.  Set the message loop in the bubble
  // view so that it can be quit once the bubble is hidden.
  views::ButtonListener* listener = view;
  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED,
                             gfx::Point(), gfx::Point(),
                             0, 0);
  listener->ButtonPressed(view->ok_button_, event);

  // View should no longer be showing.  The message loop will exit once the
  // fade animation of the bubble is done.
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(OneClickSigninBubbleView::IsShowing());
}

TEST_F(OneClickSigninBubbleViewTest, DialogOkButton) {
  OneClickSigninBubbleView* view = ShowOneClickSigninBubble(
      BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_MODAL_DIALOG);

  // Simulate pressing the OK button.  Set the message loop in the bubble
  // view so that it can be quit once the bubble is hidden.
  views::ButtonListener* listener = view;
  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED,
                             gfx::Point(), gfx::Point(),
                             0, 0);
  listener->ButtonPressed(view->ok_button_, event);

  // View should no longer be showing and sync should start
  // The message loop will exit once the fade animation of the dialog is done.
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(OneClickSigninBubbleView::IsShowing());
  EXPECT_TRUE(on_start_sync_called_);
  EXPECT_EQ(OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS, mode_);
}

TEST_F(OneClickSigninBubbleViewTest, DialogUndoButton) {
  OneClickSigninBubbleView* view = ShowOneClickSigninBubble(
    BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_MODAL_DIALOG);

  // Simulate pressing the undo button.  Set the message loop in the bubble
  // view so that it can be quit once the bubble is hidden.
  views::ButtonListener* listener = view;
  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED,
                             gfx::Point(), gfx::Point(),
                             0, 0);
  listener->ButtonPressed(view->undo_button_, event);

  // View should no longer be showing.  The message loop will exit once the
  // fade animation of the bubble is done.
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(OneClickSigninBubbleView::IsShowing());
  EXPECT_TRUE(on_start_sync_called_);
  EXPECT_EQ(OneClickSigninSyncStarter::UNDO_SYNC, mode_);
}

TEST_F(OneClickSigninBubbleViewTest, BubbleAdvancedLink) {
  OneClickSigninBubbleView* view = ShowOneClickSigninBubble(
    BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE);

  // Simulate pressing a link in the bubble.
  views::LinkListener* listener = view;
  listener->LinkClicked(view->advanced_link_, 0);

  // View should no longer be showing and the OnAdvancedLinkClicked method
  // of the delegate should have been called.
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(OneClickSigninBubbleView::IsShowing());
  EXPECT_EQ(1, advanced_click_count_);
}

TEST_F(OneClickSigninBubbleViewTest, DialogAdvancedLink) {
  OneClickSigninBubbleView* view = ShowOneClickSigninBubble(
    BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_MODAL_DIALOG);

  // Simulate pressing a link in the bubble.
  views::LinkListener* listener = view;
  listener->LinkClicked(view->advanced_link_, 0);

  // View should no longer be showing. No delegate method should have been
  // called: the callback is responsible to open the settings page.
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(on_start_sync_called_);
  EXPECT_EQ(OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST, mode_);
  EXPECT_FALSE(OneClickSigninBubbleView::IsShowing());
  EXPECT_EQ(0, advanced_click_count_);
}

TEST_F(OneClickSigninBubbleViewTest, BubbleLearnMoreLink) {
  OneClickSigninBubbleView* view = ShowOneClickSigninBubble(
    BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE);

  views::LinkListener* listener = view;
  listener->LinkClicked(view->learn_more_link_, 0);

  // View should no longer be showing and the OnLearnMoreLinkClicked method
  // of the delegate should have been called with |is_dialog| == false.
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(OneClickSigninBubbleView::IsShowing());
  EXPECT_EQ(1, bubble_learn_more_click_count_);
  EXPECT_EQ(0, dialog_learn_more_click_count_);
}

TEST_F(OneClickSigninBubbleViewTest, DialogLearnMoreLink) {
  OneClickSigninBubbleView* view = ShowOneClickSigninBubble(
      BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_MODAL_DIALOG);

  views::LinkListener* listener = view;
  listener->LinkClicked(view->learn_more_link_, 0);

  // View should still be showing and the OnLearnMoreLinkClicked method
  // of the delegate should have been called with |is_dialog| == true.
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(OneClickSigninBubbleView::IsShowing());
  EXPECT_EQ(0, bubble_learn_more_click_count_);
  EXPECT_EQ(1, dialog_learn_more_click_count_);
}

TEST_F(OneClickSigninBubbleViewTest, BubblePressEnterKey) {
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

TEST_F(OneClickSigninBubbleViewTest, DialogPressEnterKey) {
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

TEST_F(OneClickSigninBubbleViewTest, BubblePressEscapeKey) {
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

TEST_F(OneClickSigninBubbleViewTest, DialogPressEscapeKey) {
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
  EXPECT_TRUE(on_start_sync_called_);
  EXPECT_EQ(OneClickSigninSyncStarter::UNDO_SYNC, mode_);
}
