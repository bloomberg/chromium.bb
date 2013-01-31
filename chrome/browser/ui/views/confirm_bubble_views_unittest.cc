// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/confirm_bubble_views.h"

#include "chrome/browser/ui/confirm_bubble.h"
#include "chrome/browser/ui/test/test_confirm_bubble_model.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

using views::Widget;

typedef views::ViewsTestBase ConfirmBubbleViewsTest;

TEST_F(ConfirmBubbleViewsTest, CreateAndClose) {
  // Create parent widget, as confirm bubble must have an owner.
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  scoped_ptr<views::Widget> parent_widget(new Widget);
  parent_widget->Init(params);
  parent_widget->Show();

  // Bubble owns the model.
  bool model_deleted = false;
  TestConfirmBubbleModel* model =
      new TestConfirmBubbleModel(&model_deleted, NULL, NULL, NULL);
  ConfirmBubbleViews* bubble =
      new ConfirmBubbleViews(parent_widget->GetNativeView(),
                             gfx::Point(12, 34),
                             model);
  views::BubbleDelegateView::CreateBubble(bubble);
  bubble->Show();

  // We're anchored to a point, not a specific view or widget.
  EXPECT_EQ("12,34", bubble->anchor_point().ToString());
  EXPECT_FALSE(bubble->anchor_view());
  EXPECT_FALSE(bubble->anchor_widget());

  // Clean up.
  bubble->GetWidget()->CloseNow();
  parent_widget->CloseNow();
  EXPECT_TRUE(model_deleted);
}
