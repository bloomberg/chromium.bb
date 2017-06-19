// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_user_view.h"
#include "ash/login/ui/login_display_style.h"
#include "ash/login/ui/login_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

class LoginUserViewUnittest : public LoginTestBase {
 protected:
  LoginUserViewUnittest() = default;
  ~LoginUserViewUnittest() override = default;

  // Builds a new LoginUserView instance and adds it to |container_|.
  LoginUserView* AddUserView(LoginDisplayStyle display_style,
                             bool show_dropdown) {
    auto* view = new LoginUserView(display_style, show_dropdown);
    mojom::UserInfoPtr user = CreateUser("foo");
    view->UpdateForUser(user);
    container_->AddChildView(view);
    widget()->GetContentsView()->Layout();
    return view;
  }

  // LoginTestBase:
  void SetUp() override {
    LoginTestBase::SetUp();

    container_ = new views::View();
    container_->SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical));

    auto* root = new views::View();
    root->SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal));
    root->AddChildView(container_);
    ShowWidgetWithContent(root);
  }

  views::View* container_ = nullptr;  // Owned by test widget view hierarchy.

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginUserViewUnittest);
};

}  // namespace

// Verifies that the user view does not change width for short/long usernames.
TEST_F(LoginUserViewUnittest, DifferentUsernamesHaveSameWidth) {
  LoginUserView* large = AddUserView(LoginDisplayStyle::kLarge, false);
  LoginUserView* small = AddUserView(LoginDisplayStyle::kSmall, false);
  LoginUserView* extra_small =
      AddUserView(LoginDisplayStyle::kExtraSmall, false);

  int large_width = large->size().width();
  int small_width = small->size().width();
  int extra_small_width = extra_small->size().width();
  EXPECT_GT(large_width, 0);
  EXPECT_GT(small_width, 0);
  EXPECT_GT(extra_small_width, 0);

  for (int i = 0; i < 25; ++i) {
    std::string name(i, 'a');
    mojom::UserInfoPtr user = CreateUser(name);
    large->UpdateForUser(user);
    small->UpdateForUser(user);
    extra_small->UpdateForUser(user);
    container_->Layout();

    EXPECT_EQ(large_width, large->size().width());
    EXPECT_EQ(small_width, small->size().width());
    EXPECT_EQ(extra_small_width, extra_small->size().width());
  }
}

// Verifies that the user views all have different sizes with different display
// styles.
TEST_F(LoginUserViewUnittest, DifferentStylesHaveDifferentSizes) {
  LoginUserView* large = AddUserView(LoginDisplayStyle::kLarge, false);
  LoginUserView* small = AddUserView(LoginDisplayStyle::kSmall, false);
  LoginUserView* extra_small =
      AddUserView(LoginDisplayStyle::kExtraSmall, false);

  EXPECT_NE(large->size(), gfx::Size());
  EXPECT_NE(large->size(), small->size());
  EXPECT_NE(large->size(), extra_small->size());
  EXPECT_NE(small->size(), extra_small->size());
}

// Verifies that displaying the dropdown does not change the view size. Further,
// the dropdown should not change the centering for the user label.
TEST_F(LoginUserViewUnittest, DropdownDoesNotChangeSize) {
  LoginUserView* with = AddUserView(LoginDisplayStyle::kLarge, true);
  LoginUserView* without = AddUserView(LoginDisplayStyle::kLarge, false);
  EXPECT_NE(with->size(), gfx::Size());
  EXPECT_EQ(with->size(), without->size());

  views::View* with_label = LoginUserView::TestApi(with).user_label();
  views::View* without_label = LoginUserView::TestApi(without).user_label();

  EXPECT_EQ(with_label->GetBoundsInScreen().x(),
            without_label->GetBoundsInScreen().x());
  EXPECT_NE(with_label->size(), gfx::Size());
  EXPECT_EQ(with_label->size(), without_label->size());
}

}  // namespace ash