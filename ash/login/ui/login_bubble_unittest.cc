// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/login/ui/login_bubble.h"
#include "ash/login/ui/login_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Total width of the bubble view.
constexpr int kBubbleTotalWidthDp = 176;

// Horizontal margin of the bubble view.
constexpr int kBubbleHorizontalMarginDp = 14;

// Top margin of the bubble view.
constexpr int kBubbleTopMarginDp = 13;

// Bottom margin of the bubble view.
constexpr int kBubbleBottomMarginDp = 18;

// Non zero size for the bubble anchor view.
constexpr int kBubbleAnchorViewSizeDp = 100;

class LoginBubbleTest : public LoginTestBase {
 protected:
  LoginBubbleTest() = default;
  ~LoginBubbleTest() override = default;

  // LoginTestBase:
  void SetUp() override {
    LoginTestBase::SetUp();

    container_ = new views::View();
    container_->SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical));

    bubble_opener_ = new views::View();
    other_view_ = new views::View();
    bubble_opener_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    other_view_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    other_view_->SetPreferredSize(
        gfx::Size(kBubbleAnchorViewSizeDp, kBubbleAnchorViewSizeDp));
    bubble_opener_->SetPreferredSize(
        gfx::Size(kBubbleAnchorViewSizeDp, kBubbleAnchorViewSizeDp));

    container_->AddChildView(bubble_opener_);
    container_->AddChildView(other_view_);
    SetWidget(CreateWidgetWithContent(container_));

    bubble_ = std::make_unique<LoginBubble>();
  }

  void TearDown() override {
    bubble_.reset();
    LoginTestBase::TearDown();
  }

  // Owned by test widget view hierarchy.
  views::View* container_ = nullptr;
  // Owned by test widget view hierarchy.
  views::View* bubble_opener_ = nullptr;
  // Owned by test widget view hierarchy.
  views::View* other_view_ = nullptr;

  std::unique_ptr<LoginBubble> bubble_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginBubbleTest);
};

}  // namespace

// Verifies the base bubble settings.
TEST_F(LoginBubbleTest, BaseBubbleSettings) {
  bubble_->ShowUserMenu(base::string16(), base::string16(), container_,
                        bubble_opener_, false /*show_remove_user*/);
  EXPECT_TRUE(bubble_->IsVisible());

  LoginBaseBubbleView* bubble_view = bubble_->bubble_view_for_test();
  EXPECT_EQ(bubble_view->GetDialogButtons(), ui::DIALOG_BUTTON_NONE);
  EXPECT_EQ(bubble_view->width(), kBubbleTotalWidthDp);
  EXPECT_EQ(bubble_view->color(), SK_ColorBLACK);
  EXPECT_EQ(bubble_view->margins(),
            gfx::Insets(kBubbleTopMarginDp, kBubbleHorizontalMarginDp,
                        kBubbleBottomMarginDp, kBubbleHorizontalMarginDp));
  bubble_->Close();
}

// Verifies the bubble handles key event correctly.
TEST_F(LoginBubbleTest, BubbleKeyEventHandling) {
  EXPECT_FALSE(bubble_->IsVisible());

  // Verifies that key event won't open the bubble.
  ui::test::EventGenerator& generator = GetEventGenerator();
  other_view_->RequestFocus();
  generator.PressKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  EXPECT_FALSE(bubble_->IsVisible());

  // Verifies that key event on the bubble opener view won't close the bubble.
  bubble_->ShowUserMenu(base::string16(), base::string16(), container_,
                        bubble_opener_, false /*show_remove_user*/);
  EXPECT_TRUE(bubble_->IsVisible());
  bubble_opener_->RequestFocus();
  generator.PressKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  EXPECT_TRUE(bubble_->IsVisible());

  // Verifies that key event on the other view will close the bubble.
  other_view_->RequestFocus();
  generator.PressKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  EXPECT_FALSE(bubble_->IsVisible());
}

// Verifies the bubble handles mouse event correctly.
TEST_F(LoginBubbleTest, BubbleMouseEventHandling) {
  EXPECT_FALSE(bubble_->IsVisible());

  // Verifies that mouse event won't open the bubble.
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.MoveMouseTo(other_view_->GetBoundsInScreen().CenterPoint());
  generator.ClickLeftButton();
  EXPECT_FALSE(bubble_->IsVisible());

  // Verifies that mouse event on the bubble opener view won't close the bubble.
  bubble_->ShowUserMenu(base::string16(), base::string16(), container_,
                        bubble_opener_, false /*show_remove_user*/);
  EXPECT_TRUE(bubble_->IsVisible());
  generator.MoveMouseTo(bubble_opener_->GetBoundsInScreen().CenterPoint());
  generator.ClickLeftButton();
  EXPECT_TRUE(bubble_->IsVisible());

  // Verifies that mouse event on the bubble itself won't close the bubble.
  generator.MoveMouseTo(
      bubble_->bubble_view_for_test()->GetBoundsInScreen().CenterPoint());
  generator.ClickLeftButton();
  EXPECT_TRUE(bubble_->IsVisible());

  // Verifies that mouse event on the other view will close the bubble.
  generator.MoveMouseTo(other_view_->GetBoundsInScreen().CenterPoint());
  generator.ClickLeftButton();
  EXPECT_FALSE(bubble_->IsVisible());
}

// Verifies the bubble handles gesture event correctly.
TEST_F(LoginBubbleTest, BubbleGestureEventHandling) {
  EXPECT_FALSE(bubble_->IsVisible());

  // Verifies that gesture event won't open the bubble.
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.GestureTapAt(other_view_->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(bubble_->IsVisible());

  // Verifies that gesture event on the bubble opener view won't close the
  // bubble.
  bubble_->ShowUserMenu(base::string16(), base::string16(), container_,
                        bubble_opener_, false /*show_remove_user*/);
  EXPECT_TRUE(bubble_->IsVisible());
  generator.GestureTapAt(bubble_opener_->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(bubble_->IsVisible());

  // Verifies that gesture event on the bubble itself won't close the bubble.
  generator.GestureTapAt(
      bubble_->bubble_view_for_test()->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(bubble_->IsVisible());

  // Verifies that gesture event on the other view will close the bubble.
  generator.GestureTapAt(other_view_->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(bubble_->IsVisible());
}

}  // namespace ash
