// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_password_view.h"

#include "ash/login/ui/login_pin_view.h"
#include "ash/login/ui/login_test_base.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {

class LoginPinViewTest : public LoginTestBase {
 protected:
  LoginPinViewTest() = default;
  ~LoginPinViewTest() override = default;

  // LoginScreenTest:
  void SetUp() override {
    LoginTestBase::SetUp();

    view_ = new LoginPinView(
        base::Bind(&LoginPinViewTest::OnPinKey, base::Unretained(this)),
        base::Bind(&LoginPinViewTest::OnPinBackspace, base::Unretained(this)));

    ShowWidgetWithContent(view_);
  }

  // Called when a password is submitted.
  void OnPinKey(int value) { value_ = value; }
  void OnPinBackspace() { backspace_ = true; }

  LoginPinView* view_ = nullptr;  // Owned by test widget view hierarchy.
  base::Optional<int> value_;
  bool backspace_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginPinViewTest);
};

}  // namespace

// Verifies that PIN submit works with 'Enter'.
TEST_F(LoginPinViewTest, ButtonsFireEvents) {
  ui::test::EventGenerator& generator = GetEventGenerator();
  LoginPinView::TestApi test_api(view_);

  // Verify pin button events are emitted with the correct value.
  for (int i = 0; i <= 9; ++i) {
    test_api.GetButton(i)->RequestFocus();
    generator.PressKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);
    EXPECT_TRUE(value_.has_value());
    EXPECT_EQ(*value_, i);
    value_.reset();
  }

  // Verify backspace events are emitted.
  EXPECT_FALSE(backspace_);
  test_api.GetBackspaceButton()->RequestFocus();
  generator.PressKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);
  EXPECT_TRUE(backspace_);
}

// Validates buttons have the correct spacing.
TEST_F(LoginPinViewTest, ButtonSpacingAndSize) {
  LoginPinView::TestApi test_api(view_);

  // Validate pin button size.
  for (int i = 0; i <= 9; ++i) {
    DCHECK_EQ(test_api.GetButton(i)->size().width(),
              LoginPinView::kButtonSizeDp);
    DCHECK_EQ(test_api.GetButton(i)->size().height(),
              LoginPinView::kButtonSizeDp);
  }

  // Validate backspace button size.
  DCHECK_EQ(test_api.GetBackspaceButton()->size().width(),
            LoginPinView::kButtonSizeDp);
  DCHECK_EQ(test_api.GetBackspaceButton()->size().height(),
            LoginPinView::kButtonSizeDp);

  // Record all the x/y coordinates of the buttons.
  std::set<int> seen_x;
  std::set<int> seen_y;
  for (int i = 0; i <= 9; ++i) {
    gfx::Rect screen_bounds = test_api.GetButton(i)->GetBoundsInScreen();
    seen_x.insert(screen_bounds.x());
    seen_y.insert(screen_bounds.y());
  }
  seen_x.insert(test_api.GetBackspaceButton()->GetBoundsInScreen().x());
  seen_y.insert(test_api.GetBackspaceButton()->GetBoundsInScreen().y());

  // Sort the coordinates so we can easily check the distance between them.
  std::vector<int> sorted_x(seen_x.begin(), seen_x.end());
  std::vector<int> sorted_y(seen_y.begin(), seen_y.end());
  std::sort(sorted_x.begin(), sorted_x.end());
  std::sort(sorted_y.begin(), sorted_y.end());

  // Validate each x or y coordinate has the correct distance between it and the
  // next one. This is correct because we have already validated button size.
  EXPECT_EQ(3u, sorted_x.size());
  for (size_t i = 0; i < sorted_x.size() - 1; ++i) {
    EXPECT_EQ(sorted_x[i] + LoginPinView::kButtonSizeDp +
                  LoginPinView::kButtonSeparatorSizeDp,
              sorted_x[i + 1]);
  }

  EXPECT_EQ(4u, sorted_y.size());
  for (size_t i = 0; i < sorted_y.size() - 1; ++i) {
    EXPECT_EQ(sorted_y[i] + LoginPinView::kButtonSizeDp +
                  LoginPinView::kButtonSeparatorSizeDp,
              sorted_y[i + 1]);
  }
}

}  // namespace ash