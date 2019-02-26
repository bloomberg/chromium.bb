// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/parent_access_view.h"

#include <memory>
#include <string>

#include "ash/login/mock_login_screen_client.h"
#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/login_button.h"
#include "ash/login/ui/login_pin_view.h"
#include "ash/login/ui/login_test_base.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/optional.h"
#include "components/account_id/account_id.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

class ParentAccessViewTest : public LoginTestBase {
 protected:
  ParentAccessViewTest()
      : account_id_(AccountId::FromUserEmail("child@gmail.com")) {}
  ~ParentAccessViewTest() override = default;

  // LoginScreenTest:
  void SetUp() override {
    LoginTestBase::SetUp();

    ParentAccessView::Callbacks callbacks;
    callbacks.on_finished = base::BindRepeating(
        &ParentAccessViewTest::OnFinished, base::Unretained(this));

    view_ = new ParentAccessView(account_id_, callbacks);
    SetWidget(CreateWidgetWithContent(view_));

    login_client_ = BindMockLoginScreenClient();
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

  // Called when ParentAccessView finished processing.
  void OnFinished(bool access_granted) {
    access_granted ? ++successful_validation_ : ++back_action_;
  }

  const AccountId account_id_;
  std::unique_ptr<MockLoginScreenClient> login_client_;

  // Submitted access code.
  base::Optional<std::string> code_;

  // Number of times the view was dismissed with back button.
  int back_action_ = 0;

  // Number of times the view was dismissed after successful validation.
  int successful_validation_ = 0;

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
  EXPECT_EQ(0, successful_validation_);
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

  login_client_->set_validate_parent_access_code_result(true);
  EXPECT_CALL(*login_client_,
              ValidateParentAccessCode_(account_id_, "012345", testing::_))
      .Times(1);

  SimulateButtonPress(test_api.submit_button());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, successful_validation_);
}

// Tests that access code can be entered with numpad.
TEST_F(ParentAccessViewTest, Numpad) {
  ParentAccessView::TestApi test_api(view_);

  ui::test::EventGenerator* generator = GetEventGenerator();
  for (int i = 0; i < 6; ++i)
    generator->PressKey(ui::KeyboardCode(ui::VKEY_NUMPAD0 + i), ui::EF_NONE);
  EXPECT_TRUE(test_api.submit_button()->enabled());

  login_client_->set_validate_parent_access_code_result(true);
  EXPECT_CALL(*login_client_,
              ValidateParentAccessCode_(account_id_, "012345", testing::_))
      .Times(1);

  SimulateButtonPress(test_api.submit_button());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, successful_validation_);
}

// Tests that backspace button works.
TEST_F(ParentAccessViewTest, Backspace) {
  ParentAccessView::TestApi test_api(view_);
  EXPECT_FALSE(test_api.submit_button()->enabled());

  ui::test::EventGenerator* generator = GetEventGenerator();
  for (int i = 0; i < 6; ++i)
    generator->PressKey(ui::KeyboardCode::VKEY_1, ui::EF_NONE);
  EXPECT_TRUE(test_api.submit_button()->enabled());

  // Active field has content - backspace clears the content, but does not move
  // focus.
  generator->PressKey(ui::KeyboardCode::VKEY_BACK, ui::EF_NONE);
  EXPECT_FALSE(test_api.submit_button()->enabled());

  // Active Field is empty - backspace moves focus to before last field.
  generator->PressKey(ui::KeyboardCode::VKEY_BACK, ui::EF_NONE);
  EXPECT_FALSE(test_api.submit_button()->enabled());

  // Change value in before last field.
  generator->PressKey(ui::KeyboardCode::VKEY_2, ui::EF_NONE);
  EXPECT_FALSE(test_api.submit_button()->enabled());

  // Fill in value in last field.
  generator->PressKey(ui::KeyboardCode::VKEY_3, ui::EF_NONE);
  EXPECT_TRUE(test_api.submit_button()->enabled());

  login_client_->set_validate_parent_access_code_result(true);
  EXPECT_CALL(*login_client_,
              ValidateParentAccessCode_(account_id_, "111123", testing::_))
      .Times(1);

  SimulateButtonPress(test_api.submit_button());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, successful_validation_);
}

// Tests input with virtual pin keyboard.
TEST_F(ParentAccessViewTest, PinKeyboard) {
  ParentAccessView::TestApi test_api(view_);
  LoginPinView::TestApi test_pin_keyboard(test_api.pin_keyboard_view());
  EXPECT_FALSE(test_api.submit_button()->enabled());

  for (int i = 0; i < 6; ++i)
    SimulatePinKeyboardPress(test_pin_keyboard.GetButton(i));
  EXPECT_TRUE(test_api.submit_button()->enabled());

  login_client_->set_validate_parent_access_code_result(true);
  EXPECT_CALL(*login_client_,
              ValidateParentAccessCode_(account_id_, "012345", testing::_))
      .Times(1);

  SimulateButtonPress(test_api.submit_button());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, successful_validation_);
}

// Tests that error state is shown and cleared when neccesary.
TEST_F(ParentAccessViewTest, ErrorState) {
  ParentAccessView::TestApi test_api(view_);
  EXPECT_EQ(ParentAccessView::State::kNormal, test_api.state());

  ui::test::EventGenerator* generator = GetEventGenerator();
  for (int i = 0; i < 6; ++i) {
    generator->PressKey(ui::KeyboardCode(ui::KeyboardCode::VKEY_0 + i),
                        ui::EF_NONE);
  }

  // Error should be shown after unsuccessful validation.
  login_client_->set_validate_parent_access_code_result(false);
  EXPECT_CALL(*login_client_,
              ValidateParentAccessCode_(account_id_, "012345", testing::_))
      .Times(1);

  SimulateButtonPress(test_api.submit_button());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ParentAccessView::State::kError, test_api.state());
  EXPECT_EQ(0, successful_validation_);

  // Updating input code (here last digit) should clear error state.
  generator->PressKey(ui::KeyboardCode::VKEY_6, ui::EF_NONE);
  EXPECT_EQ(ParentAccessView::State::kNormal, test_api.state());

  login_client_->set_validate_parent_access_code_result(true);
  EXPECT_CALL(*login_client_,
              ValidateParentAccessCode_(account_id_, "012346", testing::_))
      .Times(1);

  SimulateButtonPress(test_api.submit_button());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, successful_validation_);
}

}  // namespace ash
