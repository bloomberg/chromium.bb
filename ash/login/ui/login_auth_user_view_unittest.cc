// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_auth_user_view.h"
#include "ash/login/ui/login_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

class LoginAuthUserViewUnittest : public LoginTestBase {
 protected:
  LoginAuthUserViewUnittest() = default;
  ~LoginAuthUserViewUnittest() override = default;

  // LoginTestBase:
  void SetUp() override {
    LoginTestBase::SetUp();

    user_ = CreateUser("user");
    view_ = new LoginAuthUserView(
        user_, base::Bind([](bool auth_success) {}) /*on_auth*/,
        base::Bind([]() {}) /*on_easy_unlock_icon_hovered*/,
        base::Bind([]() {}) /*on_easy_unlock_icon_tapped*/,
        base::Bind([]() {}) /*on_tap*/);

    // We proxy |view_| inside of |container_| so we can control layout.
    container_ = new views::View();
    container_->SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical));
    container_->AddChildView(view_);
    SetWidget(CreateWidgetWithContent(container_));
  }

  mojom::LoginUserInfoPtr user_;
  views::View* container_ = nullptr;   // Owned by test widget view hierarchy.
  LoginAuthUserView* view_ = nullptr;  // Owned by test widget view hierarchy.
  base::Optional<int> value_;
  bool backspace_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginAuthUserViewUnittest);
};

}  // namespace

// Verifies showing the PIN keyboard makes the user view grow.
TEST_F(LoginAuthUserViewUnittest, ShowingPinExpandsView) {
  gfx::Size start_size = view_->size();
  view_->SetAuthMethods(LoginAuthUserView::AUTH_PIN);
  container_->Layout();
  gfx::Size expanded_size = view_->size();
  EXPECT_GT(expanded_size.height(), start_size.height());
}

// Verifies that an auth user that shows a password is opaque.
TEST_F(LoginAuthUserViewUnittest, ShowingPasswordForcesOpaque) {
  LoginAuthUserView::TestApi auth_test(view_);
  LoginUserView::TestApi user_test(auth_test.user_view());

  // Add another view that will hold focus. The user view cannot have focus
  // since focus will keep it opaque.
  auto* focus = new views::View();
  focus->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  container_->AddChildView(focus);
  focus->RequestFocus();
  EXPECT_FALSE(auth_test.user_view()->HasFocus());

  // If the user view is showing a password it must be opaque.
  view_->SetAuthMethods(LoginAuthUserView::AUTH_PASSWORD);
  EXPECT_TRUE(user_test.is_opaque());
  view_->SetAuthMethods(LoginAuthUserView::AUTH_NONE);
  EXPECT_FALSE(user_test.is_opaque());
  view_->SetAuthMethods(LoginAuthUserView::AUTH_PASSWORD);
  EXPECT_TRUE(user_test.is_opaque());
}

}  // namespace ash
