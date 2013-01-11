// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_screen.h"
#include "chrome/browser/chromeos/login/enrollment/mock_enterprise_enrollment_screen.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/language_switch_menu.h"
#include "chrome/browser/chromeos/login/mock_authenticator.h"
#include "chrome/browser/chromeos/login/mock_eula_screen.h"
#include "chrome/browser/chromeos/login/mock_login_status_consumer.h"
#include "chrome/browser/chromeos/login/mock_network_screen.h"
#include "chrome/browser/chromeos/login/mock_update_screen.h"
#include "chrome/browser/chromeos/login/network_screen.h"
#include "chrome/browser/chromeos/login/reset_screen.h"
#include "chrome/browser/chromeos/login/test_login_utils.h"
#include "chrome/browser/chromeos/login/user_image_screen.h"
#include "chrome/browser/chromeos/login/view_screen.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/login/wizard_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/public/common/unicode/locid.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {
const char kUsername[] = "test_user@managedchrome.com";
const char kPassword[] = "test_password";
}  // namespace

using ::testing::_;

template <class T, class H>
class MockOutShowHide : public T {
 public:
  template <class P> explicit  MockOutShowHide(P p) : T(p) {}
  template <class P> MockOutShowHide(P p, H* actor)
      : T(p, actor), actor_(actor) {}

  H* actor() const { return actor_.get(); }

  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());

 private:
  scoped_ptr<H> actor_;
};

#define MOCK(mock_var, screen_name, mocked_class, actor_class)                 \
  mock_var = new MockOutShowHide<mocked_class, actor_class>(                   \
      WizardController::default_controller(), new actor_class);                \
  WizardController::default_controller()->screen_name.reset(mock_var);         \
  EXPECT_CALL(*mock_var, Show()).Times(0);                                     \
  EXPECT_CALL(*mock_var, Hide()).Times(0);

class WizardControllerTest : public WizardInProcessBrowserTest {
 protected:
  WizardControllerTest() : WizardInProcessBrowserTest(
      WizardController::kTestNoScreenName) {}
  virtual ~WizardControllerTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerTest, SwitchLanguage) {
  ASSERT_TRUE(WizardController::default_controller() != NULL);
  WizardController::default_controller()->AdvanceToScreen(
      WizardController::kNetworkScreenName);

  // Checking the default locale. Provided that the profile is cleared in SetUp.
  EXPECT_EQ("en-US", g_browser_process->GetApplicationLocale());
  EXPECT_STREQ("en", icu::Locale::getDefault().getLanguage());
  EXPECT_FALSE(base::i18n::IsRTL());
  const std::wstring en_str =
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_NETWORK_SELECTION_TITLE));

  LanguageSwitchMenu::SwitchLanguage("fr");
  EXPECT_EQ("fr", g_browser_process->GetApplicationLocale());
  EXPECT_STREQ("fr", icu::Locale::getDefault().getLanguage());
  EXPECT_FALSE(base::i18n::IsRTL());
  const std::wstring fr_str =
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_NETWORK_SELECTION_TITLE));

  EXPECT_NE(en_str, fr_str);

  LanguageSwitchMenu::SwitchLanguage("ar");
  EXPECT_EQ("ar", g_browser_process->GetApplicationLocale());
  EXPECT_STREQ("ar", icu::Locale::getDefault().getLanguage());
  EXPECT_TRUE(base::i18n::IsRTL());
  const std::wstring ar_str =
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_NETWORK_SELECTION_TITLE));

  EXPECT_NE(fr_str, ar_str);
}

