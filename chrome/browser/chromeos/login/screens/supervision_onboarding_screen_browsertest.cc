// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/supervision_onboarding_screen.h"

#include <initializer_list>
#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/mixin_based_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/supervision_onboarding_screen_handler.h"
#include "chromeos/constants/chromeos_features.h"

namespace chromeos {

namespace {

constexpr char kTestUser[] = "test-user1@gmail.com";

chromeos::OobeUI* GetOobeUI() {
  auto* host = chromeos::LoginDisplayHost::default_host();
  return host ? host->GetOobeUI() : nullptr;
}

}  // namespace

class SupervisionOnboardingTest : public MixinBasedInProcessBrowserTest {
 public:
  SupervisionOnboardingTest() = default;
  ~SupervisionOnboardingTest() override = default;

  void SetUpOnMainThread() override {
    login_manager_.LoginAndWaitForActiveSession(
        LoginManagerMixin::CreateDefaultUserContext(test_user_));

    ShowLoginWizard(OobeScreen::SCREEN_TEST_NO_WINDOW);
    WizardController::default_controller()
        ->screen_manager()
        ->DeleteScreenForTesting(SupervisionOnboardingScreenView::kScreenId);
    auto supervision_onboarding_screen =
        std::make_unique<SupervisionOnboardingScreen>(
            GetOobeUI()->GetView<SupervisionOnboardingScreenHandler>(),
            base::BindRepeating(&SupervisionOnboardingTest::HandleScreenExit,
                                base::Unretained(this)));
    supervision_onboarding_screen_ = supervision_onboarding_screen.get();
    WizardController::default_controller()
        ->screen_manager()
        ->SetScreenForTesting(std::move(supervision_onboarding_screen));

    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

  void TurnOnFeature() {
    feature_list_.InitAndEnableFeature(
        features::kEnableSupervisionOnboardingScreens);
  }

  void ShowAndWaitForScreen() {
    supervision_onboarding_screen_->Show();

    OobeScreenWaiter screen_waiter(SupervisionOnboardingScreenView::kScreenId);
    screen_waiter.set_assert_next_screen();
    screen_waiter.Wait();
  }

  void ClickButton(const std::string& button_id) {
    std::initializer_list<base::StringPiece> button_path = {
        "supervision-onboarding", button_id};
    test::OobeJS().CreateEnabledWaiter(true, button_path)->Wait();
    test::OobeJS().TapOnPath(button_path);
  }

  void WaitForScreenExit() {
    if (screen_exited_)
      return;

    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  SupervisionOnboardingScreen* supervision_onboarding_screen_;
  base::test::ScopedFeatureList feature_list_;

 private:
  void HandleScreenExit() {
    ASSERT_FALSE(screen_exited_);
    screen_exited_ = true;
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  bool screen_exited_ = false;
  base::OnceClosure screen_exit_callback_;

  const LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId(kTestUser, kTestUser)};
  LoginManagerMixin login_manager_{&mixin_host_, {test_user_}};
};

IN_PROC_BROWSER_TEST_F(SupervisionOnboardingTest,
                       ExitImmediatelyWhenFeatureIsOff) {
  supervision_onboarding_screen_->Show();
  WaitForScreenExit();
}

IN_PROC_BROWSER_TEST_F(SupervisionOnboardingTest, NextButtonExitsScreen) {
  TurnOnFeature();
  ShowAndWaitForScreen();

  ClickButton("next-button");
  WaitForScreenExit();
}

IN_PROC_BROWSER_TEST_F(SupervisionOnboardingTest, BackButtonExitsScreen) {
  TurnOnFeature();
  ShowAndWaitForScreen();

  ClickButton("back-button");
  WaitForScreenExit();
}

IN_PROC_BROWSER_TEST_F(SupervisionOnboardingTest, SkipButtonExitsScreen) {
  TurnOnFeature();
  ShowAndWaitForScreen();

  ClickButton("skip-button");
  WaitForScreenExit();
}

}  // namespace chromeos
