// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_auth_user_view.h"
#include "ash/login/ui/login_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/layout/box_layout.h"

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
    view_ = new LoginAuthUserView(user_, base::Bind([](bool auth_success) {}));

    // We proxy |view_| inside of |container_| so we can control layout.
    container_ = new views::View();
    container_->SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical));
    container_->AddChildView(view_);
    ShowWidgetWithContent(container_);
  }

  mojom::UserInfoPtr user_;
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

}  // namespace ash