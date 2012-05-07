// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/chromeos/cros/cros_mock.h"
#include "chrome/browser/chromeos/cros/mock_cryptohome_library.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/login_display.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/login_performer.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/mock_authenticator.h"
#include "chrome/browser/chromeos/login/mock_login_utils.h"
#include "chrome/browser/chromeos/login/mock_url_fetchers.h"
#include "chrome/browser/chromeos/login/mock_user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/common/net/gaia/mock_url_fetcher_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/dbus/mock_dbus_thread_manager.h"
#include "chromeos/dbus/mock_session_manager_client.h"
#include "grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

using ::testing::AnyNumber;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::ReturnNull;
using ::testing::WithArg;
using ::testing::_;

const char kUsername[] = "test_user@gmail.com";
const char kNewUsername[] = "test_new_user@gmail.com";
const char kPassword[] = "test_password";

class MockLoginDisplay : public LoginDisplay {
 public:
  MockLoginDisplay()
      : LoginDisplay(NULL, gfx::Rect()) {
  }

  MOCK_METHOD4(Init, void(const UserList&, bool, bool, bool));
  MOCK_METHOD0(OnPreferencesChanged, void(void));
  MOCK_METHOD1(OnUserImageChanged, void(const User&));
  MOCK_METHOD0(OnFadeOut, void(void));
  MOCK_METHOD1(OnLoginSuccess, void(const std::string&));
  MOCK_METHOD1(SetUIEnabled, void(bool));
  MOCK_METHOD1(SelectPod, void(int));
  MOCK_METHOD3(ShowError, void(int, int, HelpAppLauncher::HelpTopic));
  MOCK_METHOD1(ShowGaiaPasswordChanged, void(const std::string&));
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
  MOCK_CONST_METHOD0(GetWidget, views::Widget*(void));
  MOCK_METHOD0(OnSessionStart, void(void));
  MOCK_METHOD0(OnCompleteLogin, void(void));
  MOCK_METHOD0(OpenProxySettings, void(void));
  MOCK_METHOD1(SetOobeProgressBarVisible, void(bool));
  MOCK_METHOD1(SetShutdownButtonEnabled, void(bool));
  MOCK_METHOD1(SetStatusAreaVisible, void(bool));
  MOCK_METHOD0(ShowBackground, void(void));
  MOCK_METHOD0(CheckForAutoEnrollment, void(void));
  MOCK_METHOD2(StartWizard, void(const std::string&, DictionaryValue*));
  MOCK_METHOD0(StartSignInScreen, void(void));
  MOCK_METHOD0(ResumeSignInScreen, void(void));
  MOCK_METHOD0(OnPreferencesChanged, void(void));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockLoginDisplayHost);
};

class ExistingUserControllerTest : public CrosInProcessBrowserTest {
 protected:
  ExistingUserControllerTest()
      : mock_cryptohome_library_(NULL),
        mock_network_library_(NULL),
        mock_login_display_(NULL),
        mock_login_display_host_(NULL),
        testing_profile_(NULL) {
  }

