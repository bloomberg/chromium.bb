// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/toolbar/test_toolbar_actions_bar_bubble_delegate.h"
#include "chrome/browser/ui/views/toolbar/toolbar_actions_bar_bubble_views.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/test_widget_observer.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace {

gfx::Point GetCenterInScreenCoordinates(const views::View* view) {
  gfx::Point center(view->width() / 2, view->height() / 2);
  views::View::ConvertPointToScreen(view, &center);
  return center;
}

}  // namespace

class ToolbarActionsBarBubbleViewsTest : public views::ViewsTestBase {
 protected:
  scoped_ptr<views::Widget> CreateAnchorWidget() {
    scoped_ptr<views::Widget> anchor_widget(new views::Widget());
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    anchor_widget->Init(params);
    return anchor_widget;
  }

  base::string16 HeadingString() { return base::ASCIIToUTF16("Heading"); }
  base::string16 BodyString() { return base::ASCIIToUTF16("Body"); }
  base::string16 ActionString() { return base::ASCIIToUTF16("Action"); }
  base::string16 DismissString() { return base::ASCIIToUTF16("Dismiss"); }
};

TEST_F(ToolbarActionsBarBubbleViewsTest, TestBubbleLayoutActionButton) {
  scoped_ptr<views::Widget> anchor_widget = CreateAnchorWidget();
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  ToolbarActionsBarBubbleViews* bubble = new ToolbarActionsBarBubbleViews(
      anchor_widget->GetContentsView(), delegate.GetDelegate());
  views::BubbleDelegateView::CreateBubble(bubble);
  bubble->Show();

  EXPECT_TRUE(bubble->heading_label());
  EXPECT_EQ(HeadingString(), bubble->heading_label()->text());
  EXPECT_TRUE(bubble->content_label());
  EXPECT_EQ(BodyString(), bubble->content_label()->text());
  EXPECT_TRUE(bubble->action_button());
  EXPECT_EQ(ActionString(), bubble->action_button()->GetText());
  EXPECT_FALSE(bubble->dismiss_button());

  bubble->GetWidget()->Close();
  base::RunLoop().RunUntilIdle();
}

TEST_F(ToolbarActionsBarBubbleViewsTest,
       TestBubbleLayoutActionAndDismissButton) {
  scoped_ptr<views::Widget> anchor_widget = CreateAnchorWidget();
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  delegate.set_dismiss_button_text(DismissString());
  ToolbarActionsBarBubbleViews* bubble = new ToolbarActionsBarBubbleViews(
      anchor_widget->GetContentsView(), delegate.GetDelegate());
  views::BubbleDelegateView::CreateBubble(bubble);
  bubble->Show();

  EXPECT_TRUE(bubble->heading_label());
  EXPECT_EQ(HeadingString(), bubble->heading_label()->text());
  EXPECT_TRUE(bubble->content_label());
  EXPECT_EQ(BodyString(), bubble->content_label()->text());
  EXPECT_TRUE(bubble->action_button());
  EXPECT_EQ(ActionString(), bubble->action_button()->GetText());
  EXPECT_TRUE(bubble->dismiss_button());
  EXPECT_EQ(DismissString(), bubble->dismiss_button()->GetText());

  bubble->GetWidget()->Close();
  base::RunLoop().RunUntilIdle();
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestShowAndCloseBubble) {
  scoped_ptr<views::Widget> anchor_widget = CreateAnchorWidget();
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  delegate.set_dismiss_button_text(DismissString());
  ToolbarActionsBarBubbleViews* bubble = new ToolbarActionsBarBubbleViews(
      anchor_widget->GetContentsView(), delegate.GetDelegate());

  EXPECT_FALSE(delegate.shown());
  EXPECT_FALSE(delegate.close_action());
  views::Widget* bubble_widget =
      views::BubbleDelegateView::CreateBubble(bubble);
  views::test::TestWidgetObserver bubble_observer(bubble_widget);
  bubble->Show();
  EXPECT_TRUE(delegate.shown());
  EXPECT_FALSE(delegate.close_action());

  bubble_widget->Close();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.close_action());
  EXPECT_EQ(ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS_USER_ACTION,
            *delegate.close_action());
  EXPECT_TRUE(bubble_observer.widget_closed());
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestClickActionButton) {
  scoped_ptr<views::Widget> anchor_widget = CreateAnchorWidget();
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  delegate.set_dismiss_button_text(DismissString());
  ToolbarActionsBarBubbleViews* bubble = new ToolbarActionsBarBubbleViews(
      anchor_widget->GetContentsView(), delegate.GetDelegate());
  views::Widget* bubble_widget =
      views::BubbleDelegateView::CreateBubble(bubble);
  views::test::TestWidgetObserver bubble_observer(bubble_widget);
  bubble->Show();

  EXPECT_FALSE(delegate.close_action());
  ui::test::EventGenerator generator(GetContext(),
                                     anchor_widget->GetNativeWindow());
  generator.MoveMouseTo(GetCenterInScreenCoordinates(bubble->action_button()));
  generator.ClickLeftButton();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.close_action());
  EXPECT_EQ(ToolbarActionsBarBubbleDelegate::CLOSE_EXECUTE,
            *delegate.close_action());
  EXPECT_TRUE(bubble_observer.widget_closed());
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestCloseOnDeactivation) {
  scoped_ptr<views::Widget> anchor_widget = CreateAnchorWidget();
  TestToolbarActionsBarBubbleDelegate delegate(HeadingString(), BodyString(),
                                               ActionString());
  delegate.set_dismiss_button_text(DismissString());
  ToolbarActionsBarBubbleViews* bubble = new ToolbarActionsBarBubbleViews(
      anchor_widget->GetContentsView(), delegate.GetDelegate());
  views::Widget* bubble_widget =
      views::BubbleDelegateView::CreateBubble(bubble);
  views::test::TestWidgetObserver bubble_observer(bubble_widget);
  bubble->Show();

  EXPECT_FALSE(delegate.close_action());
  // Close the bubble by activating another widget. The delegate should be
  // told it was dismissed.
  anchor_widget->Activate();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.close_action());
  EXPECT_EQ(ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS_DEACTIVATION,
            *delegate.close_action());
  EXPECT_TRUE(bubble_observer.widget_closed());
}
