// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_password_view.h"

#include "ash/login/ui/login_test_base.h"
#include "ash/public/cpp/config.h"
#include "ash/shell.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

class LoginPasswordViewTest : public LoginTestBase {
 protected:
  LoginPasswordViewTest() = default;
  ~LoginPasswordViewTest() override = default;

  // LoginScreenTest:
  void SetUp() override {
    LoginTestBase::SetUp();

    view_ = new LoginPasswordView();
    view_->Init(base::Bind(&LoginPasswordViewTest::OnPasswordSubmit,
                           base::Unretained(this)),
                base::Bind(&LoginPasswordViewTest::OnPasswordTextChanged,
                           base::Unretained(this)));
    SetWidget(CreateWidgetWithContent(view_));
  }

  // Called when a password is submitted.
  void OnPasswordSubmit(const base::string16& password) {
    password_ = password;
  }

  // Called when the password field text changed.
  void OnPasswordTextChanged(bool is_empty) {
    is_password_field_empty_ = is_empty;
  }

  LoginPasswordView* view_ = nullptr;
  base::Optional<base::string16> password_;
  bool is_password_field_empty_ = true;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginPasswordViewTest);
};

}  // namespace

// Verifies that password submit works with 'Enter'.
TEST_F(LoginPasswordViewTest, PasswordSubmitIncludesPasswordText) {
  // TODO: Renable in mash once crbug.com/725257 is fixed.
  if (Shell::GetAshConfig() == Config::MASH)
    return;

  LoginPasswordView::TestApi test_api(view_);

  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.PressKey(ui::KeyboardCode::VKEY_A, 0);
  generator.PressKey(ui::KeyboardCode::VKEY_B, 0);
  generator.PressKey(ui::KeyboardCode::VKEY_C, 0);
  generator.PressKey(ui::KeyboardCode::VKEY_1, 0);
  generator.PressKey(ui::KeyboardCode::VKEY_RETURN, 0);

  EXPECT_TRUE(password_.has_value());
  EXPECT_EQ(base::ASCIIToUTF16("abc1"), *password_);
}

// Verifies that password submit works when clicking the submit button.
TEST_F(LoginPasswordViewTest, PasswordSubmitViaButton) {
  // TODO: Renable in mash once crbug.com/725257 is fixed.
  if (Shell::GetAshConfig() == Config::MASH)
    return;

  LoginPasswordView::TestApi test_api(view_);

  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.PressKey(ui::KeyboardCode::VKEY_A, 0);
  generator.PressKey(ui::KeyboardCode::VKEY_B, 0);
  generator.PressKey(ui::KeyboardCode::VKEY_C, 0);
  generator.PressKey(ui::KeyboardCode::VKEY_1, 0);
  generator.MoveMouseTo(
      test_api.submit_button()->GetBoundsInScreen().CenterPoint());
  generator.ClickLeftButton();

  EXPECT_TRUE(password_.has_value());
  EXPECT_EQ(base::ASCIIToUTF16("abc1"), *password_);
}

// Verifies that text is cleared after submitting a password.
TEST_F(LoginPasswordViewTest, PasswordSubmitClearsPassword) {
  // TODO: Renable in mash once crbug.com/725257 is fixed.
  if (Shell::GetAshConfig() == Config::MASH)
    return;

  LoginPasswordView::TestApi test_api(view_);
  ui::test::EventGenerator& generator = GetEventGenerator();

  // Submit 'a' password.
  EXPECT_TRUE(is_password_field_empty_);
  generator.PressKey(ui::KeyboardCode::VKEY_A, 0);
  EXPECT_FALSE(is_password_field_empty_);
  generator.PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  EXPECT_TRUE(is_password_field_empty_);
  EXPECT_TRUE(password_.has_value());
  EXPECT_EQ(base::ASCIIToUTF16("a"), *password_);

  password_.reset();

  // Submit 'b' password.
  generator.PressKey(ui::KeyboardCode::VKEY_B, 0);
  EXPECT_FALSE(is_password_field_empty_);
  generator.PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  EXPECT_TRUE(is_password_field_empty_);
  EXPECT_TRUE(password_.has_value());
  // The submitted password is 'b' instead of "ab".
  EXPECT_EQ(base::ASCIIToUTF16("b"), *password_);
}

}  // namespace ash