  ExistingUserController* existing_user_controller() {
    return ExistingUserController::current_controller();
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    CrosInProcessBrowserTest::CleanUpOnMainThread();
    testing_profile_.reset(NULL);
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    MockDBusThreadManager* mock_dbus_thread_manager =
        new MockDBusThreadManager;
    DBusThreadManager::InitializeForTesting(mock_dbus_thread_manager);
    CrosInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    cros_mock_->InitStatusAreaMocks();
    cros_mock_->SetStatusAreaMocksExpectations();

    mock_network_library_ = cros_mock_->mock_network_library();
    EXPECT_CALL(*mock_network_library_, AddUserActionObserver(_))
        .Times(AnyNumber());
    MockSessionManagerClient* mock_session_manager_client =
        mock_dbus_thread_manager->mock_session_manager_client();
    EXPECT_CALL(*mock_session_manager_client, EmitLoginPromptReady())
        .Times(1);
    EXPECT_CALL(*mock_session_manager_client, RetrieveDevicePolicy(_))
        .Times(AnyNumber());

    cros_mock_->InitMockCryptohomeLibrary();
    mock_cryptohome_library_ = cros_mock_->mock_cryptohome_library();
    EXPECT_CALL(*mock_cryptohome_library_, IsMounted())
        .Times(AnyNumber())
        .WillRepeatedly(Return(true));

    mock_login_utils_ = new MockLoginUtils();
    LoginUtils::Set(mock_login_utils_);
    EXPECT_CALL(*mock_login_utils_, PrewarmAuthentication())
        .Times(AnyNumber());
    EXPECT_CALL(*mock_login_utils_, StopBackgroundFetchers())
        .Times(AnyNumber());

    mock_login_display_.reset(new MockLoginDisplay());
    mock_login_display_host_.reset(new MockLoginDisplayHost());

    EXPECT_CALL(*mock_user_manager_.user_manager(), IsKnownUser(kUsername))
        .Times(AnyNumber())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_user_manager_.user_manager(), IsKnownUser(kNewUsername))
        .Times(AnyNumber())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_user_manager_.user_manager(), IsUserLoggedIn())
        .Times(AnyNumber())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_user_manager_.user_manager(), IsLoggedInAsGuest())
        .Times(AnyNumber())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_user_manager_.user_manager(), IsLoggedInAsDemoUser())
        .Times(AnyNumber())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_user_manager_.user_manager(), IsSessionStarted())
        .Times(AnyNumber())
        .WillRepeatedly(Return(false));

    EXPECT_CALL(*mock_login_display_host_.get(), CreateLoginDisplay(_))
        .Times(1)
        .WillOnce(Return(mock_login_display_.get()));
    EXPECT_CALL(*mock_login_display_host_.get(), GetNativeWindow())
        .Times(1)
        .WillOnce(ReturnNull());
    EXPECT_CALL(*mock_login_display_.get(), Init(_, false, true, true))
        .Times(1);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    testing_profile_.reset(new TestingProfile());
    ExistingUserController* controller =
        new ExistingUserController(mock_login_display_host_.get());
    controller->Init(UserList());
    profile_prepared_cb_ =
        base::Bind(&ExistingUserController::OnProfilePrepared,
                   base::Unretained(existing_user_controller()),
                   testing_profile_.get());
  }

  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE {
    CrosInProcessBrowserTest::TearDownInProcessBrowserTestFixture();
    DBusThreadManager::Shutdown();
  }

  // These mocks are owned by CrosLibrary class.
  MockCryptohomeLibrary* mock_cryptohome_library_;
  MockNetworkLibrary* mock_network_library_;

  scoped_ptr<MockLoginDisplay> mock_login_display_;
  scoped_ptr<MockLoginDisplayHost> mock_login_display_host_;

  ScopedMockUserManagerEnabler mock_user_manager_;

  // Owned by LoginUtilsWrapper.
  MockLoginUtils* mock_login_utils_;

  scoped_ptr<TestingProfile> testing_profile_;

  // Mock URLFetcher.
  MockURLFetcherFactory<SuccessFetcher> factory_;

  base::Callback<void(void)> profile_prepared_cb_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExistingUserControllerTest);
};

scoped_refptr<Authenticator> CreateAuthenticator(
    LoginStatusConsumer* consumer) {
  return new MockAuthenticator(consumer, kUsername, kPassword);
}

scoped_refptr<Authenticator> CreateAuthenticatorNewUser(
    LoginStatusConsumer* consumer) {
  return new MockAuthenticator(consumer, kNewUsername, kPassword);
}

// These tests are failing under ASan, but not natively.
// See http://crbug.com/126224
#if defined(ADDRESS_SANITIZER)
#define MAYBE_ExistingUserLogin DISABLED_ExistingUserLogin
#define MAYBE_AutoEnrollAfterSignIn DISABLED_AutoEnrollAfterSignIn
#define MAYBE_NewUserDontAutoEnrollAfterSignIn \
    DISABLED_NewUserDontAutoEnrollAfterSignIn
