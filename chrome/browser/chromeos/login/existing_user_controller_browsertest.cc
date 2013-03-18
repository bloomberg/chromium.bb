// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/chromeos/cros/cros_mock.h"
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
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/dbus/mock_dbus_thread_manager.h"
#include "chromeos/dbus/mock_session_manager_client.h"
#include "chromeos/dbus/mock_shill_manager_client.h"
#include "google_apis/gaia/mock_url_fetcher_factory.h"
#include "grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AnyOf;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::ReturnNull;
using ::testing::Sequence;
using ::testing::WithArg;

namespace chromeos {

namespace {

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
  MOCK_METHOD1(ShowErrorScreen, void(LoginDisplay::SigninError));
  MOCK_METHOD1(ShowGaiaPasswordChanged, void(const std::string&));
  MOCK_METHOD1(ShowPasswordChangedDialog, void(bool));
  MOCK_METHOD1(ShowSigninUI, void(const std::string&));
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
  MOCK_METHOD0(BeforeSessionStart, void(void));
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

scoped_refptr<Authenticator> CreateAuthenticator(
    LoginStatusConsumer* consumer) {
  return new MockAuthenticator(consumer, kUsername, kPassword);
}

scoped_refptr<Authenticator> CreateAuthenticatorNewUser(
    LoginStatusConsumer* consumer) {
  return new MockAuthenticator(consumer, kNewUsername, kPassword);
}

}  // namespace

class ExistingUserControllerTest : public CrosInProcessBrowserTest {
 protected:
  ExistingUserControllerTest()
      : mock_network_library_(NULL),
        mock_login_display_(NULL),
        mock_login_display_host_(NULL),
        testing_profile_(NULL) {
  }

  ExistingUserController* existing_user_controller() {
    return ExistingUserController::current_controller();
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    MockDBusThreadManager* mock_dbus_thread_manager =
        new MockDBusThreadManager;
    EXPECT_CALL(*mock_dbus_thread_manager, GetSystemBus())
        .WillRepeatedly(Return(reinterpret_cast<dbus::Bus*>(NULL)));
    EXPECT_CALL(*mock_dbus_thread_manager, GetIBusInputContextClient())
        .WillRepeatedly(
            Return(reinterpret_cast<IBusInputContextClient*>(NULL)));
    EXPECT_CALL(*mock_dbus_thread_manager->mock_shill_manager_client(),
                GetProperties(_))
        .Times(AnyNumber());
    EXPECT_CALL(*mock_dbus_thread_manager->mock_shill_manager_client(),
                AddPropertyChangedObserver(_))
        .Times(AnyNumber());
    EXPECT_CALL(*mock_dbus_thread_manager->mock_shill_manager_client(),
                RemovePropertyChangedObserver(_))
        .Times(AnyNumber());
    DBusThreadManager::InitializeForTesting(mock_dbus_thread_manager);
    CrosInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    cros_mock_->InitStatusAreaMocks();
    cros_mock_->SetStatusAreaMocksExpectations();

    mock_network_library_ = cros_mock_->mock_network_library();
    EXPECT_CALL(*mock_network_library_, AddUserActionObserver(_))
        .Times(AnyNumber());
    EXPECT_CALL(*mock_network_library_, LoadOncNetworks(_, _, _, _))
        .WillRepeatedly(Return(true));

    MockSessionManagerClient* mock_session_manager_client =
        mock_dbus_thread_manager->mock_session_manager_client();
    EXPECT_CALL(*mock_session_manager_client, EmitLoginPromptReady())
        .Times(1);
    EXPECT_CALL(*mock_session_manager_client, RetrieveDevicePolicy(_))
        .Times(AnyNumber());

    mock_login_utils_ = new MockLoginUtils();
    LoginUtils::Set(mock_login_utils_);
    EXPECT_CALL(*mock_login_utils_, PrewarmAuthentication())
        .Times(AnyNumber());
    EXPECT_CALL(*mock_login_utils_, StopBackgroundFetchers())
        .Times(AnyNumber());
    EXPECT_CALL(*mock_login_utils_, DelegateDeleted(_))
        .Times(1);

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
    EXPECT_CALL(*mock_user_manager_.user_manager(), IsLoggedInAsPublicAccount())
        .Times(AnyNumber())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_user_manager_.user_manager(), IsSessionStarted())
        .Times(AnyNumber())
        .WillRepeatedly(Return(false));

    // |mock_login_display_| is owned by the ExistingUserController, which calls
    // CreateLoginDisplay() on the |mock_login_display_host_| to get it.
    mock_login_display_ = new MockLoginDisplay();
    EXPECT_CALL(*mock_login_display_host_.get(), CreateLoginDisplay(_))
        .Times(1)
        .WillOnce(Return(mock_login_display_));
    EXPECT_CALL(*mock_login_display_host_.get(), GetNativeWindow())
        .Times(1)
        .WillOnce(ReturnNull());
    EXPECT_CALL(*mock_login_display_host_.get(), OnPreferencesChanged())
        .Times(1);
    EXPECT_CALL(*mock_login_display_, Init(_, false, true, true))
        .Times(1);
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kLoginManager);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    testing_profile_.reset(new TestingProfile());
    existing_user_controller_.reset(
        new ExistingUserController(mock_login_display_host_.get()));
    ASSERT_EQ(existing_user_controller(), existing_user_controller_.get());
    existing_user_controller_->Init(UserList());
    profile_prepared_cb_ =
        base::Bind(&ExistingUserController::OnProfilePrepared,
                   base::Unretained(existing_user_controller()),
                   testing_profile_.get());
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    CrosInProcessBrowserTest::CleanUpOnMainThread();
    testing_profile_.reset(NULL);
  }

  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE {
    CrosInProcessBrowserTest::TearDownInProcessBrowserTestFixture();
    DBusThreadManager::Shutdown();
  }

  scoped_ptr<ExistingUserController> existing_user_controller_;

  // These mocks are owned by CrosLibrary class.
  MockNetworkLibrary* mock_network_library_;

  MockLoginDisplay* mock_login_display_;
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

