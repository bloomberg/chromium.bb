// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_user_view.h"
#include "ash/login/ui/login_display_style.h"
#include "ash/login/ui/login_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"
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
    auto* view =
        new LoginUserView(display_style, show_dropdown,
                          base::BindRepeating(&LoginUserViewUnittest::OnTapped,
                                              base::Unretained(this)));
    mojom::LoginUserInfoPtr user = CreateUser("foo");
    view->UpdateForUser(user, false /*animate*/);
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
    SetWidget(CreateWidgetWithContent(root));
  }

  int tap_count_ = 0;

  views::View* container_ = nullptr;  // Owned by test widget view hierarchy.

 private:
  void OnTapped() { ++tap_count_; }

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
    mojom::LoginUserInfoPtr user = CreateUser(name);
    large->UpdateForUser(user, false /*animate*/);
    small->UpdateForUser(user, false /*animate*/);
    extra_small->UpdateForUser(user, false /*animate*/);
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

// Verifies that the entire user view is a tap target, and not just (for
// example) the user icon.
TEST_F(LoginUserViewUnittest, EntireViewIsTapTarget) {
  LoginUserView* view = AddUserView(LoginDisplayStyle::kSmall, false);
  EXPECT_NE(view->size(), gfx::Size());

  // Returns true if there is a tap at |point| offset by |dx|, |dy|.
  auto tap = [this](gfx::Point point, int dx, int dy) -> bool {
    point.Offset(dx, dy);
    GetEventGenerator().MoveMouseTo(point);
    GetEventGenerator().ClickLeftButton();
    bool result = tap_count_ == 1;
    tap_count_ = 0;
    return result;
  };

  // Click various locations inside of the view.
  EXPECT_TRUE(tap(view->GetBoundsInScreen().CenterPoint(), 0, 0));
  EXPECT_TRUE(tap(view->GetBoundsInScreen().origin(), 0, 0));
  EXPECT_TRUE(tap(view->GetBoundsInScreen().top_right(), -1, 0));
  EXPECT_TRUE(tap(view->GetBoundsInScreen().bottom_left(), 0, -1));
  EXPECT_TRUE(tap(view->GetBoundsInScreen().bottom_right(), -1, -1));

  // Click a location outside of the view bounds.
  EXPECT_FALSE(tap(view->GetBoundsInScreen().bottom_right(), 1, 1));
}

// Verifies the focused user view is opaque. Verifies that a hovered view is
// opaque. Verifies the interaction between focus and hovered opaqueness.
TEST_F(LoginUserViewUnittest, FocusHoverOpaqueInteractions) {
  LoginUserView* one = AddUserView(LoginDisplayStyle::kSmall, false);
  LoginUserView* two = AddUserView(LoginDisplayStyle::kSmall, false);
  LoginUserView::TestApi one_test(one);
  LoginUserView::TestApi two_test(two);

  // Start out as non-opaque.
  EXPECT_FALSE(one_test.is_opaque());
  EXPECT_FALSE(two_test.is_opaque());

  // Only the focused element is opaque.
  one->RequestFocus();
  EXPECT_TRUE(one_test.is_opaque());
  EXPECT_FALSE(two_test.is_opaque());
  two->RequestFocus();
  EXPECT_FALSE(one_test.is_opaque());
  EXPECT_TRUE(two_test.is_opaque());

  // Non-focused element can be opaque if the mouse is over it.
  GetEventGenerator().MoveMouseTo(one->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(one_test.is_opaque());
  EXPECT_TRUE(two_test.is_opaque());

  // Focused element stays opaque when mouse is over it.
  GetEventGenerator().MoveMouseTo(two->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(one_test.is_opaque());
  EXPECT_TRUE(two_test.is_opaque());

  // Focused element stays opaque when mouse leaves it.
  GetEventGenerator().MoveMouseTo(one->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(one_test.is_opaque());
  EXPECT_TRUE(two_test.is_opaque());

  // Losing focus (after a mouse hover) makes the element transparent.
  one->RequestFocus();
  EXPECT_TRUE(one_test.is_opaque());
  EXPECT_FALSE(two_test.is_opaque());
}

// Verifies that forced opaque keeps the element opaque even if it gains/loses
// focus, and that a forced opaque element can transition to both
// opaque/transparent when losing forced opaque.
TEST_F(LoginUserViewUnittest, ForcedOpaque) {
  LoginUserView* one = AddUserView(LoginDisplayStyle::kSmall, false);
  LoginUserView* two = AddUserView(LoginDisplayStyle::kSmall, false);
  LoginUserView::TestApi one_test(one);
  LoginUserView::TestApi two_test(two);

  // Start out as non-opaque.
  EXPECT_FALSE(one_test.is_opaque());
  EXPECT_FALSE(two_test.is_opaque());

  // Non-opaque becomes opaque with SetForceOpaque.
  one->SetForceOpaque(true);
  EXPECT_TRUE(one_test.is_opaque());
  EXPECT_FALSE(two_test.is_opaque());

  // Forced opaque stays opaque when gaining or losing focus.
  one->RequestFocus();
  EXPECT_TRUE(one_test.is_opaque());
  EXPECT_FALSE(two_test.is_opaque());
  two->RequestFocus();
  EXPECT_TRUE(one_test.is_opaque());
  EXPECT_TRUE(two_test.is_opaque());

  // An element can become transparent when losing forced opaque.
  EXPECT_TRUE(two->HasFocus());
  one->SetForceOpaque(false);
  EXPECT_FALSE(one_test.is_opaque());
  EXPECT_TRUE(two_test.is_opaque());

  // An element can stay opaque when losing forced opaque.
  EXPECT_TRUE(two->HasFocus());
  two->SetForceOpaque(true);
  EXPECT_FALSE(one_test.is_opaque());
  EXPECT_TRUE(two_test.is_opaque());
  two->SetForceOpaque(false);
  EXPECT_FALSE(one_test.is_opaque());
  EXPECT_TRUE(two_test.is_opaque());
}

}  // namespace ash
