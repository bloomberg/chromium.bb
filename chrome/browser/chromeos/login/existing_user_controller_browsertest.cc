// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/chromeos/cros/cros_mock.h"
#include "chrome/browser/chromeos/cros/mock_cryptohome_library.h"
#include "chrome/browser/chromeos/cros/mock_login_library.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/login_performer.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/mock_authenticator.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/login/wizard_in_process_browser_test.h"
#include "grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

using ::testing::AnyNumber;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

const char kUsername[] = "test_user@gmail.com";
const char kPassword[] = "test_password";

class MockLoginPerformerDelegate : public LoginPerformer::Delegate {
 public:
  explicit MockLoginPerformerDelegate(ExistingUserController* controller)
      : controller_(controller) {
  }

  void OnLoginSuccess(const std::string&,
                      const std::string&,
                      const GaiaAuthConsumer::ClientLoginResult&,
                      bool) {
    LoginPerformer* login_performer = controller_->login_performer_.release();
    login_performer = NULL;
    controller_->ActivateWizard(WizardController::kUserImageScreenName);
    delete WizardController::default_controller();
  }

  MOCK_METHOD1(OnLoginFailure, void(const LoginFailure&));
  MOCK_METHOD1(WhiteListCheckFailed, void(const std::string&));

 private:
  ExistingUserController* controller_;

  DISALLOW_COPY_AND_ASSIGN(MockLoginPerformerDelegate);
};

class ExistingUserControllerTest : public WizardInProcessBrowserTest {
 protected:
  ExistingUserControllerTest()
      : chromeos::WizardInProcessBrowserTest(""),
        mock_cryptohome_library_(NULL),
        mock_login_library_(NULL),
        mock_network_library_(NULL) {
  }

  virtual void SetUpWizard() {
    gfx::Rect background_bounds(login::kWizardScreenWidth,
                                login::kWizardScreenHeight);
    ExistingUserController* controller =
        new ExistingUserController(std::vector<UserManager::User>(),
                                   background_bounds);
    controller->Init();
  }

  ExistingUserController* existing_user_controller() {
    return ExistingUserController::current_controller();
  }

  virtual void SetUpInProcessBrowserTestFixture() {
    WizardInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    cros_mock_->InitStatusAreaMocks();
    cros_mock_->SetStatusAreaMocksExpectations();

    mock_network_library_ = cros_mock_->mock_network_library();

    mock_login_library_ = new MockLoginLibrary();
    EXPECT_CALL(*mock_login_library_, EmitLoginPromptReady())
        .Times(1);
    EXPECT_CALL(*mock_login_library_, RetrieveProperty(_, _, _))
        .Times(AnyNumber())
        .WillRepeatedly((Return(true)));
    cros_mock_->test_api()->SetLoginLibrary(mock_login_library_, true);

    cros_mock_->InitMockCryptohomeLibrary();
    mock_cryptohome_library_ = cros_mock_->mock_cryptohome_library();
    EXPECT_CALL(*mock_cryptohome_library_, IsMounted())
        .Times(AnyNumber())
        .WillRepeatedly((Return(true)));
    LoginUtils::Set(new MockLoginUtils(kUsername, kPassword));
  }

  virtual void TearDownInProcessBrowserTestFixture() {
    WizardInProcessBrowserTest::TearDownInProcessBrowserTestFixture();
    cros_mock_->test_api()->SetLoginLibrary(NULL, false);
  }

  MockCryptohomeLibrary* mock_cryptohome_library_;
  MockLoginLibrary* mock_login_library_;
  MockNetworkLibrary* mock_network_library_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExistingUserControllerTest);
};

IN_PROC_BROWSER_TEST_F(ExistingUserControllerTest, NewUserLogin) {
  MockLoginPerformerDelegate* mock_delegate =
      new MockLoginPerformerDelegate(existing_user_controller());
  existing_user_controller()->set_login_performer_delegate(mock_delegate);

  existing_user_controller()->LoginNewUser(kUsername, kPassword);
}

}  // namespace chromeos
