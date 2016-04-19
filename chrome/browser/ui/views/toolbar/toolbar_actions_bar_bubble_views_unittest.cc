// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_actions_bar_bubble_views.h"

#include <memory>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/toolbar/test_toolbar_actions_bar_bubble_delegate.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/test/test_widget_observer.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

class ToolbarActionsBarBubbleViewsTest : public views::ViewsTestBase {
 protected:
  ToolbarActionsBarBubbleViewsTest() {}
  ~ToolbarActionsBarBubbleViewsTest() override {}

  void TearDown() override {
    anchor_widget_.reset();
    views::ViewsTestBase::TearDown();
  }

  std::unique_ptr<views::Widget> CreateAnchorWidget() {
    std::unique_ptr<views::Widget> anchor_widget(new views::Widget());
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    anchor_widget->Init(params);
    return anchor_widget;
  }

  void ShowBubble(TestToolbarActionsBarBubbleDelegate* delegate) {
    ASSERT_TRUE(delegate);
    ASSERT_FALSE(bubble_widget_);
    ASSERT_FALSE(bubble_);
    anchor_widget_ = CreateAnchorWidget();
    bubble_ = new ToolbarActionsBarBubbleViews(
      anchor_widget_->GetContentsView(), delegate->GetDelegate());
    bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_);
    bubble_->Show();
  }

  void CloseBubble() {
    ASSERT_TRUE(bubble_);
    bubble_->GetWidget()->Close();
    base::RunLoop().RunUntilIdle();
    bubble_ = nullptr;
    bubble_widget_ = nullptr;
  }

  void ClickView(const views::View* view) {
    ASSERT_TRUE(view);
    ui::test::EventGenerator generator(GetContext(),
                                       anchor_widget_->GetNativeWindow());
    generator.MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
    generator.ClickLeftButton();
    base::RunLoop().RunUntilIdle();
  }

  base::string16 HeadingString() { return base::ASCIIToUTF16("Heading"); }
  base::string16 BodyString() { return base::ASCIIToUTF16("Body"); }
  base::string16 ActionString() { return base::ASCIIToUTF16("Action"); }
  base::string16 DismissString() { return base::ASCIIToUTF16("Dismiss"); }
  base::string16 LearnMoreString() { return base::ASCIIToUTF16("Learn"); }
  base::string16 ItemListString() {
    return base::ASCIIToUTF16("Item 1\nItem2");
  }

  views::Widget* anchor_widget() { return anchor_widget_.get(); }
  views::Widget* bubble_widget() { return bubble_widget_; }
  ToolbarActionsBarBubbleViews* bubble() { return bubble_; }

 private:
  std::unique_ptr<views::Widget> anchor_widget_;
  views::Widget* bubble_widget_ = nullptr;
  ToolbarActionsBarBubbleViews* bubble_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ToolbarActionsBarBubbleViewsTest);
};

TEST_F(ToolbarActionsBarBubbleViewsTest, TestBubbleLayoutActionButton) {
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  ShowBubble(&delegate);

  EXPECT_TRUE(bubble()->GetDialogClientView()->ok_button());
  EXPECT_EQ(ActionString(),
            bubble()->GetDialogClientView()->ok_button()->GetText());
  EXPECT_FALSE(bubble()->GetDialogClientView()->cancel_button());

  CloseBubble();
}

TEST_F(ToolbarActionsBarBubbleViewsTest,
       TestBubbleLayoutActionAndDismissButton) {
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  delegate.set_dismiss_button_text(DismissString());

  ShowBubble(&delegate);

  EXPECT_TRUE(bubble()->GetDialogClientView()->ok_button());
  EXPECT_EQ(ActionString(),
            bubble()->GetDialogClientView()->ok_button()->GetText());
  EXPECT_TRUE(bubble()->GetDialogClientView()->cancel_button());
  EXPECT_EQ(DismissString(),
            bubble()->GetDialogClientView()->cancel_button()->GetText());

  EXPECT_FALSE(bubble()->learn_more_button());
  EXPECT_FALSE(bubble()->item_list());

  CloseBubble();
}

