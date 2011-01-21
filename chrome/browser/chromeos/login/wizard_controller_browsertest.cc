// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/account_screen.h"
#include "chrome/browser/chromeos/login/eula_view.h"
#include "chrome/browser/chromeos/login/language_switch_menu.h"
#include "chrome/browser/chromeos/login/login_screen.h"
#include "chrome/browser/chromeos/login/mock_update_screen.h"
#include "chrome/browser/chromeos/login/network_screen.h"
#include "chrome/browser/chromeos/login/network_selection_view.h"
#include "chrome/browser/chromeos/login/user_image_screen.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/login/wizard_in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "unicode/locid.h"
#include "views/accelerator.h"

namespace {

template <class T>
class MockOutShowHide : public T {
 public:
  template <class P> MockOutShowHide(P p) : T(p) {}
  template <class P1, class P2> MockOutShowHide(P1 p1, P2 p2) : T(p1, p2) {}
  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());
};

template <class T>
struct CreateMockWizardScreenHelper {
  static MockOutShowHide<T>* Create(WizardController* wizard);
};

template <class T> MockOutShowHide<T>*
CreateMockWizardScreenHelper<T>::Create(WizardController* wizard) {
  return new MockOutShowHide<T>(wizard);
}

template <> MockOutShowHide<chromeos::NetworkScreen>*
CreateMockWizardScreenHelper<chromeos::NetworkScreen>::Create(
    WizardController* wizard) {
  return new MockOutShowHide<chromeos::NetworkScreen>(wizard);
}

#define MOCK(mock_var, screen_name, mocked_class)                              \
  mock_var = CreateMockWizardScreenHelper<mocked_class>::Create(controller()); \
  controller()->screen_name.reset(mock_var);                                   \
  EXPECT_CALL(*mock_var, Show()).Times(0);                                     \
  EXPECT_CALL(*mock_var, Hide()).Times(0);

}  // namespace

class WizardControllerTest : public chromeos::WizardInProcessBrowserTest {
 protected:
  WizardControllerTest() : chromeos::WizardInProcessBrowserTest(
      WizardController::kTestNoScreenName) {}
  virtual ~WizardControllerTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerTest);
};

