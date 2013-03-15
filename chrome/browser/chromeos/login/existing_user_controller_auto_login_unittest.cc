// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/mock_login_display.h"
#include "chrome/browser/chromeos/login/mock_login_display_host.h"
#include "chrome/browser/chromeos/login/mock_login_utils.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;
using testing::_;

namespace chromeos {

namespace {

const char kAutoLoginUsername[] = "public_session_user@localhost";
// These values are only used to test the configuration.  They don't
// delay the test.
const int kAutoLoginNoDelay = 0;
const int kAutoLoginDelay1 = 60000;
const int kAutoLoginDelay2 = 180000;

}  // namespace

class ExistingUserControllerAutoLoginTest : public ::testing::Test {
 protected:
  ExistingUserControllerAutoLoginTest()
      : message_loop_(MessageLoop::TYPE_UI),
        ui_thread_(content::BrowserThread::UI, &message_loop_),
        local_state_(TestingBrowserProcess::GetGlobal()) {
  }

  virtual void SetUp() {
    mock_login_display_host_.reset(new MockLoginDisplayHost);
    mock_login_display_ = new MockLoginDisplay();
    mock_login_utils_ = new MockLoginUtils();
    LoginUtils::Set(mock_login_utils_);

    EXPECT_CALL(*mock_login_display_host_.get(), CreateLoginDisplay(_))
        .Times(1)
        .WillOnce(Return(mock_login_display_));

    existing_user_controller_.reset(
        new ExistingUserController(mock_login_display_host_.get()));

    // Prevent settings changes from auto-starting the timer.
    CrosSettings::Get()->RemoveSettingsObserver(
        kAccountsPrefDeviceLocalAccountAutoLoginId,
        existing_user_controller());
    CrosSettings::Get()->RemoveSettingsObserver(
        kAccountsPrefDeviceLocalAccountAutoLoginDelay,
        existing_user_controller());
  }

  const ExistingUserController* existing_user_controller() const {
    return ExistingUserController::current_controller();
  }

  ExistingUserController* existing_user_controller() {
    return ExistingUserController::current_controller();
  }

  void SetAutoLoginSettings(const std::string& username, int delay) {
    CrosSettings::Get()->SetString(
        kAccountsPrefDeviceLocalAccountAutoLoginId,
        username);
    CrosSettings::Get()->SetInteger(
        kAccountsPrefDeviceLocalAccountAutoLoginDelay,
        delay);
  }

  // ExistingUserController private member accessors.
  base::OneShotTimer<ExistingUserController>* auto_login_timer() {
    return existing_user_controller()->auto_login_timer_.get();
  }

  const std::string& auto_login_username() const {
    return existing_user_controller()->public_session_auto_login_username_;
  }
  void set_auto_login_username(const std::string& username) {
    existing_user_controller()->public_session_auto_login_username_ = username;
  }

  int auto_login_delay() const {
    return existing_user_controller()->public_session_auto_login_delay_;
  }
  void set_auto_login_delay(int delay) {
    existing_user_controller()->public_session_auto_login_delay_ = delay;
  }

  bool is_login_in_progress() const {
    return existing_user_controller()->is_login_in_progress_;
  }
  void set_is_login_in_progress(bool is_login_in_progress) {
    existing_user_controller()->is_login_in_progress_ = is_login_in_progress;
  }

  void ConfigureAutoLogin() {
    existing_user_controller()->ConfigurePublicSessionAutoLogin();
  }

 private:
  // Owned by LoginUtilsWrapper.
  MockLoginUtils* mock_login_utils_;

  // |mock_login_display_| is owned by the ExistingUserController, which calls
  // CreateLoginDisplay() on the |mock_login_display_host_| to get it.
  MockLoginDisplay* mock_login_display_;

