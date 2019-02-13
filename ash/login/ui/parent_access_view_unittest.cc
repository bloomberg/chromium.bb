// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/parent_access_view.h"

#include <string>

#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/login_button.h"
#include "ash/login/ui/login_pin_view.h"
#include "ash/login/ui/login_test_base.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/optional.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

class ParentAccessViewTest : public LoginTestBase {
 protected:
  ParentAccessViewTest() = default;
  ~ParentAccessViewTest() override = default;

  // LoginScreenTest:
  void SetUp() override {
    LoginTestBase::SetUp();

    ParentAccessView::Callbacks callbacks;
    callbacks.on_back = base::BindRepeating(&ParentAccessViewTest::OnBack,
                                            base::Unretained(this));
    callbacks.on_submit = base::BindRepeating(&ParentAccessViewTest::OnSubmit,
                                              base::Unretained(this));

    view_ = new ParentAccessView(callbacks);
    SetWidget(CreateWidgetWithContent(view_));
  }

  // Simulates mouse press event on a |button|.
  void SimulateButtonPress(views::Button* button) {
    ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), 0, 0);
    view_->ButtonPressed(button, event);
  }

  // Simulates mouse press event on pin keyboard |button|.
  void SimulatePinKeyboardPress(views::View* button) {
    ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), 0, 0);
    button->OnEvent(&event);
  }

  // Back button callback.
  void OnBack() { ++back_action_; }

  // Submit button callback.
  void OnSubmit(const std::string& code) {
    code_ = code;
    ++submit_action_;
  }

  // Submitted access code.
  base::Optional<std::string> code_;

  // Number of times back button event was sent.
  int back_action_ = 0;

  // Number of times submit button event was sent.
  int submit_action_ = 0;

  ParentAccessView* view_ = nullptr;  // Owned by test widget view hierarchy.

 private:
  DISALLOW_COPY_AND_ASSIGN(ParentAccessViewTest);
};

}  // namespace

// Tests that back button works.
TEST_F(ParentAccessViewTest, BackButton) {
  ParentAccessView::TestApi test_api(view_);
  EXPECT_TRUE(test_api.back_button()->enabled());
  EXPECT_EQ(0, back_action_);

  SimulateButtonPress(test_api.back_button());

  EXPECT_EQ(1, back_action_);
  EXPECT_EQ(0, submit_action_);
}

// Tests that submit button submits code from code input.
TEST_F(ParentAccessViewTest, SubmitButton) {
  ParentAccessView::TestApi test_api(view_);
  EXPECT_FALSE(test_api.submit_button()->enabled());

  ui::test::EventGenerator* generator = GetEventGenerator();
  for (int i = 0; i < 6; ++i) {
    generator->PressKey(ui::KeyboardCode(ui::KeyboardCode::VKEY_0 + i),
                        ui::EF_NONE);
  }
  EXPECT_TRUE(test_api.submit_button()->enabled());

  SimulateButtonPress(test_api.submit_button());
  ASSERT_TRUE(code_.has_value());
  EXPECT_EQ("012345", *code_);
  EXPECT_EQ(1, submit_action_);
}

// Tests that backspace button works.
TEST_F(ParentAccessViewTest, Backspace) {
  ParentAccessView::TestApi test_api(view_);
  EXPECT_FALSE(test_api.submit_button()->enabled());

  ui::test::EventGenerator* generator = GetEventGenerator();
  for (int i = 0; i < 6; ++i)
    generator->PressKey(ui::KeyboardCode::VKEY_1, ui::EF_NONE);
  EXPECT_TRUE(test_api.submit_button()->enabled());

  // Clear last field - will move focus to before last field.
  generator->PressKey(ui::KeyboardCode::VKEY_BACK, ui::EF_NONE);
  EXPECT_FALSE(test_api.submit_button()->enabled());

  // Change value in before last field.
  generator->PressKey(ui::KeyboardCode::VKEY_2, ui::EF_NONE);
  EXPECT_FALSE(test_api.submit_button()->enabled());

  // Fill in value in last field.
  generator->PressKey(ui::KeyboardCode::VKEY_3, ui::EF_NONE);
  EXPECT_TRUE(test_api.submit_button()->enabled());

  SimulateButtonPress(test_api.submit_button());
  ASSERT_TRUE(code_.has_value());
  EXPECT_EQ("111123", *code_);
  EXPECT_EQ(1, submit_action_);
}

// Tests input with virtual pin keyboard.
TEST_F(ParentAccessViewTest, PinKeyboard) {
  ParentAccessView::TestApi test_api(view_);
  LoginPinView::TestApi test_pin_keyboard(test_api.pin_keyboard_view());
  EXPECT_FALSE(test_api.submit_button()->enabled());

  for (int i = 0; i < 6; ++i)
    SimulatePinKeyboardPress(test_pin_keyboard.GetButton(i));
  EXPECT_TRUE(test_api.submit_button()->enabled());

  SimulateButtonPress(test_api.submit_button());
  ASSERT_TRUE(code_.has_value());
  EXPECT_EQ("012345", *code_);
  EXPECT_EQ(1, submit_action_);
}

}  // namespace ash