TEST_F(ToolbarActionsBarBubbleViewsTest,
       TestBubbleLayoutActionDismissAndLearnMoreButton) {
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  delegate.set_dismiss_button_text(DismissString());
  delegate.set_learn_more_button_text(LearnMoreString());
  ShowBubble(&delegate);

  EXPECT_TRUE(bubble()->GetDialogClientView()->ok_button());
  EXPECT_EQ(ActionString(),
            bubble()->GetDialogClientView()->ok_button()->GetText());
  EXPECT_TRUE(bubble()->GetDialogClientView()->cancel_button());
  EXPECT_EQ(DismissString(),
            bubble()->GetDialogClientView()->cancel_button()->GetText());
  EXPECT_TRUE(bubble()->learn_more_button());
  EXPECT_EQ(LearnMoreString(), bubble()->learn_more_button()->text());
  EXPECT_FALSE(bubble()->item_list());

  CloseBubble();
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestBubbleLayoutListView) {
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  delegate.set_item_list_text(ItemListString());
  ShowBubble(&delegate);

  EXPECT_TRUE(bubble()->GetDialogClientView()->ok_button());
  EXPECT_EQ(ActionString(),
            bubble()->GetDialogClientView()->ok_button()->GetText());
  EXPECT_FALSE(bubble()->GetDialogClientView()->cancel_button());
  EXPECT_FALSE(bubble()->learn_more_button());
  EXPECT_TRUE(bubble()->item_list());
  EXPECT_EQ(ItemListString(), bubble()->item_list()->text());

  CloseBubble();
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestShowAndCloseBubble) {
  std::unique_ptr<views::Widget> anchor_widget = CreateAnchorWidget();
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  delegate.set_dismiss_button_text(DismissString());
  ToolbarActionsBarBubbleViews* bubble = new ToolbarActionsBarBubbleViews(
      anchor_widget->GetContentsView(), delegate.GetDelegate());

  EXPECT_FALSE(delegate.shown());
  EXPECT_FALSE(delegate.close_action());
  views::Widget* bubble_widget =
      views::BubbleDialogDelegateView::CreateBubble(bubble);
  views::test::TestWidgetObserver bubble_observer(bubble_widget);
  bubble->Show();
  EXPECT_TRUE(delegate.shown());
  EXPECT_FALSE(delegate.close_action());

  bubble->GetDialogClientView()->CancelWindow();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.close_action());
  EXPECT_EQ(ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS_USER_ACTION,
            *delegate.close_action());
  EXPECT_TRUE(bubble_observer.widget_closed());
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestClickActionButton) {
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  delegate.set_dismiss_button_text(DismissString());
  delegate.set_learn_more_button_text(LearnMoreString());
  ShowBubble(&delegate);
  views::test::TestWidgetObserver bubble_observer(bubble_widget());

  EXPECT_FALSE(delegate.close_action());
  ClickView(bubble()->GetDialogClientView()->ok_button());
  ASSERT_TRUE(delegate.close_action());
  EXPECT_EQ(ToolbarActionsBarBubbleDelegate::CLOSE_EXECUTE,
            *delegate.close_action());
  EXPECT_TRUE(bubble_observer.widget_closed());
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestClickDismissButton) {
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  delegate.set_dismiss_button_text(DismissString());
  delegate.set_learn_more_button_text(LearnMoreString());
  ShowBubble(&delegate);
  views::test::TestWidgetObserver bubble_observer(bubble_widget());

  EXPECT_FALSE(delegate.close_action());
  ClickView(bubble()->GetDialogClientView()->cancel_button());
  ASSERT_TRUE(delegate.close_action());
  EXPECT_EQ(ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS_USER_ACTION,
            *delegate.close_action());
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestClickLearnMoreLink) {
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  delegate.set_dismiss_button_text(DismissString());
  delegate.set_learn_more_button_text(LearnMoreString());
  ShowBubble(&delegate);
  views::test::TestWidgetObserver bubble_observer(bubble_widget());

  EXPECT_FALSE(delegate.close_action());
  ClickView(bubble()->learn_more_button());
  ASSERT_TRUE(delegate.close_action());
  EXPECT_EQ(ToolbarActionsBarBubbleDelegate::CLOSE_LEARN_MORE,
            *delegate.close_action());
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestCloseOnDeactivation) {
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  ShowBubble(&delegate);
  views::test::TestWidgetObserver bubble_observer(bubble_widget());

  EXPECT_FALSE(delegate.close_action());
  // Close the bubble by activating another widget. The delegate should be
  // told it was dismissed.
  anchor_widget()->Activate();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.close_action());
  EXPECT_EQ(ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS_DEACTIVATION,
            *delegate.close_action());
  EXPECT_TRUE(bubble_observer.widget_closed());
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestDontCloseOnDeactivation) {
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  delegate.set_close_on_deactivate(false);
  ShowBubble(&delegate);
  views::test::TestWidgetObserver bubble_observer(bubble_widget());

  EXPECT_FALSE(delegate.close_action());
  // Activate another widget. The bubble shouldn't close.
  anchor_widget()->Activate();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(delegate.close_action());
  CloseBubble();
}
