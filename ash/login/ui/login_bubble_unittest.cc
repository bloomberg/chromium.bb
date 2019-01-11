// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "ash/login/ui/login_bubble.h"
#include "ash/login/ui/login_button.h"
#include "ash/login/ui/login_menu_view.h"
#include "ash/login/ui/login_test_base.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/animation/test/ink_drop_host_view_test_api.h"
#include "ui/views/controls/label.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Non zero size for the bubble anchor view.
constexpr int kBubbleAnchorViewSizeDp = 100;

std::vector<LoginMenuView::Item> PopulateMenuItems() {
  std::vector<LoginMenuView::Item> items;
  // Add one regular item.
  LoginMenuView::Item item1;
  item1.title = "Regular Item 1";
  item1.is_group = false;
  item1.selected = true;
  items.push_back(item1);

  // Add one group item.
  LoginMenuView::Item item2;
  item2.title = "Group Item 2";
  item2.is_group = true;
  items.push_back(item2);

  // Add another regular item.
  LoginMenuView::Item item3;
  item3.title = "Regular Item 2";
  item3.is_group = false;
  items.push_back(item3);
  return items;
}

class LoginBubbleTest : public LoginTestBase {
 protected:
  LoginBubbleTest() = default;
  ~LoginBubbleTest() override = default;

  // LoginTestBase:
  void SetUp() override {
    LoginTestBase::SetUp();

    container_ = new views::View();
    container_->SetLayoutManager(
        std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));

    bubble_opener_ = new LoginButton(nullptr /*listener*/);
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
    bubble_->Close();
    LoginTestBase::TearDown();
  }

  void ShowSelectionMenu(const LoginMenuView::OnSelect& on_select) {
    LoginMenuView* view = new LoginMenuView(PopulateMenuItems(), container_,
                                            bubble_opener_, on_select);
    bubble_->ShowSelectionMenu(view);
  }

  // Owned by test widget view hierarchy.
  views::View* container_ = nullptr;
  // Owned by test widget view hierarchy.
  LoginButton* bubble_opener_ = nullptr;
  // Owned by test widget view hierarchy.
  views::View* other_view_ = nullptr;

  std::unique_ptr<LoginBubble> bubble_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginBubbleTest);
};

}  // namespace

TEST_F(LoginBubbleTest, TestShowSelectionMenu) {
  ui::test::EventGenerator* generator = GetEventGenerator();

  EXPECT_FALSE(bubble_->IsVisible());
  LoginMenuView::Item selected_item;
  bool selected = false;
  ShowSelectionMenu(base::BindLambdaForTesting([&](LoginMenuView::Item item) {
    selected_item = item;
    selected = true;
  }));
  EXPECT_TRUE(bubble_->IsVisible());

  // Verifies that regular item 1 is selectable.
  LoginMenuView* menu_view =
      static_cast<LoginMenuView*>(bubble_->bubble_view());
  LoginMenuView::TestApi test_api1(menu_view);
  EXPECT_TRUE(test_api1.contents()->child_at(0)->HasFocus());
  generator->PressKey(ui::KeyboardCode::VKEY_RETURN, 0 /*flag*/);
  EXPECT_FALSE(bubble_->IsVisible());
  EXPECT_EQ(selected_item.title, "Regular Item 1");
  EXPECT_TRUE(selected);

  // Verfies that group item 2 is not selectable.
  selected = false;
  ShowSelectionMenu(base::BindLambdaForTesting([&](LoginMenuView::Item item) {
    selected_item = item;
    selected = true;
  }));
  EXPECT_TRUE(bubble_->IsVisible());
  menu_view = static_cast<LoginMenuView*>(bubble_->bubble_view());
  LoginMenuView::TestApi test_api2(menu_view);
  test_api2.contents()->child_at(1)->RequestFocus();
  generator->PressKey(ui::KeyboardCode::VKEY_RETURN, 0 /*flag*/);
  EXPECT_TRUE(bubble_->IsVisible());
  EXPECT_FALSE(selected);

  // Verifies up/down arrow key can navigate menu entries.
  generator->PressKey(ui::KeyboardCode::VKEY_UP, 0 /*flag*/);
  EXPECT_TRUE(test_api2.contents()->child_at(0)->HasFocus());
  generator->PressKey(ui::KeyboardCode::VKEY_UP, 0 /*flag*/);
  EXPECT_TRUE(test_api2.contents()->child_at(0)->HasFocus());

  generator->PressKey(ui::KeyboardCode::VKEY_DOWN, 0 /*flag*/);
  // Group item is skipped in up/down key navigation.
  EXPECT_TRUE(test_api2.contents()->child_at(2)->HasFocus());
  generator->PressKey(ui::KeyboardCode::VKEY_DOWN, 0 /*flag*/);
  EXPECT_TRUE(test_api2.contents()->child_at(2)->HasFocus());
  EXPECT_TRUE(bubble_->IsVisible());

  bubble_->Close();
  EXPECT_FALSE(bubble_->IsVisible());
}

}  // namespace ash
