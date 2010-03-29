// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/chromeos/cros/mock_cryptohome_library.h"
#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/chromeos/cros/mock_login_library.h"
#include "chrome/browser/chromeos/login/login_manager_view.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"
#include "chrome/common/chrome_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

using ::testing::AnyNumber;
using ::testing::Return;

const char kUsername[] = "test_user@gmail.com";
const char kPassword[] = "test_password";

class MockAuthenticator : public Authenticator {
 public:
  explicit MockAuthenticator(LoginStatusConsumer* consumer,
                             const std::string& expected_username,
                             const std::string& expected_password)
      : Authenticator(consumer),
        expected_username_(expected_username),
        expected_password_(expected_password) {
  }

  // Returns true after calling OnLoginSuccess().
  virtual bool Authenticate(const std::string& username,
                            const std::string& password) {
    EXPECT_EQ(expected_username_, username);
    EXPECT_EQ(expected_password_, password);
    consumer_->OnLoginSuccess(username, std::vector<std::string>());
    return true;
  }

 private:
  std::string expected_username_;
  std::string expected_password_;

  DISALLOW_COPY_AND_ASSIGN(MockAuthenticator);
};

class MockLoginUtils : public LoginUtils {
 public:
  explicit MockLoginUtils(const std::string& expected_username,
                          const std::string& expected_password)
      : expected_username_(expected_username),
        expected_password_(expected_password) {
  }

  virtual void CompleteLogin(const std::string& username,
                             std::vector<std::string> cookies) {
    EXPECT_EQ(expected_username_, username);
  }

  virtual Authenticator* CreateAuthenticator(LoginStatusConsumer* consumer) {
    return new MockAuthenticator(
        consumer, expected_username_, expected_password_);
  }

 private:
  std::string expected_username_;
  std::string expected_password_;

  DISALLOW_COPY_AND_ASSIGN(MockLoginUtils);
};

class LoginManagerViewTest : public CrosInProcessBrowserTest {
 public:
  LoginManagerViewTest() {
  }

 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitchWithValue(switches::kLoginScreen,
                                        WizardController::kLoginScreenName);
  }

  virtual Browser* CreateBrowser(Profile* profile) {
    // Don't need to create separate browser window for OOBE wizard.
    return NULL;
  }

  virtual void SetUpInProcessBrowserTestFixture() {
    InitStatusAreaMocks();
    SetStatusAreaMocksExpectations();

    mock_login_library_ = new MockLoginLibrary();
    EXPECT_CALL(*mock_login_library_, EmitLoginPromptReady())
        .Times(1);
    test_api()->SetLoginLibrary(mock_login_library_);

    mock_cryptohome_library_ = new MockCryptohomeLibrary();
    EXPECT_CALL(*mock_cryptohome_library_, IsMounted())
        .Times(AnyNumber())
        .WillRepeatedly((Return(true)));
    test_api()->SetCryptohomeLibrary(mock_cryptohome_library_);

    LoginUtils::Set(new MockLoginUtils(kUsername, kPassword));
  }

  virtual void TearDownInProcessBrowserTestFixture() {
    CrosInProcessBrowserTest::TearDownInProcessBrowserTestFixture();
    test_api()->SetLoginLibrary(NULL);
  }

 private:
  MockLoginLibrary* mock_login_library_;
  MockCryptohomeLibrary* mock_cryptohome_library_;

  DISALLOW_COPY_AND_ASSIGN(LoginManagerViewTest);
};

IN_PROC_BROWSER_TEST_F(LoginManagerViewTest, TestBasic) {
  WizardController* controller = WizardController::default_controller();
  ASSERT_TRUE(controller != NULL);
  ASSERT_EQ(controller->current_screen(), controller->GetLoginScreen());
  LoginManagerView* login = controller->GetLoginScreen()->view();
  login->SetUsername(kUsername);
  login->SetPassword(kPassword);
  login->Login();

  // End the message loop to quit the test.
  MessageLoop::current()->Quit();
}

}  // namespace chromeos
