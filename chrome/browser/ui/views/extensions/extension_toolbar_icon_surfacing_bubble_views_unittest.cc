// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_bubble_delegate.h"
#include "chrome/browser/ui/views/extensions/extension_toolbar_icon_surfacing_bubble_views.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/test_widget_observer.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace {

using ExtensionToolbarIconSurfacingBubbleTest = views::ViewsTestBase;

class TestDelegate : public ToolbarActionsBarBubbleDelegate {
 public:
  TestDelegate() {}
  ~TestDelegate() {}

  void OnToolbarActionsBarBubbleClosed(CloseAction action) override {
    EXPECT_FALSE(action_);
    action_.reset(new CloseAction(action));
  }

  CloseAction* action() const { return action_.get(); }

 private:
  scoped_ptr<CloseAction> action_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

}  // namespace

TEST_F(ExtensionToolbarIconSurfacingBubbleTest,
       ExtensionToolbarIconSurfacingBubbleTest) {
  scoped_ptr<views::Widget> anchor_widget(new views::Widget());
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  anchor_widget->Init(params);

  {
    // Test clicking on the button.
    TestDelegate delegate;
    ExtensionToolbarIconSurfacingBubble* bubble =
        new ExtensionToolbarIconSurfacingBubble(
            anchor_widget->GetContentsView(),
            &delegate);
    views::Widget* bubble_widget =
        views::BubbleDelegateView::CreateBubble(bubble);
    bubble_widget->Show();
    views::test::TestWidgetObserver bubble_observer(bubble_widget);
    EXPECT_FALSE(delegate.action());

    // Find the button and click on it.
    views::View* button = nullptr;
    for (int i = 0; i < bubble->child_count(); ++i) {
      if (bubble->child_at(i)->GetClassName() ==
              views::LabelButton::kViewClassName) {
        button = bubble->child_at(i);
        break;
      }
    }
    ASSERT_TRUE(button);
    button->OnMouseReleased(ui::MouseEvent(
        ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
    base::RunLoop().RunUntilIdle();

    // The bubble should be closed, and the delegate should be told that the
    // button was clicked.
    ASSERT_TRUE(delegate.action());
    EXPECT_EQ(ToolbarActionsBarBubbleDelegate::ACKNOWLEDGED,
              *delegate.action());
    EXPECT_TRUE(bubble_observer.widget_closed());
  }

  {
    // Test dismissing the bubble without clicking the button.
    TestDelegate delegate;
    ExtensionToolbarIconSurfacingBubble* bubble =
        new ExtensionToolbarIconSurfacingBubble(
            anchor_widget->GetContentsView(),
            &delegate);
    views::Widget* bubble_widget =
        views::BubbleDelegateView::CreateBubble(bubble);
    bubble_widget->Show();
    views::test::TestWidgetObserver bubble_observer(bubble_widget);
    EXPECT_FALSE(delegate.action());

    // Close the bubble. The delegate should be told it was dismissed.
    bubble_widget->Close();
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(delegate.action());
    EXPECT_EQ(ToolbarActionsBarBubbleDelegate::DISMISSED, *delegate.action());
    EXPECT_TRUE(bubble_observer.widget_closed());
  }
}
