// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_switches.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/extensions/quick_unlock_private/quick_unlock_private_api.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/chromeos/login/screens/gaia_view.h"
#include "chrome/browser/chromeos/login/screens/sync_consent_screen.h"
#include "chrome/browser/chromeos/login/screens/update_screen.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/update_engine_client.h"
#include "content/public/browser/notification_service.h"

namespace chromeos {
namespace {
constexpr base::TimeDelta kJsConditionCheckFrequency =
    base::TimeDelta::FromMilliseconds(200);
constexpr base::TimeDelta kLoginDisplayHostCheckFrequency =
    base::TimeDelta::FromMilliseconds(200);
}  // namespace

// Waits for js condition to be fulfilled.
class JsConditionWaiter {
 public:
  enum class Options {
    kNone,
    kSatisifyIfOobeDestroyed,
  };

  // If |options| is true, we are waiting for the end condition, so it is
  // automatically fullfilled if LoginDisplayHost is already destroyed.
  JsConditionWaiter(const test::JSChecker& js_checker,
                    const std::string& js_condition,
                    Options options)
      : js_checker_(js_checker),
        js_condition_(js_condition),
        options_(options) {}

  ~JsConditionWaiter() = default;

  void Wait() {
    if (IsConditionFulfilled())
      return;

    timer_.Start(FROM_HERE, kJsConditionCheckFrequency, this,
                 &JsConditionWaiter::CheckCondition);
    run_loop_.Run();
  }

 private:
  bool IsConditionFulfilled() {
    return (options_ == Options::kSatisifyIfOobeDestroyed &&
            !LoginDisplayHost::default_host()) ||
           js_checker_.GetBool(js_condition_);
  }

  void CheckCondition() {
    if (IsConditionFulfilled()) {
      run_loop_.Quit();
      timer_.Stop();
    }
  }

  test::JSChecker js_checker_;
  const std::string js_condition_;
  const Options options_;

  base::RepeatingTimer timer_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(JsConditionWaiter);
};

// Waits for LoginDisplayHost to shut down.
class LoginDisplayHostShutdownWaiter {
 public:
  LoginDisplayHostShutdownWaiter() = default;
  ~LoginDisplayHostShutdownWaiter() = default;

  void Wait() {
    if (!LoginDisplayHost::default_host())
      return;

    timer_.Start(FROM_HERE, kLoginDisplayHostCheckFrequency, this,
                 &LoginDisplayHostShutdownWaiter::CheckCondition);
    run_loop_.Run();
  }

 private:
  void CheckCondition() {
    if (!LoginDisplayHost::default_host()) {
      run_loop_.Quit();
      timer_.Stop();
    }
  }

  base::RepeatingTimer timer_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(LoginDisplayHostShutdownWaiter);
};

class ScopedQuickUnlockPrivateGetAuthTokenFunctionObserver {
 public:
  explicit ScopedQuickUnlockPrivateGetAuthTokenFunctionObserver(
      extensions::QuickUnlockPrivateGetAuthTokenFunction::TestObserver*
          observer) {
    extensions::QuickUnlockPrivateGetAuthTokenFunction::SetTestObserver(
        observer);
  }
  ~ScopedQuickUnlockPrivateGetAuthTokenFunctionObserver() {
    extensions::QuickUnlockPrivateGetAuthTokenFunction::SetTestObserver(
        nullptr);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      ScopedQuickUnlockPrivateGetAuthTokenFunctionObserver);
};

class OobeInteractiveUITest
    : public OobeBaseTest,
      public extensions::QuickUnlockPrivateGetAuthTokenFunction::TestObserver,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  struct Parameters {
    bool is_tablet;
    bool is_quick_unlock_enabled;

    std::string ToString() const {
      return std::string("{is_tablet: ") + (is_tablet ? "true" : "false") +
             ", is_quick_unlock_enabled: " +
             (is_quick_unlock_enabled ? "true" : "false") + "}";
    }
  };

