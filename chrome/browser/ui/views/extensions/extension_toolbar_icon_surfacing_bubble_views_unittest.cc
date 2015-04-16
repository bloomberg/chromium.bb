// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/toolbar/test_toolbar_actions_bar_bubble_delegate.h"
#include "chrome/browser/ui/views/extensions/extension_toolbar_icon_surfacing_bubble_views.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/test_widget_observer.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

using ExtensionToolbarIconSurfacingBubbleTest = views::ViewsTestBase;

TEST_F(ExtensionToolbarIconSurfacingBubbleTest,
       ExtensionToolbarIconSurfacingBubbleTest) {
  const base::string16 kHeadingString = base::ASCIIToUTF16("Heading");
  const base::string16 kBodyString = base::ASCIIToUTF16("Body");
  const base::string16 kActionString = base::ASCIIToUTF16("Action");

  scoped_ptr<views::Widget> anchor_widget(new views::Widget());
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  anchor_widget->Init(params);

  {
    // Test clicking on the button.
    TestToolbarActionsBarBubbleDelegate delegate(
        kHeadingString, kBodyString, kActionString);
    ExtensionToolbarIconSurfacingBubble* bubble =
        new ExtensionToolbarIconSurfacingBubble(
            anchor_widget->GetContentsView(),
            delegate.GetDelegate());
    views::Widget* bubble_widget =
        views::BubbleDelegateView::CreateBubble(bubble);
    EXPECT_FALSE(delegate.shown());
    bubble->Show();
    EXPECT_TRUE(delegate.shown());
    views::test::TestWidgetObserver bubble_observer(bubble_widget);
    EXPECT_FALSE(delegate.close_action());

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
    ASSERT_TRUE(delegate.close_action());
    EXPECT_EQ(ToolbarActionsBarBubbleDelegate::CLOSE_EXECUTE,
              *delegate.close_action());
    EXPECT_TRUE(bubble_observer.widget_closed());
  }

  {
    // Test dismissing the bubble without clicking the button.
    TestToolbarActionsBarBubbleDelegate delegate(
        kHeadingString, kBodyString, kActionString);
    ExtensionToolbarIconSurfacingBubble* bubble =
        new ExtensionToolbarIconSurfacingBubble(
            anchor_widget->GetContentsView(),
            delegate.GetDelegate());
    views::Widget* bubble_widget =
        views::BubbleDelegateView::CreateBubble(bubble);
    bubble->Show();
    views::test::TestWidgetObserver bubble_observer(bubble_widget);
    EXPECT_FALSE(delegate.close_action());

    // Close the bubble. The delegate should be told it was dismissed.
    bubble_widget->Close();
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(delegate.close_action());
    EXPECT_EQ(ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS,
              *delegate.close_action());
    EXPECT_TRUE(bubble_observer.widget_closed());
  }
}