class WizardControllerFlowTest : public WizardControllerTest {
 protected:
  WizardControllerFlowTest() {}
  // Overriden from InProcessBrowserTest:
  virtual void SetUpOnMainThread() OVERRIDE {
    WizardControllerTest::SetUpOnMainThread();

    // Make sure that OOBE is run as an "official" build.
    WizardController::default_controller()->is_official_build_ = true;

    // Clear portal list (as it is by default in OOBE).
    CrosLibrary::Get()->GetNetworkLibrary()->SetCheckPortalList("");

    // Set up the mocks for all screens.
    MOCK(mock_network_screen_, network_screen_,
         MockNetworkScreen, MockNetworkScreenActor);
    MOCK(mock_update_screen_, update_screen_,
         MockUpdateScreen, MockUpdateScreenActor);
    MOCK(mock_eula_screen_, eula_screen_, MockEulaScreen, MockEulaScreenActor);
    MOCK(mock_enterprise_enrollment_screen_, enterprise_enrollment_screen_,
         MockEnterpriseEnrollmentScreen, MockEnterpriseEnrollmentScreenActor);

    // Switch to the initial screen.
    EXPECT_EQ(NULL, WizardController::default_controller()->current_screen());
    EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
    WizardController::default_controller()->AdvanceToScreen(
        WizardController::kNetworkScreenName);
  }

  void OnExit(ScreenObserver::ExitCodes exit_code) {
    WizardController::default_controller()->OnExit(exit_code);
  }