#else
#define MAYBE_ExistingUserLogin ExistingUserLogin
#define MAYBE_AutoEnrollAfterSignIn AutoEnrollAfterSignIn
#define MAYBE_NewUserDontAutoEnrollAfterSignIn NewUserDontAutoEnrollAfterSignIn
#endif  // defined(ADDRESS_SANITIZER)
IN_PROC_BROWSER_TEST_F(ExistingUserControllerTest, MAYBE_ExistingUserLogin) {
  EXPECT_CALL(*mock_login_display_, SetUIEnabled(false))
      .Times(1);
  EXPECT_CALL(*mock_login_utils_, CreateAuthenticator(_))
      .Times(1)
      .WillOnce(WithArg<0>(Invoke(CreateAuthenticator)));
  EXPECT_CALL(*mock_login_utils_,
              PrepareProfile(kUsername, _, kPassword, false, _, _, _))
      .Times(1)
      .WillOnce(InvokeWithoutArgs(&profile_prepared_cb_,
                                  &base::Callback<void(void)>::Run));
  EXPECT_CALL(*mock_login_utils_,
              DoBrowserLaunch(testing_profile_.get(),
                              mock_login_display_host_.get()))
      .Times(1);
  EXPECT_CALL(*mock_login_display_.get(), OnLoginSuccess(kUsername))
      .Times(1);
  EXPECT_CALL(*mock_login_display_.get(), OnFadeOut())
      .Times(1);
  EXPECT_CALL(*mock_login_display_host_,
              StartWizard(WizardController::kUserImageScreenName, NULL))
      .Times(0);
  existing_user_controller()->Login(kUsername, kPassword);
  ui_test_utils::RunAllPendingInMessageLoop();
}

IN_PROC_BROWSER_TEST_F(ExistingUserControllerTest,
                       MAYBE_AutoEnrollAfterSignIn) {
  EXPECT_CALL(*mock_login_display_host_,
              StartWizard(WizardController::kEnterpriseEnrollmentScreenName, _))
      .Times(1);
  EXPECT_CALL(*mock_login_display_.get(), OnFadeOut())
      .Times(1);
  EXPECT_CALL(*mock_login_display_host_.get(), OnCompleteLogin())
      .Times(1);
  existing_user_controller()->DoAutoEnrollment();
  existing_user_controller()->CompleteLogin(kUsername, kPassword);
  ui_test_utils::RunAllPendingInMessageLoop();
}

IN_PROC_BROWSER_TEST_F(ExistingUserControllerTest,
                       MAYBE_NewUserDontAutoEnrollAfterSignIn) {
  EXPECT_CALL(*mock_login_display_host_,
              StartWizard(WizardController::kEnterpriseEnrollmentScreenName, _))
      .Times(0);
  // That will be sign in of a new user and (legacy) registration screen is
  // activated. In a real WizardController instance that is immediately switched
  // to image screen but this tests uses MockLoginDisplayHost instead.
  EXPECT_CALL(*mock_login_display_host_,
              StartWizard(WizardController::kRegistrationScreenName, _))
      .Times(1);
  EXPECT_CALL(*mock_login_utils_, CreateAuthenticator(_))
      .Times(1)
      .WillOnce(WithArg<0>(Invoke(CreateAuthenticatorNewUser)));
  EXPECT_CALL(*mock_login_utils_,
              PrepareProfile(kNewUsername, _, kPassword, false, _, _, _))
      .Times(1)
      .WillOnce(InvokeWithoutArgs(&profile_prepared_cb_,
                                  &base::Callback<void(void)>::Run));
  EXPECT_CALL(*mock_login_display_.get(), OnLoginSuccess(kNewUsername))
      .Times(1);
  EXPECT_CALL(*mock_login_display_.get(), OnFadeOut())
      .Times(1);
  EXPECT_CALL(*mock_login_display_host_.get(), OnCompleteLogin())
      .Times(1);
  existing_user_controller()->CompleteLogin(kNewUsername, kPassword);
  ui_test_utils::RunAllPendingInMessageLoop();
}

}  // namespace chromeos
