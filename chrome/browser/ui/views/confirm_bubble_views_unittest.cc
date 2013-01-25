// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/confirm_bubble_views.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/confirm_bubble.h"
#include "chrome/browser/ui/confirm_bubble_model.h"
#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

using views::Widget;

class TestConfirmBubbleModel : public ConfirmBubbleModel {
 public:
  TestConfirmBubbleModel() {}
  virtual ~TestConfirmBubbleModel() {}

  virtual string16 GetTitle() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual string16 GetButtonLabel(BubbleButton button) const OVERRIDE;
  virtual void Accept() OVERRIDE {}
  virtual void Cancel() OVERRIDE {}
  virtual string16 GetLinkText() const OVERRIDE;
  virtual void LinkClicked() OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestConfirmBubbleModel);
};

string16 TestConfirmBubbleModel::GetTitle() const {
  return ASCIIToUTF16("Title");
}

string16 TestConfirmBubbleModel::GetMessageText() const {
  return ASCIIToUTF16("Message");
}

gfx::Image* TestConfirmBubbleModel::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_PRODUCT_LOGO_16);
}

int TestConfirmBubbleModel::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

string16 TestConfirmBubbleModel::GetButtonLabel(BubbleButton button) const {
  return button == BUTTON_OK ? ASCIIToUTF16("OK") : ASCIIToUTF16("Cancel");
}

string16 TestConfirmBubbleModel::GetLinkText() const {
  return ASCIIToUTF16("Link");
}

///////////////////////////////////////////////////////////////////////////////

typedef views::ViewsTestBase ConfirmBubbleViewsTest;

TEST_F(ConfirmBubbleViewsTest, CreateAndClose) {
  // Create parent widget, as confirm bubble must have an owner.
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  scoped_ptr<views::Widget> parent_widget(new Widget);
  parent_widget->Init(params);
  parent_widget->Show();

  // Bubble owns the model.
  ConfirmBubbleViews* bubble =
      new ConfirmBubbleViews(parent_widget->GetNativeView(),
                             gfx::Point(12, 34),
                             new TestConfirmBubbleModel);
  views::BubbleDelegateView::CreateBubble(bubble);
  bubble->Show();

  // We're anchored to a point, not a specific view or widget.
  EXPECT_EQ("12,34", bubble->anchor_point().ToString());
  EXPECT_FALSE(bubble->anchor_view());
  EXPECT_FALSE(bubble->anchor_widget());

  // Clean up.
  bubble->GetWidget()->CloseNow();
  parent_widget->CloseNow();
}
