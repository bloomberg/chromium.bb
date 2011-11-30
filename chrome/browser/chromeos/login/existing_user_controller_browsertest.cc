// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/cros/cros_mock.h"
#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/chromeos/cros/mock_cryptohome_library.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/browser/chromeos/dbus/mock_dbus_thread_manager.h"
#include "chrome/browser/chromeos/dbus/mock_session_manager_client.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/login_display.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/login_performer.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/mock_authenticator.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

using ::testing::AnyNumber;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::ReturnNull;

const char kUsername[] = "test_user@gmail.com";
const char kPassword[] = "test_password";

class MockLoginDisplay : public LoginDisplay {
 public:
  MockLoginDisplay()
      : LoginDisplay(NULL, gfx::Rect()) {
  }

  MOCK_METHOD3(Init, void(const UserList&, bool, bool));
  MOCK_METHOD1(OnUserImageChanged, void(const User&));
  MOCK_METHOD0(OnFadeOut, void(void));
  MOCK_METHOD1(OnLoginSuccess, void(const std::string&));
  MOCK_METHOD1(SetUIEnabled, void(bool));
  MOCK_METHOD1(SelectPod, void(int));
  MOCK_METHOD3(ShowError, void(int, int, HelpAppLauncher::HelpTopic));
  MOCK_METHOD1(OnBeforeUserRemoved, void(const std::string&));
  MOCK_METHOD1(OnUserRemoved, void(const std::string&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockLoginDisplay);
};

class MockLoginDisplayHost : public LoginDisplayHost {
 public:
  MockLoginDisplayHost() {
  }

  MOCK_METHOD1(CreateLoginDisplay, LoginDisplay*(LoginDisplay::Delegate*));
  MOCK_CONST_METHOD0(GetNativeWindow, gfx::NativeWindow(void));
  MOCK_METHOD0(OnSessionStart, void(void));
  MOCK_METHOD1(SetOobeProgress, void(BackgroundView::LoginStep));
  MOCK_METHOD1(SetOobeProgressBarVisible, void(bool));
  MOCK_METHOD1(SetShutdownButtonEnabled, void(bool));
  MOCK_METHOD1(SetStatusAreaEnabled, void(bool));
  MOCK_METHOD1(SetStatusAreaVisible, void(bool));
  MOCK_METHOD0(ShowBackground, void(void));
  MOCK_METHOD2(StartWizard, void(const std::string&,
                                 const GURL&));
  MOCK_METHOD0(StartSignInScreen, void(void));

 private:
 DISALLOW_COPY_AND_ASSIGN(MockLoginDisplayHost);
};

class MockLoginPerformerDelegate : public LoginPerformer::Delegate {
 public:
  explicit MockLoginPerformerDelegate(ExistingUserController* controller)
      : controller_(controller) {
  }

  void OnLoginSuccess(const std::string&,
                      const std::string&,
                      const GaiaAuthConsumer::ClientLoginResult&,
                      bool, bool) {
    ignore_result(controller_->login_performer_.release());
    controller_->ActivateWizard(WizardController::kUserImageScreenName);
  }

  MOCK_METHOD1(OnLoginFailure, void(const LoginFailure&));
  MOCK_METHOD1(WhiteListCheckFailed, void(const std::string&));

 private:
  ExistingUserController* controller_;

  DISALLOW_COPY_AND_ASSIGN(MockLoginPerformerDelegate);
};

class ExistingUserControllerTest : public CrosInProcessBrowserTest {
 protected:
  ExistingUserControllerTest()
      : mock_cryptohome_library_(NULL),
        mock_network_library_(NULL),
        mock_login_display_(NULL),
        mock_login_display_host_(NULL) {
  }

  ExistingUserController* existing_user_controller() {
    return ExistingUserController::current_controller();
  }

  virtual void SetUpInProcessBrowserTestFixture() {
    MockDBusThreadManager* mock_dbus_thread_manager =
        new MockDBusThreadManager;
    DBusThreadManager::InitializeForTesting(mock_dbus_thread_manager);
    CrosInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    cros_mock_->InitStatusAreaMocks();
    cros_mock_->SetStatusAreaMocksExpectations();

    mock_network_library_ = cros_mock_->mock_network_library();
    MockSessionManagerClient* mock_session_manager_client =
        mock_dbus_thread_manager->mock_session_manager_client();
    EXPECT_CALL(*mock_session_manager_client, EmitLoginPromptReady())
        .Times(1);
    EXPECT_CALL(*mock_session_manager_client, RetrievePolicy(_))
        .Times(AnyNumber());

    cros_mock_->InitMockCryptohomeLibrary();
    mock_cryptohome_library_ = cros_mock_->mock_cryptohome_library();
    EXPECT_CALL(*mock_cryptohome_library_, IsMounted())
        .Times(AnyNumber())
        .WillRepeatedly(Return(true));
    LoginUtils::Set(new MockLoginUtils(kUsername, kPassword));

    mock_login_display_.reset(new MockLoginDisplay());
    mock_login_display_host_.reset(new MockLoginDisplayHost());

    EXPECT_CALL(*mock_login_display_host_.get(), CreateLoginDisplay(_))
        .Times(1)
        .WillOnce(Return(mock_login_display_.get()));
    EXPECT_CALL(*mock_login_display_host_.get(), GetNativeWindow())
        .Times(1)
        .WillOnce(ReturnNull());
    EXPECT_CALL(*mock_login_display_.get(), Init(_, false, true))
        .Times(1);
  }

  virtual void SetUpOnMainThread() {
    ExistingUserController* controller =
        new ExistingUserController(mock_login_display_host_.get());
    controller->Init(UserList());
    MockLoginPerformerDelegate* mock_delegate =
          new MockLoginPerformerDelegate(controller);
    existing_user_controller()->set_login_performer_delegate(mock_delegate);
  }

  virtual void TearDownInProcessBrowserTestFixture() {
    CrosInProcessBrowserTest::TearDownInProcessBrowserTestFixture();
    DBusThreadManager::Shutdown();
  }

  // These mocks are owned by CrosLibrary class.
  MockCryptohomeLibrary* mock_cryptohome_library_;
  MockNetworkLibrary* mock_network_library_;

  scoped_ptr<MockLoginDisplay> mock_login_display_;
  scoped_ptr<MockLoginDisplayHost> mock_login_display_host_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExistingUserControllerTest);
};

IN_PROC_BROWSER_TEST_F(ExistingUserControllerTest, NewUserLogin) {
  EXPECT_CALL(*mock_login_display_host_, SetStatusAreaEnabled(false))
      .Times(1);
  EXPECT_CALL(*mock_login_display_, SetUIEnabled(false))
      .Times(1);
  EXPECT_CALL(*mock_login_display_host_,
              StartWizard(WizardController::kUserImageScreenName,
                          GURL()))
      .Times(1);
  existing_user_controller()->Login(kUsername, kPassword);
}

}  // namespace chromeos
