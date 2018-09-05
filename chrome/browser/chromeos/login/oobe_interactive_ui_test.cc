// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/screens/gaia_view.h"
#include "chrome/browser/chromeos/login/screens/sync_consent_screen.h"
#include "chrome/browser/chromeos/login/screens/update_screen.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chromeos/dbus/update_engine_client.h"
#include "content/public/browser/notification_service.h"

namespace chromeos {
namespace {
constexpr base::TimeDelta kJsConditionCheckFrequency =
    base::TimeDelta::FromMilliseconds(2000);
}  // namespace

// Waits for js condition to be fulfilled.
class JsConditionWaiter {
 public:
  JsConditionWaiter(const test::JSChecker& js_checker,
                    const std::string& js_condition)
      : js_checker_(js_checker), js_condition_(js_condition) {}

  ~JsConditionWaiter() = default;

  void Wait() {
    if (IsConditionFulfilled())
      return;

    timer_.Start(FROM_HERE, kJsConditionCheckFrequency, this,
                 &JsConditionWaiter::CheckCondition);
    run_loop_.Run();
  }

 private:
  bool IsConditionFulfilled() { return js_checker_.GetBool(js_condition_); }

  void CheckCondition() {
    if (IsConditionFulfilled()) {
      run_loop_.Quit();
      timer_.Stop();
    }
  }

  test::JSChecker js_checker_;
  const std::string js_condition_;

  base::RepeatingTimer timer_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(JsConditionWaiter);
};

class OobeInteractiveUITest : public OobeBaseTest {
 public:
  OobeInteractiveUITest() = default;
  ~OobeInteractiveUITest() override = default;

  void TearDownOnMainThread() override {
    // If the login display is still showing, exit gracefully.
    if (LoginDisplayHost::default_host()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&chrome::AttemptExit));
      RunUntilBrowserProcessQuits();
    }