  scoped_ptr<MockLoginDisplayHost> mock_login_display_host_;
  scoped_ptr<ExistingUserController> existing_user_controller_;
  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  ScopedTestingLocalState local_state_;
  ScopedDeviceSettingsTestHelper device_settings_test_helper_;
};

TEST_F(ExistingUserControllerAutoLoginTest, StartAutoLoginTimer) {
  // Timer shouldn't start until signin screen is ready.
  set_auto_login_username(kAutoLoginUsername);
  set_auto_login_delay(kAutoLoginDelay2);
  existing_user_controller()->StartPublicSessionAutoLoginTimer();
  EXPECT_FALSE(auto_login_timer());

  // Timer shouldn't start if the policy isn't set.
  set_auto_login_username("");
  existing_user_controller()->OnSigninScreenReady();
  existing_user_controller()->StartPublicSessionAutoLoginTimer();
  EXPECT_FALSE(auto_login_timer());

  // Timer shouldn't fire in the middle of a login attempt.
  set_auto_login_username(kAutoLoginUsername);
  set_is_login_in_progress(true);
  existing_user_controller()->StartPublicSessionAutoLoginTimer();
  EXPECT_FALSE(auto_login_timer());

  // Otherwise start.
  set_is_login_in_progress(false);
  existing_user_controller()->StartPublicSessionAutoLoginTimer();
  ASSERT_TRUE(auto_login_timer());
  EXPECT_TRUE(auto_login_timer()->IsRunning());
  EXPECT_EQ(auto_login_timer()->GetCurrentDelay().InMilliseconds(),
            kAutoLoginDelay2);
}

TEST_F(ExistingUserControllerAutoLoginTest, StopAutoLoginTimer) {
  existing_user_controller()->OnSigninScreenReady();
  set_auto_login_username(kAutoLoginUsername);
  set_auto_login_delay(kAutoLoginDelay2);

  existing_user_controller()->StartPublicSessionAutoLoginTimer();
  ASSERT_TRUE(auto_login_timer());
  EXPECT_TRUE(auto_login_timer()->IsRunning());

  existing_user_controller()->StopPublicSessionAutoLoginTimer();
  ASSERT_TRUE(auto_login_timer());
  EXPECT_FALSE(auto_login_timer()->IsRunning());
}

TEST_F(ExistingUserControllerAutoLoginTest, ResetAutoLoginTimer) {
  existing_user_controller()->OnSigninScreenReady();
  set_auto_login_username(kAutoLoginUsername);

  // Timer starts off not running.
  EXPECT_FALSE(auto_login_timer());

  // When the timer isn't running, nothing should happen.
  existing_user_controller()->ResetPublicSessionAutoLoginTimer();
  EXPECT_FALSE(auto_login_timer());

  // Start the timer.
  set_auto_login_delay(kAutoLoginDelay2);
  existing_user_controller()->StartPublicSessionAutoLoginTimer();
  ASSERT_TRUE(auto_login_timer());
  EXPECT_TRUE(auto_login_timer()->IsRunning());
  EXPECT_EQ(auto_login_timer()->GetCurrentDelay().InMilliseconds(),
            kAutoLoginDelay2);

  // User activity should restart the timer, so check to see that the
  // timer delay was modified.
  set_auto_login_delay(kAutoLoginDelay1);
  existing_user_controller()->ResetPublicSessionAutoLoginTimer();
  ASSERT_TRUE(auto_login_timer());
  EXPECT_TRUE(auto_login_timer()->IsRunning());
  EXPECT_EQ(auto_login_timer()->GetCurrentDelay().InMilliseconds(),
            kAutoLoginDelay1);
}

TEST_F(ExistingUserControllerAutoLoginTest, ConfigureAutoLogin) {
  existing_user_controller()->OnSigninScreenReady();

  // Timer shouldn't start when the policy is disabled.
  ConfigureAutoLogin();
  EXPECT_FALSE(auto_login_timer());
  EXPECT_EQ(auto_login_delay(), 0);
  EXPECT_EQ(auto_login_username(), "");

  // Timer shouldn't start when the delay alone is set.
  SetAutoLoginSettings("", kAutoLoginDelay1);
  ConfigureAutoLogin();
  EXPECT_FALSE(auto_login_timer());
  EXPECT_EQ(auto_login_delay(), kAutoLoginDelay1);
  EXPECT_EQ(auto_login_username(), "");

  // Timer should start when the username is set.
  SetAutoLoginSettings(kAutoLoginUsername, kAutoLoginDelay1);
  ConfigureAutoLogin();
  ASSERT_TRUE(auto_login_timer());
  EXPECT_TRUE(auto_login_timer()->IsRunning());
  EXPECT_EQ(auto_login_timer()->GetCurrentDelay().InMilliseconds(),
            kAutoLoginDelay1);
  EXPECT_EQ(auto_login_delay(), kAutoLoginDelay1);
  EXPECT_EQ(auto_login_username(), kAutoLoginUsername);

  // Timer should restart when the delay is changed.
  SetAutoLoginSettings(kAutoLoginUsername, kAutoLoginDelay2);
  ConfigureAutoLogin();
  ASSERT_TRUE(auto_login_timer());
  EXPECT_TRUE(auto_login_timer()->IsRunning());
  EXPECT_EQ(auto_login_timer()->GetCurrentDelay().InMilliseconds(),
            kAutoLoginDelay2);
  EXPECT_EQ(auto_login_delay(), kAutoLoginDelay2);
  EXPECT_EQ(auto_login_username(), kAutoLoginUsername);

  // Timer should stop when the username is unset.
  SetAutoLoginSettings("", kAutoLoginDelay2);
  ConfigureAutoLogin();
  ASSERT_TRUE(auto_login_timer());
  EXPECT_FALSE(auto_login_timer()->IsRunning());
  EXPECT_EQ(auto_login_timer()->GetCurrentDelay().InMilliseconds(),
            kAutoLoginDelay2);
  EXPECT_EQ(auto_login_username(), "");
  EXPECT_EQ(auto_login_delay(), kAutoLoginDelay2);
}

}  // namespace chromeos
