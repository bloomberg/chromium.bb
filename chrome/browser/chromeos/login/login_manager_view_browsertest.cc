// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/chromeos/cros/mock_cryptohome_library.h"
#include "chrome/browser/chromeos/cros/mock_login_library.h"
#include "chrome/browser/chromeos/login/login_manager_view.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/mock_screen_observer.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/login/wizard_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

class LoginManagerViewTest : public WizardInProcessBrowserTest {
 public:
  LoginManagerViewTest(): WizardInProcessBrowserTest("login") {
  }

 protected:
  virtual void SetUpInProcessBrowserTestFixture() {
    WizardInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
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
    WizardInProcessBrowserTest::TearDownInProcessBrowserTestFixture();
    test_api()->SetLoginLibrary(NULL);
    test_api()->SetCryptohomeLibrary(NULL);
  }

 private:
  MockLoginLibrary* mock_login_library_;
  MockCryptohomeLibrary* mock_cryptohome_library_;

  DISALLOW_COPY_AND_ASSIGN(LoginManagerViewTest);
};

IN_PROC_BROWSER_TEST_F(LoginManagerViewTest, TestBasic) {
  ASSERT_TRUE(controller() != NULL);
  ASSERT_EQ(controller()->current_screen(), controller()->GetLoginScreen());

  scoped_ptr<MockScreenObserver> mock_screen_observer(
      new MockScreenObserver());
  EXPECT_CALL(*mock_screen_observer,
              OnExit(ScreenObserver::LOGIN_SIGN_IN_SELECTED))
      .Times(1);

  LoginManagerView* login = controller()->GetLoginScreen()->view();
  login->set_observer(mock_screen_observer.get());
  login->SetUsername(kUsername);
  login->SetPassword(kPassword);
  login->Login();
  login->set_observer(NULL);
}

}  // namespace chromeos