    OobeBaseTest::TearDownOnMainThread();
  }

  void WaitForLoginDisplayHostShutdown() {
    if (!LoginDisplayHost::default_host())
      return;

    base::RunLoop runloop;
    LOG(INFO)
        << "OobeInteractiveUITest: Waiting for LoginDisplayHost to shut down.";
    while (LoginDisplayHost::default_host()) {
      runloop.RunUntilIdle();
    }
    LOG(INFO) << "OobeInteractiveUITest: LoginDisplayHost is down.";
  }

  void WaitForOobeWelcomeScreen() {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
        content::NotificationService::AllSources());
    observer.Wait();

    JsConditionWaiter(js_checker_,
                      "Oobe.getInstance().currentScreen.id == 'connect'")
        .Wait();
  }

  void RunWelcomeScreenChecks() {
#if defined(GOOGLE_CHROME_BUILD)
    constexpr int kNumberOfVideosPlaying = 1;
#else
    constexpr int kNumberOfVideosPlaying = 0;
#endif

    js_checker_.ExpectFalse("$('oobe-welcome-md').$.welcomeScreen.hidden");
    js_checker_.ExpectTrue("$('oobe-welcome-md').$.accessibilityScreen.hidden");
    js_checker_.ExpectTrue("$('oobe-welcome-md').$.languageScreen.hidden");
    js_checker_.ExpectTrue("$('oobe-welcome-md').$.timezoneScreen.hidden");

    js_checker_.ExpectEQ(
        "(() => {let cnt = 0; for (let v of "
        "$('oobe-welcome-md').$.welcomeScreen.root.querySelectorAll('video')) "
        "{  cnt += v.paused ? 0 : 1; }; return cnt; })()",
        kNumberOfVideosPlaying);
  }

  void TapWelcomeNext() {
    js_checker_.Evaluate(
        "$('oobe-welcome-md').$.welcomeScreen.$.welcomeNextButton.click()");
  }

  void WaitForNetworkSelectionScreen() {
    JsConditionWaiter(
        js_checker_,
        "Oobe.getInstance().currentScreen.id == 'network-selection'")
        .Wait();
    LOG(INFO)
        << "OobeInteractiveUITest: Switched to 'network-selection' screen.";
  }

  void RunNetworkSelectionScreenChecks() {
    js_checker_.ExpectTrue(
        "!$('oobe-network-md').$.networkDialog.querySelector('oobe-next-button'"
        ").disabled");
  }

  void TapNetworkSelectionNext() {
    js_checker_.Evaluate(
        "$('oobe-network-md').$.networkDialog.querySelector('oobe-next-button')"
        ".click()");
  }

  void WaitForEulaScreen() {
    JsConditionWaiter(js_checker_,
                      "Oobe.getInstance().currentScreen.id == 'eula'")
        .Wait();
    LOG(INFO) << "OobeInteractiveUITest: Switched to 'eula' screen.";
  }

  void RunEulaScreenChecks() {
    // Wait for actual EULA to appear.
    JsConditionWaiter(js_checker_, "!$('oobe-eula-md').$.eulaDialog.hidden")
        .Wait();
    js_checker_.ExpectTrue("!$('oobe-eula-md').$.acceptButton.disabled");
  }

  void TapEulaAccept() {
    js_checker_.Evaluate("$('oobe-eula-md').$.acceptButton.click();");
  }

  void WaitForUpdateScreen() {
    JsConditionWaiter(js_checker_,
                      "Oobe.getInstance().currentScreen.id == 'update'")
        .Wait();

    LOG(INFO) << "OobeInteractiveUITest: Switched to 'update' screen.";
  }

  void ExitUpdateScreenNoUpdate() {
    UpdateScreen* screen = static_cast<UpdateScreen*>(
        WizardController::default_controller()->GetScreen(
            OobeScreen::SCREEN_OOBE_UPDATE));
    UpdateEngineClient::Status status;
    status.status = UpdateEngineClient::UPDATE_STATUS_ERROR;
    screen->UpdateStatusChanged(status);
  }

  void WaitForGaiaSignInScreen() {
    JsConditionWaiter(js_checker_,
                      "Oobe.getInstance().currentScreen.id == 'gaia-signin'")
        .Wait();
    LOG(INFO) << "OobeInteractiveUITest: Switched to 'gaia-signin' screen.";
  }

  void LogInAsRegularUser() {
    LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetGaiaScreenView()
        ->ShowSigninScreenForTest(OobeBaseTest::kFakeUserEmail,
                                  OobeBaseTest::kFakeUserPassword,
                                  OobeBaseTest::kEmptyUserServices);
  }

  void WaitForSyncConsentScreen() {
    JsConditionWaiter(js_checker_,
                      "Oobe.getInstance().currentScreen.id == 'sync-consent'")
        .Wait();

    LOG(INFO) << "OobeInteractiveUITest: Logged in. Switched to 'sync-consent' "
                 "screen.";
  }

  void ExitScreenSyncConsent() {
    SyncConsentScreen* screen = static_cast<SyncConsentScreen*>(
        WizardController::default_controller()->GetScreen(
            OobeScreen::SCREEN_SYNC_CONSENT));

    screen->SetProfileSyncDisabledByPolicyForTesting(true);
    screen->OnStateChanged(nullptr);
  }

  void WaitForMarketingOptInScreen() {
    JsConditionWaiter(
        js_checker_,
        "Oobe.getInstance().currentScreen.id == 'marketing-opt-in'")
        .Wait();
    LOG(INFO)
        << "OobeInteractiveUITest: Switched to 'marketing-opt-in' screen.";
  }

  void RunMarketingOptInScreenChecks() {
    js_checker_.ExpectTrue("!$('marketing-opt-in').hidden");
    js_checker_.ExpectEQ(
        "$('marketing-opt-in-impl').root.querySelectorAll('oobe-text-button')."
        "length",
        1);
  }

  void ExitMarketingOptInScreen() {
    js_checker_.Evaluate(
        "$('marketing-opt-in-impl').root.querySelectorAll('oobe-text-button')["
        "0].click()");
    JsConditionWaiter(js_checker_,
                      "Oobe.getInstance().currentScreen.id == 'user-image'")
        .Wait();

    LOG(INFO) << "OobeInteractiveUITest: Switched to 'user-image' screen.";
  }

  void RunUserImageScreenChecks() {
    js_checker_.ExpectTrue("!$('user-image').hidden");
  }

  void ExitUserImageScreen() {
    js_checker_.Evaluate("$('user-image').querySelector('#ok-button').click()");

    LOG(INFO) << "OobeInteractiveUITest: Exited 'user-image' screen.";
    WaitForLoginDisplayHostShutdown();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OobeInteractiveUITest);
};

IN_PROC_BROWSER_TEST_F(OobeInteractiveUITest, SimpleEndToEnd) {
  WaitForOobeWelcomeScreen();
  RunWelcomeScreenChecks();
  TapWelcomeNext();

  WaitForNetworkSelectionScreen();
  RunNetworkSelectionScreenChecks();
  TapNetworkSelectionNext();

#if defined(GOOGLE_CHROME_BUILD)
  WaitForEulaScreen();
  RunEulaScreenChecks();
  TapEulaAccept();
#endif

  WaitForUpdateScreen();
  ExitUpdateScreenNoUpdate();
  WaitForGaiaSignInScreen();

  LogInAsRegularUser();

#if defined(GOOGLE_CHROME_BUILD)
  WaitForSyncConsentScreen();
  ExitScreenSyncConsent();
#endif

  WaitForMarketingOptInScreen();
  RunMarketingOptInScreenChecks();
  ExitMarketingOptInScreen();

  RunUserImageScreenChecks();
  ExitUserImageScreen();

  WaitForLoginDisplayHostShutdown();
}

}  //  namespace chromeos