IN_PROC_BROWSER_TEST_F(ExistingUserControllerTest, ExistingUserLogin) {
  // This is disabled twice: once right after signin but before checking for
  // auto-enrollment, and again after doing an ownership status check.
  EXPECT_CALL(*mock_login_display_, SetUIEnabled(false))
      .Times(2);
  EXPECT_CALL(*mock_login_utils_, CreateAuthenticator(_))
      .Times(1)
      .WillOnce(WithArg<0>(Invoke(CreateAuthenticator)));
  EXPECT_CALL(*mock_login_utils_,
              PrepareProfile(kUsername, _, kPassword, _, _, _))
      .Times(1)
      .WillOnce(InvokeWithoutArgs(&profile_prepared_cb_,
                                  &base::Callback<void(void)>::Run));
  EXPECT_CALL(*mock_login_utils_,
              DoBrowserLaunch(testing_profile_.get(),
                              mock_login_display_host_.get()))
      .Times(1);
  EXPECT_CALL(*mock_login_display_, OnLoginSuccess(kUsername))
      .Times(1);
  EXPECT_CALL(*mock_login_display_, SetUIEnabled(true))
      .Times(1);
  EXPECT_CALL(*mock_login_display_, OnFadeOut())
      .Times(1);
  EXPECT_CALL(*mock_login_display_host_,
              StartWizard(WizardController::kTermsOfServiceScreenName, NULL))
      .Times(0);
  EXPECT_CALL(*mock_user_manager_.user_manager(), IsCurrentUserNew())
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));
  existing_user_controller()->Login(kUsername, kPassword);
  content::RunAllPendingInMessageLoop();
}

IN_PROC_BROWSER_TEST_F(ExistingUserControllerTest, AutoEnrollAfterSignIn) {
  EXPECT_CALL(*mock_login_display_host_,
              StartWizard(WizardController::kEnterpriseEnrollmentScreenName, _))
      .Times(1);
  EXPECT_CALL(*mock_login_display_, OnFadeOut())
      .Times(1);
  EXPECT_CALL(*mock_login_display_host_.get(), OnCompleteLogin())
      .Times(1);
  EXPECT_CALL(*mock_user_manager_.user_manager(), IsCurrentUserNew())
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));
  // The order of these expected calls matters: the UI if first disabled
  // during the login sequence, and is enabled again for the enrollment screen.
  Sequence uiEnabledSequence;
  EXPECT_CALL(*mock_login_display_, SetUIEnabled(false))
      .Times(1)
      .InSequence(uiEnabledSequence);
  EXPECT_CALL(*mock_login_display_, SetUIEnabled(true))
      .Times(1)
      .InSequence(uiEnabledSequence);
  existing_user_controller()->DoAutoEnrollment();
  existing_user_controller()->CompleteLogin(kUsername, kPassword);
  content::RunAllPendingInMessageLoop();
}

IN_PROC_BROWSER_TEST_F(ExistingUserControllerTest,
                       NewUserDontAutoEnrollAfterSignIn) {
  EXPECT_CALL(*mock_login_display_host_,
              StartWizard(WizardController::kEnterpriseEnrollmentScreenName, _))
      .Times(0);
  // This will be the first sign-in of a new user, which may cause the (legacy)
  // registration to be activated. A real WizardController instance immediately
  // advances to the Terms of Service or user image screen but this test uses
  // MockLoginDisplayHost Instead.
  EXPECT_CALL(*mock_login_display_host_,
              StartWizard(AnyOf(WizardController::kRegistrationScreenName,
                                WizardController::kTermsOfServiceScreenName),
                          NULL))
      .Times(1);
  EXPECT_CALL(*mock_login_utils_, CreateAuthenticator(_))
      .Times(1)
      .WillOnce(WithArg<0>(Invoke(CreateAuthenticatorNewUser)));
  EXPECT_CALL(*mock_login_utils_,
              PrepareProfile(kNewUsername, _, kPassword, _, _, _))
      .Times(1)
      .WillOnce(InvokeWithoutArgs(&profile_prepared_cb_,
                                  &base::Callback<void(void)>::Run));
  EXPECT_CALL(*mock_login_display_, OnLoginSuccess(kNewUsername))
      .Times(1);
  EXPECT_CALL(*mock_login_display_, OnFadeOut())
      .Times(1);
  EXPECT_CALL(*mock_login_display_host_.get(), OnCompleteLogin())
      .Times(1);
  EXPECT_CALL(*mock_user_manager_.user_manager(), IsCurrentUserNew())
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));

  // The order of these expected calls matters: the UI if first disabled
  // during the login sequence, and is enabled again after login completion.
  Sequence uiEnabledSequence;
  // This is disabled twice: once right after signin but before checking for
  // auto-enrollment, and again after doing an ownership status check.
  EXPECT_CALL(*mock_login_display_, SetUIEnabled(false))
      .Times(2)
      .InSequence(uiEnabledSequence);
  EXPECT_CALL(*mock_login_display_, SetUIEnabled(true))
      .Times(1)
      .InSequence(uiEnabledSequence);

  existing_user_controller()->CompleteLogin(kNewUsername, kPassword);
  content::RunAllPendingInMessageLoop();
}

}  // namespace chromeos