  OobeInteractiveUITest() = default;
  ~OobeInteractiveUITest() override = default;

  void SetUp() override {
    params_ = Parameters();
    std::tie(params_->is_tablet, params_->is_quick_unlock_enabled) = GetParam();
    LOG(INFO) << "OobeInteractiveUITest() started with params "
              << params_->ToString();
    OobeBaseTest::SetUp();
  }

  void TearDown() override {
    OobeBaseTest::TearDown();
    params_.reset();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);

    if (params_->is_tablet)
      command_line->AppendSwitch(ash::switches::kAshEnableTabletMode);
  }

  void SetUpInProcessBrowserTestFixture() override {
    OobeBaseTest::SetUpInProcessBrowserTestFixture();

    if (params_->is_quick_unlock_enabled)
      quick_unlock::EnableForTesting();
  }

  void TearDownOnMainThread() override {
    // If the login display is still showing, exit gracefully.
    if (LoginDisplayHost::default_host()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&chrome::AttemptExit));
      RunUntilBrowserProcessQuits();
    }

    OobeBaseTest::TearDownOnMainThread();
  }

  // QuickUnlockPrivateGetAuthTokenFunction::TestObserver:
  void OnGetAuthTokenCalled(const std::string& password) override {
    quick_unlock_private_get_auth_token_password_ = password;
  }

  void WaitForLoginDisplayHostShutdown() {
    if (!LoginDisplayHost::default_host())
      return;

    LOG(INFO)
        << "OobeInteractiveUITest: Waiting for LoginDisplayHost to shut down.";
    LoginDisplayHostShutdownWaiter().Wait();
    LOG(INFO) << "OobeInteractiveUITest: LoginDisplayHost is down.";
  }

  void WaitForOobeWelcomeScreen() {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
        content::NotificationService::AllSources());
    observer.Wait();

    JsConditionWaiter(js_checker_,
                      "Oobe.getInstance().currentScreen.id == 'connect'",
                      JsConditionWaiter::Options::kNone)
        .Wait();
  }

  void RunWelcomeScreenChecks() {
#if defined(GOOGLE_CHROME_BUILD)
    constexpr int kNumberOfVideosPlaying = 1;
#else
    constexpr int kNumberOfVideosPlaying = 0;
#endif

    js_checker_.ExpectTrue("!$('oobe-welcome-md').$.welcomeScreen.hidden");
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
    js_checker_.ExecuteAsync(
        "$('oobe-welcome-md').$.welcomeScreen.$.welcomeNextButton.click()");
  }

  void WaitForNetworkSelectionScreen() {
    JsConditionWaiter(
        js_checker_,
        "Oobe.getInstance().currentScreen.id == 'network-selection'",
        JsConditionWaiter::Options::kNone)
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
    js_checker_.ExecuteAsync(
        "$('oobe-network-md').$.networkDialog.querySelector('oobe-next-button')"
        ".click()");
  }

  void WaitForEulaScreen() {
    JsConditionWaiter(js_checker_,
                      "Oobe.getInstance().currentScreen.id == 'eula'",
                      JsConditionWaiter::Options::kNone)
        .Wait();
    LOG(INFO) << "OobeInteractiveUITest: Switched to 'eula' screen.";
  }

  void RunEulaScreenChecks() {
    // Wait for actual EULA to appear.
    JsConditionWaiter(js_checker_, "!$('oobe-eula-md').$.eulaDialog.hidden",
                      JsConditionWaiter::Options::kNone)
        .Wait();
    js_checker_.ExpectTrue("!$('oobe-eula-md').$.acceptButton.disabled");
  }

  void TapEulaAccept() {
    js_checker_.ExecuteAsync("$('oobe-eula-md').$.acceptButton.click();");
  }

  void WaitForUpdateScreen() {
    JsConditionWaiter(js_checker_,
                      "Oobe.getInstance().currentScreen.id == 'update'",
                      JsConditionWaiter::Options::kNone)
        .Wait();
    JsConditionWaiter(js_checker_, "!$('update').hidden",
                      JsConditionWaiter::Options::kNone)
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
                      "Oobe.getInstance().currentScreen.id == 'gaia-signin'",
                      JsConditionWaiter::Options::kNone)
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
    LOG(INFO) << "OobeInteractiveUITest: Logged in.";
  }

  void WaitForSyncConsentScreen() {
    LOG(INFO) << "OobeInteractiveUITest: Waiting for 'sync-consent' screen.";
    JsConditionWaiter(js_checker_,
                      "Oobe.getInstance().currentScreen.id == 'sync-consent'",
                      JsConditionWaiter::Options::kNone)
        .Wait();
  }

  void ExitScreenSyncConsent() {
    SyncConsentScreen* screen = static_cast<SyncConsentScreen*>(
        WizardController::default_controller()->GetScreen(
            OobeScreen::SCREEN_SYNC_CONSENT));

    screen->SetProfileSyncDisabledByPolicyForTesting(true);
    screen->OnStateChanged(nullptr);
    LOG(INFO) << "OobeInteractiveUITest: Waiting for 'sync-consent' screen "
                 "to close.";
    JsConditionWaiter(js_checker_,
                      "Oobe.getInstance().currentScreen.id != 'sync-consent'",
                      JsConditionWaiter::Options::kSatisifyIfOobeDestroyed)
        .Wait();
  }

  void WaitForFingerprintScreen() {
    LOG(INFO)
        << "OobeInteractiveUITest: Waiting for 'fingerprint-setup' screen.";
    JsConditionWaiter(
        js_checker_,
        "Oobe.getInstance().currentScreen.id == 'fingerprint-setup'",
        JsConditionWaiter::Options::kNone)
        .Wait();
    LOG(INFO) << "OobeInteractiveUITest: Waiting for fingerprint setup screen "
                 "to show.";
    JsConditionWaiter(js_checker_, "!$('fingerprint-setup').hidden",
                      JsConditionWaiter::Options::kNone)
        .Wait();
    LOG(INFO) << "OobeInteractiveUITest: Waiting for fingerprint setup screen "
                 "to initializes.";
    JsConditionWaiter(js_checker_, "!$('fingerprint-setup-impl').hidden",
                      JsConditionWaiter::Options::kNone)
        .Wait();
    LOG(INFO) << "OobeInteractiveUITest: Waiting for fingerprint setup screen "
                 "to show setupFingerprint.";
    JsConditionWaiter(js_checker_,
                      "!$('fingerprint-setup-impl').$.setupFingerprint.hidden",
                      JsConditionWaiter::Options::kNone)
        .Wait();
  }

  void RunFingerprintScreenChecks() {
    js_checker_.ExpectTrue("!$('fingerprint-setup').hidden");
    js_checker_.ExpectTrue("!$('fingerprint-setup-impl').hidden");
    js_checker_.ExpectTrue(
        "!$('fingerprint-setup-impl').$.setupFingerprint.hidden");
    js_checker_.ExecuteAsync(
        "$('fingerprint-setup-impl').$.showSensorLocationButton.click()");
    js_checker_.ExpectTrue(
        "$('fingerprint-setup-impl').$.setupFingerprint.hidden");
    LOG(INFO) << "OobeInteractiveUITest: Waiting for fingerprint setup "
                 "to switch to placeFinger.";
    JsConditionWaiter(js_checker_,
                      "!$('fingerprint-setup-impl').$.placeFinger.hidden",
                      JsConditionWaiter::Options::kNone)
        .Wait();
  }

  void ExitFingerprintPinSetupScreen() {
    js_checker_.ExpectTrue("!$('fingerprint-setup-impl').$.placeFinger.hidden");
    // This might be the last step in flow. Synchronious execute gets stuck as
    // WebContents may be destroyed in the process. So it may never return.
    // So we use ExecuteAsync() here.
    js_checker_.ExecuteAsync(
        "$('fingerprint-setup-impl').$.setupFingerprintLater.click()");
    LOG(INFO) << "OobeInteractiveUITest: Waiting for fingerprint setup screen "
                 "to close.";
    JsConditionWaiter(js_checker_,
                      "Oobe.getInstance().currentScreen.id !="
                      "'fingerprint-setup'",
                      JsConditionWaiter::Options::kSatisifyIfOobeDestroyed)
        .Wait();
    LOG(INFO) << "OobeInteractiveUITest: 'fingerprint-setup' screen done.";
  }

  void WaitForDiscoverScreen() {
    JsConditionWaiter(js_checker_,
                      "Oobe.getInstance().currentScreen.id == 'discover'",
                      JsConditionWaiter::Options::kNone)
        .Wait();
    LOG(INFO) << "OobeInteractiveUITest: Switched to 'discover' screen.";
  }

  void RunDiscoverScreenChecks() {
    js_checker_.ExpectTrue("!$('discover').hidden");
    js_checker_.ExpectTrue("!$('discover-impl').hidden");
    js_checker_.ExpectTrue(
        "!$('discover-impl').root.querySelector('discover-pin-setup-module')."
        "hidden");
    js_checker_.ExpectTrue(
        "!$('discover-impl').root.querySelector('discover-pin-setup-module').$."
        "setup.hidden");
    EXPECT_TRUE(quick_unlock_private_get_auth_token_password_.has_value());
    EXPECT_EQ(quick_unlock_private_get_auth_token_password_,
              OobeBaseTest::kFakeUserPassword);
  }

  void ExitDiscoverPinSetupScreen() {
    // This might be the last step in flow. Synchronious execute gets stuck as
    // WebContents may be destroyed in the process. So it may never return.
    // So we use ExecuteAsync() here.
    js_checker_.ExecuteAsync(
        "$('discover-impl').root.querySelector('discover-pin-setup-module')."
        "$.setupSkipButton.click()");
    JsConditionWaiter(js_checker_,
                      "Oobe.getInstance().currentScreen.id != 'discover'",
                      JsConditionWaiter::Options::kSatisifyIfOobeDestroyed)
        .Wait();
    LOG(INFO) << "OobeInteractiveUITest: 'discover' screen done.";
  }

  void WaitForUserImageScreen() {
    JsConditionWaiter(js_checker_,
                      "Oobe.getInstance().currentScreen.id == 'user-image'",
                      JsConditionWaiter::Options::kNone)
        .Wait();

    LOG(INFO) << "OobeInteractiveUITest: Switched to 'user-image' screen.";
  }

  void SimpleEndToEnd();

  base::Optional<std::string> quick_unlock_private_get_auth_token_password_;
  base::Optional<Parameters> params_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OobeInteractiveUITest);
};

void OobeInteractiveUITest::SimpleEndToEnd() {
  ASSERT_TRUE(params_.has_value());
  ScopedQuickUnlockPrivateGetAuthTokenFunctionObserver scoped_observer(this);

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

  if (quick_unlock::IsEnabledForTesting()) {
    WaitForFingerprintScreen();
    RunFingerprintScreenChecks();
    ExitFingerprintPinSetupScreen();
  }

  if (TabletModeClient::Get()->tablet_mode_enabled()) {
    WaitForDiscoverScreen();
    RunDiscoverScreenChecks();
    ExitDiscoverPinSetupScreen();
  }

  WaitForLoginDisplayHostShutdown();
}

IN_PROC_BROWSER_TEST_P(OobeInteractiveUITest, SimpleEndToEnd) {
  SimpleEndToEnd();
}

INSTANTIATE_TEST_CASE_P(OobeInteractiveUITestImpl,
                        OobeInteractiveUITest,
                        testing::Combine(testing::Bool(), testing::Bool()));

}  //  namespace chromeos