// TODO(zelidrag): Need to revisit this once translation for fr and ar is
// complete.  See http://crosbug.com/8974
IN_PROC_BROWSER_TEST_F(WizardControllerTest, FAILS_SwitchLanguage) {
  WizardController* const wizard = controller();
  ASSERT_TRUE(wizard != NULL);
  wizard->ShowFirstScreen(WizardController::kNetworkScreenName);
  views::View* current_screen = wizard->contents();
  ASSERT_TRUE(current_screen != NULL);

  // Checking the default locale. Provided that the profile is cleared in SetUp.
  EXPECT_EQ("en-US", g_browser_process->GetApplicationLocale());
  EXPECT_STREQ("en", icu::Locale::getDefault().getLanguage());
  EXPECT_FALSE(base::i18n::IsRTL());
  const std::wstring en_str =
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_NETWORK_SELECTION_TITLE));

  chromeos::LanguageSwitchMenu::SwitchLanguage("fr");
  EXPECT_EQ("fr", g_browser_process->GetApplicationLocale());
  EXPECT_STREQ("fr", icu::Locale::getDefault().getLanguage());
  EXPECT_FALSE(base::i18n::IsRTL());
  const std::wstring fr_str =
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_NETWORK_SELECTION_TITLE));

  EXPECT_NE(en_str, fr_str);

  chromeos::LanguageSwitchMenu::SwitchLanguage("ar");
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
  virtual Browser* CreateBrowser(Profile* profile) {
    Browser* ret = WizardControllerTest::CreateBrowser(profile);

    // Make sure that OOBE is run as an "official" build.
    WizardController::default_controller()->is_official_build_ = true;

    // Set up the mocks for all screens.
    MOCK(mock_account_screen_, account_screen_, chromeos::AccountScreen);
    MOCK(mock_login_screen_, login_screen_, chromeos::LoginScreen);
    MOCK(mock_network_screen_, network_screen_, chromeos::NetworkScreen);
    MOCK(mock_update_screen_, update_screen_, MockUpdateScreen);
    MOCK(mock_eula_screen_, eula_screen_, chromeos::EulaScreen);

    // Switch to the initial screen.
    EXPECT_EQ(NULL, controller()->current_screen());
    EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
    controller()->ShowFirstScreen(WizardController::kNetworkScreenName);

    return ret;
  }

  MockOutShowHide<chromeos::AccountScreen>* mock_account_screen_;
  MockOutShowHide<chromeos::LoginScreen>* mock_login_screen_;
  MockOutShowHide<chromeos::NetworkScreen>* mock_network_screen_;
  MockOutShowHide<MockUpdateScreen>* mock_update_screen_;
  MockOutShowHide<chromeos::EulaScreen>* mock_eula_screen_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerFlowTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest, ControlFlowMain) {
  EXPECT_EQ(controller()->GetNetworkScreen(), controller()->current_screen());
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  controller()->OnExit(chromeos::ScreenObserver::NETWORK_CONNECTED);

  EXPECT_EQ(controller()->GetEulaScreen(), controller()->current_screen());
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartUpdate()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  controller()->OnExit(chromeos::ScreenObserver::EULA_ACCEPTED);

  EXPECT_EQ(controller()->GetUpdateScreen(), controller()->current_screen());
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_login_screen_, Show()).Times(1);
  controller()->OnExit(chromeos::ScreenObserver::UPDATE_INSTALLED);

  EXPECT_EQ(controller()->GetLoginScreen(), controller()->current_screen());
  EXPECT_CALL(*mock_login_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_account_screen_, Show()).Times(1);
  controller()->OnExit(chromeos::ScreenObserver::LOGIN_CREATE_ACCOUNT);

  EXPECT_EQ(controller()->GetAccountScreen(), controller()->current_screen());
  EXPECT_CALL(*mock_account_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_login_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_login_screen_, Hide()).Times(0);  // last transition
  controller()->OnExit(chromeos::ScreenObserver::ACCOUNT_CREATED);

  EXPECT_EQ(controller()->GetLoginScreen(), controller()->current_screen());
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest, ControlFlowErrorUpdate) {
  EXPECT_EQ(controller()->GetNetworkScreen(), controller()->current_screen());
  EXPECT_CALL(*mock_update_screen_, StartUpdate()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  controller()->OnExit(chromeos::ScreenObserver::NETWORK_CONNECTED);

  EXPECT_EQ(controller()->GetEulaScreen(), controller()->current_screen());
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartUpdate()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  controller()->OnExit(chromeos::ScreenObserver::EULA_ACCEPTED);

  EXPECT_EQ(controller()->GetUpdateScreen(), controller()->current_screen());
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(0);  // last transition
  EXPECT_CALL(*mock_login_screen_, Show()).Times(1);
  controller()->OnExit(
      chromeos::ScreenObserver::UPDATE_ERROR_UPDATING);

  EXPECT_EQ(controller()->GetLoginScreen(), controller()->current_screen());
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest, ControlFlowEulaDeclined) {
  EXPECT_EQ(controller()->GetNetworkScreen(), controller()->current_screen());
  EXPECT_CALL(*mock_update_screen_, StartUpdate()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  controller()->OnExit(chromeos::ScreenObserver::NETWORK_CONNECTED);

  EXPECT_EQ(controller()->GetEulaScreen(), controller()->current_screen());
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(0);  // last transition
  controller()->OnExit(chromeos::ScreenObserver::EULA_BACK);

  EXPECT_EQ(controller()->GetNetworkScreen(), controller()->current_screen());
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest, ControlFlowErrorNetwork) {
  EXPECT_EQ(controller()->GetNetworkScreen(), controller()->current_screen());
  EXPECT_CALL(*mock_login_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  controller()->OnExit(chromeos::ScreenObserver::NETWORK_OFFLINE);
  EXPECT_EQ(controller()->GetLoginScreen(), controller()->current_screen());
}

#if defined(OFFICIAL_BUILD)
// This test is supposed to fail on official test.
#define MAYBE_Accelerators DISABLED_Accelerators
#else
#define MAYBE_Accelerators Accelerators
#endif

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest, MAYBE_Accelerators) {
  EXPECT_EQ(controller()->GetNetworkScreen(), controller()->current_screen());

  views::FocusManager* focus_manager =
      controller()->contents()->GetFocusManager();
  views::Accelerator accel_account_screen(ui::VKEY_A, false, true, true);
  views::Accelerator accel_login_screen(ui::VKEY_L, false, true, true);
  views::Accelerator accel_network_screen(ui::VKEY_N, false, true, true);
  views::Accelerator accel_update_screen(ui::VKEY_U, false, true, true);
  views::Accelerator accel_image_screen(ui::VKEY_I, false, true, true);
  views::Accelerator accel_eula_screen(ui::VKEY_E, false, true, true);

  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_account_screen_, Show()).Times(1);
  EXPECT_TRUE(focus_manager->ProcessAccelerator(accel_account_screen));
  EXPECT_EQ(controller()->GetAccountScreen(), controller()->current_screen());

  focus_manager = controller()->contents()->GetFocusManager();
  EXPECT_CALL(*mock_account_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_login_screen_, Show()).Times(1);
  EXPECT_TRUE(focus_manager->ProcessAccelerator(accel_login_screen));
  EXPECT_EQ(controller()->GetLoginScreen(), controller()->current_screen());

  focus_manager = controller()->contents()->GetFocusManager();
  EXPECT_CALL(*mock_login_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  EXPECT_TRUE(focus_manager->ProcessAccelerator(accel_network_screen));
  EXPECT_EQ(controller()->GetNetworkScreen(), controller()->current_screen());

  focus_manager = controller()->contents()->GetFocusManager();
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  EXPECT_TRUE(focus_manager->ProcessAccelerator(accel_update_screen));
  EXPECT_EQ(controller()->GetUpdateScreen(), controller()->current_screen());

  focus_manager = controller()->contents()->GetFocusManager();
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_TRUE(focus_manager->ProcessAccelerator(accel_image_screen));
  EXPECT_EQ(controller()->GetUserImageScreen(), controller()->current_screen());

  focus_manager = controller()->contents()->GetFocusManager();
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_TRUE(focus_manager->ProcessAccelerator(accel_eula_screen));
  EXPECT_EQ(controller()->GetEulaScreen(), controller()->current_screen());
}

COMPILE_ASSERT(chromeos::ScreenObserver::EXIT_CODES_COUNT == 18,
               add_tests_for_new_control_flow_you_just_introduced);