  MockOutShowHide<MockNetworkScreen, MockNetworkScreenActor>*
      mock_network_screen_;
  MockOutShowHide<MockUpdateScreen, MockUpdateScreenActor>* mock_update_screen_;
  MockOutShowHide<MockEulaScreen, MockEulaScreenActor>* mock_eula_screen_;
  MockOutShowHide<MockEnterpriseEnrollmentScreen,
      MockEnterpriseEnrollmentScreenActor>* mock_enterprise_enrollment_screen_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerFlowTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest, ControlFlowMain) {
  EXPECT_TRUE(ExistingUserController::current_controller() == NULL);
  EXPECT_EQ(WizardController::default_controller()->GetNetworkScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  OnExit(ScreenObserver::NETWORK_CONNECTED);

  EXPECT_EQ(WizardController::default_controller()->GetEulaScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartUpdate()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(ScreenObserver::EULA_ACCEPTED);
  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  EXPECT_EQ(WizardController::default_controller()->GetUpdateScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(0);
  OnExit(ScreenObserver::UPDATE_INSTALLED);

  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
  EXPECT_EQ("ethernet,wifi,cellular",
            CrosLibrary::Get()->GetNetworkLibrary()->GetCheckPortalList());
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest, ControlFlowErrorUpdate) {
  EXPECT_EQ(WizardController::default_controller()->GetNetworkScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_update_screen_, StartUpdate()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(ScreenObserver::NETWORK_CONNECTED);

  EXPECT_EQ(WizardController::default_controller()->GetEulaScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartUpdate()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(ScreenObserver::EULA_ACCEPTED);
  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  EXPECT_EQ(WizardController::default_controller()->GetUpdateScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(0);  // last transition
  OnExit(ScreenObserver::UPDATE_ERROR_UPDATING);

  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest, ControlFlowSkipUpdateEnroll) {
  EXPECT_EQ(WizardController::default_controller()->GetNetworkScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_update_screen_, StartUpdate()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(ScreenObserver::NETWORK_CONNECTED);

  EXPECT_EQ(WizardController::default_controller()->GetEulaScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartUpdate()).Times(0);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(0);
  WizardController::default_controller()->SkipUpdateEnrollAfterEula();
  EXPECT_CALL(*mock_enterprise_enrollment_screen_->actor(),
              SetParameters(mock_enterprise_enrollment_screen_, false, ""))
      .Times(1);
  EXPECT_CALL(*mock_enterprise_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_enterprise_enrollment_screen_, Hide()).Times(0);
  OnExit(ScreenObserver::EULA_ACCEPTED);

  EXPECT_EQ(WizardController::default_controller()->
                GetEnterpriseEnrollmentScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_TRUE(ExistingUserController::current_controller() == NULL);
  EXPECT_EQ("ethernet,wifi,cellular",
            CrosLibrary::Get()->GetNetworkLibrary()->GetCheckPortalList());
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest, ControlFlowEulaDeclined) {
  EXPECT_EQ(WizardController::default_controller()->GetNetworkScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_update_screen_, StartUpdate()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(ScreenObserver::NETWORK_CONNECTED);

  EXPECT_EQ(WizardController::default_controller()->GetEulaScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(0);  // last transition
  OnExit(ScreenObserver::EULA_BACK);

  EXPECT_EQ(WizardController::default_controller()->GetNetworkScreen(),
            WizardController::default_controller()->current_screen());
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest,
                       ControlFlowEnterpriseEnrollmentCompleted) {
  EXPECT_EQ(WizardController::default_controller()->GetNetworkScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_update_screen_, StartUpdate()).Times(0);
  EXPECT_CALL(*mock_enterprise_enrollment_screen_->actor(),
              SetParameters(mock_enterprise_enrollment_screen_, false, ""))
      .Times(1);
  EXPECT_CALL(*mock_enterprise_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);

  WizardController::default_controller()->AdvanceToScreen(
      WizardController::kEnterpriseEnrollmentScreenName);
  EnterpriseEnrollmentScreen* screen =
      WizardController::default_controller()->GetEnterpriseEnrollmentScreen();
  EXPECT_EQ(screen, WizardController::default_controller()->current_screen());
  OnExit(ScreenObserver::ENTERPRISE_ENROLLMENT_COMPLETED);

  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest,
                       ControlFlowEnterpriseAutoEnrollmentCompleted) {
  WizardController::default_controller()->SkipImageSelectionForTesting();
  EXPECT_EQ(WizardController::default_controller()->GetNetworkScreen(),
            WizardController::default_controller()->current_screen());
  EXPECT_CALL(*mock_update_screen_, StartUpdate()).Times(0);

  LoginUtils::Set(new TestLoginUtils(kUsername, kPassword));
  MockConsumer mock_consumer;

  // Must have a pending signin to resume after auto-enrollment:
  BaseLoginDisplayHost::default_host()->StartSignInScreen();
  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
  ExistingUserController::current_controller()->DoAutoEnrollment();
  ExistingUserController::current_controller()->set_login_status_consumer(
      &mock_consumer);
  // This calls StartWizard, destroying the current controller() and its mocks;
  // don't set expectations on those objects.
  ExistingUserController::current_controller()->CompleteLogin(kUsername,
                                                              kPassword);
  // Run the tasks posted to complete the login:
  MessageLoop::current()->RunUntilIdle();

  EnterpriseEnrollmentScreen* screen =
      WizardController::default_controller()->GetEnterpriseEnrollmentScreen();
  EXPECT_EQ(screen, WizardController::default_controller()->current_screen());
  // This is the main expectation: after auto-enrollment, login is resumed.
  EXPECT_CALL(mock_consumer, OnLoginSuccess(_, _, _, _)).Times(1);
  OnExit(ScreenObserver::ENTERPRISE_AUTO_MAGIC_ENROLLMENT_COMPLETED);
  // Prevent browser launch when the profile is prepared:
  browser_shutdown::SetTryingToQuit(true);
  // Run the tasks posted to complete the login:
  MessageLoop::current()->RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest, ControlFlowResetScreen) {
  EXPECT_EQ(WizardController::default_controller()->GetNetworkScreen(),
            WizardController::default_controller()->current_screen());

  BaseLoginDisplayHost::default_host()->StartSignInScreen();
  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
  ExistingUserController::current_controller()->OnStartDeviceReset();

  ResetScreen* screen =
      WizardController::default_controller()->GetResetScreen();
  EXPECT_EQ(screen, WizardController::default_controller()->current_screen());

  // After reset screen is canceled, it returns to sign-in screen.
  // And this destroys WizardController.
  OnExit(ScreenObserver::RESET_CANCELED);
  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
}

// TODO(nkostylev): Add test for WebUI accelerators http://crosbug.com/22571

COMPILE_ASSERT(ScreenObserver::EXIT_CODES_COUNT == 14,
               add_tests_for_new_control_flow_you_just_introduced);

}  // namespace chromeos
