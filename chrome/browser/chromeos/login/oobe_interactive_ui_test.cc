// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_switches.h"
#include "base/bind.h"
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
#include "chrome/browser/chromeos/login/test/test_condition_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/update_engine_client.h"
#include "content/public/browser/notification_service.h"

namespace chromeos {
namespace {

enum class JsWaitOptions {
  kNone,
  kSatisfyIfOobeDestroyed,
};

// If |options| is kSatisfyIfOobeDestroyed, we are waiting for the end
// condition, so it is automatically fullfilled if LoginDisplayHost is already
// destroyed.
void WaitForJsCondition(const std::string& js_condition,
                        JsWaitOptions options) {
  if (options == JsWaitOptions::kSatisfyIfOobeDestroyed) {
    return test::TestConditionWaiter(
               base::BindRepeating(
                   [](const std::string& js_condition) {
                     return !LoginDisplayHost::default_host() ||
                            test::OobeJS().GetBool(js_condition);
                   },
                   js_condition))
        .Wait();
  }
  return test::TestConditionWaiter(base::BindRepeating(
                                       [](const std::string& js_condition) {
                                         return test::OobeJS().GetBool(
                                             js_condition);
                                       },
                                       js_condition))
      .Wait();
}

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

}  // namespace

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
    test::TestConditionWaiter(base::BindRepeating([]() {
      return !LoginDisplayHost::default_host();
    })).Wait();
    LOG(INFO) << "OobeInteractiveUITest: LoginDisplayHost is down.";
  }

  void WaitForOobeWelcomeScreen() {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
        content::NotificationService::AllSources());
    observer.Wait();

    WaitForJsCondition("Oobe.getInstance().currentScreen.id == 'connect'",
                       JsWaitOptions::kNone);
  }

  void RunWelcomeScreenChecks() {
#if defined(GOOGLE_CHROME_BUILD)
    constexpr int kNumberOfVideosPlaying = 1;
#else
    constexpr int kNumberOfVideosPlaying = 0;
#endif

    test::OobeJS().ExpectTrue("!$('oobe-welcome-md').$.welcomeScreen.hidden");
    test::OobeJS().ExpectTrue(
        "$('oobe-welcome-md').$.accessibilityScreen.hidden");
    test::OobeJS().ExpectTrue("$('oobe-welcome-md').$.languageScreen.hidden");
    test::OobeJS().ExpectTrue("$('oobe-welcome-md').$.timezoneScreen.hidden");

    test::OobeJS().ExpectEQ(
        "(() => {let cnt = 0; for (let v of "
        "$('oobe-welcome-md').$.welcomeScreen.root.querySelectorAll('video')) "
        "{  cnt += v.paused ? 0 : 1; }; return cnt; })()",
        kNumberOfVideosPlaying);
  }

  void TapWelcomeNext() {
    test::OobeJS().ExecuteAsync(
        "$('oobe-welcome-md').$.welcomeScreen.$.welcomeNextButton.click()");
  }

  void WaitForNetworkSelectionScreen() {
    WaitForJsCondition(
        "Oobe.getInstance().currentScreen.id == 'network-selection'",
        JsWaitOptions::kNone);
    LOG(INFO)
        << "OobeInteractiveUITest: Switched to 'network-selection' screen.";
  }

  void RunNetworkSelectionScreenChecks() {
    test::OobeJS().ExpectTrue(
        "!$('oobe-network-md').$.networkDialog.querySelector('oobe-next-button'"
        ").disabled");
  }

  void TapNetworkSelectionNext() {
    test::OobeJS().ExecuteAsync(
        "$('oobe-network-md').$.networkDialog.querySelector('oobe-next-button')"
        ".click()");
  }

  void WaitForEulaScreen() {
    WaitForJsCondition("Oobe.getInstance().currentScreen.id == 'eula'",
                       JsWaitOptions::kNone);
    LOG(INFO) << "OobeInteractiveUITest: Switched to 'eula' screen.";
  }

  void RunEulaScreenChecks() {
    // Wait for actual EULA to appear.
    WaitForJsCondition("!$('oobe-eula-md').$.eulaDialog.hidden",
                       JsWaitOptions::kNone);
    test::OobeJS().ExpectTrue("!$('oobe-eula-md').$.acceptButton.disabled");
  }

  void TapEulaAccept() {
    test::OobeJS().ExecuteAsync("$('oobe-eula-md').$.acceptButton.click();");
  }

  void WaitForUpdateScreen() {
    WaitForJsCondition("Oobe.getInstance().currentScreen.id == 'update'",
                       JsWaitOptions::kNone);
    WaitForJsCondition("!$('update').hidden", JsWaitOptions::kNone);

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
    WaitForJsCondition("Oobe.getInstance().currentScreen.id == 'gaia-signin'",
                       JsWaitOptions::kNone);
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
    WaitForJsCondition("Oobe.getInstance().currentScreen.id == 'sync-consent'",
                       JsWaitOptions::kNone);
  }

  void ExitScreenSyncConsent() {
    SyncConsentScreen* screen = static_cast<SyncConsentScreen*>(
        WizardController::default_controller()->GetScreen(
            OobeScreen::SCREEN_SYNC_CONSENT));

    screen->SetProfileSyncDisabledByPolicyForTesting(true);
    screen->OnStateChanged(nullptr);
    LOG(INFO) << "OobeInteractiveUITest: Waiting for 'sync-consent' screen "
                 "to close.";
    WaitForJsCondition("Oobe.getInstance().currentScreen.id != 'sync-consent'",
                       JsWaitOptions::kSatisfyIfOobeDestroyed);
  }

  void WaitForFingerprintScreen() {
    LOG(INFO)
        << "OobeInteractiveUITest: Waiting for 'fingerprint-setup' screen.";
    WaitForJsCondition(
        "Oobe.getInstance().currentScreen.id == 'fingerprint-setup'",
        JsWaitOptions::kNone);
    LOG(INFO) << "OobeInteractiveUITest: Waiting for fingerprint setup screen "
                 "to show.";
    WaitForJsCondition("!$('fingerprint-setup').hidden", JsWaitOptions::kNone);
    LOG(INFO) << "OobeInteractiveUITest: Waiting for fingerprint setup screen "
                 "to initializes.";
    WaitForJsCondition("!$('fingerprint-setup-impl').hidden",
                       JsWaitOptions::kNone);
    LOG(INFO) << "OobeInteractiveUITest: Waiting for fingerprint setup screen "
                 "to show setupFingerprint.";
    WaitForJsCondition("!$('fingerprint-setup-impl').$.setupFingerprint.hidden",
                       JsWaitOptions::kNone);
  }

  void RunFingerprintScreenChecks() {
    test::OobeJS().ExpectTrue("!$('fingerprint-setup').hidden");
    test::OobeJS().ExpectTrue("!$('fingerprint-setup-impl').hidden");
    test::OobeJS().ExpectTrue(
        "!$('fingerprint-setup-impl').$.setupFingerprint.hidden");
    test::OobeJS().ExecuteAsync(
        "$('fingerprint-setup-impl').$.showSensorLocationButton.click()");
    test::OobeJS().ExpectTrue(
        "$('fingerprint-setup-impl').$.setupFingerprint.hidden");
    LOG(INFO) << "OobeInteractiveUITest: Waiting for fingerprint setup "
                 "to switch to placeFinger.";
    WaitForJsCondition("!$('fingerprint-setup-impl').$.placeFinger.hidden",
                       JsWaitOptions::kNone);
  }

  void ExitFingerprintPinSetupScreen() {
    test::OobeJS().ExpectTrue(
        "!$('fingerprint-setup-impl').$.placeFinger.hidden");
    // This might be the last step in flow. Synchronious execute gets stuck as
    // WebContents may be destroyed in the process. So it may never return.
    // So we use ExecuteAsync() here.
    test::OobeJS().ExecuteAsync(
        "$('fingerprint-setup-impl').$.setupFingerprintLater.click()");
    LOG(INFO) << "OobeInteractiveUITest: Waiting for fingerprint setup screen "
                 "to close.";
    WaitForJsCondition(
        "Oobe.getInstance().currentScreen.id !="
        "'fingerprint-setup'",
        JsWaitOptions::kSatisfyIfOobeDestroyed);
    LOG(INFO) << "OobeInteractiveUITest: 'fingerprint-setup' screen done.";
  }

  void WaitForDiscoverScreen() {
    WaitForJsCondition("Oobe.getInstance().currentScreen.id == 'discover'",
                       JsWaitOptions::kNone);
    LOG(INFO) << "OobeInteractiveUITest: Switched to 'discover' screen.";
  }

  void RunDiscoverScreenChecks() {
    test::OobeJS().ExpectTrue("!$('discover').hidden");
    test::OobeJS().ExpectTrue("!$('discover-impl').hidden");
    test::OobeJS().ExpectTrue(
        "!$('discover-impl').root.querySelector('discover-pin-setup-module')."
        "hidden");
    test::OobeJS().ExpectTrue(
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
    test::OobeJS().ExecuteAsync(
        "$('discover-impl').root.querySelector('discover-pin-setup-module')."
        "$.setupSkipButton.click()");
    WaitForJsCondition("Oobe.getInstance().currentScreen.id != 'discover'",
                       JsWaitOptions::kSatisfyIfOobeDestroyed);
    LOG(INFO) << "OobeInteractiveUITest: 'discover' screen done.";
  }

  void WaitForUserImageScreen() {
    WaitForJsCondition("Oobe.getInstance().currentScreen.id == 'user-image'",
                       JsWaitOptions::kNone);

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

// Flaky on MSAN/ASAN/LSAN: crbug.com/891277, crbug.com/891484.
#if defined(MEMORY_SANITIZER) || defined(LEAK_SANITIZER) || \
    defined(ADDRESS_SANITIZER)
#define MAYBE_SimpleEndToEnd DISABLED_SimpleEndToEnd
#else
#define MAYBE_SimpleEndToEnd SimpleEndToEnd
#endif
IN_PROC_BROWSER_TEST_P(OobeInteractiveUITest, MAYBE_SimpleEndToEnd) {
  SimpleEndToEnd();
}

INSTANTIATE_TEST_SUITE_P(OobeInteractiveUITestImpl,
                         OobeInteractiveUITest,
                         testing::Combine(testing::Bool(), testing::Bool()));

}  //  namespace chromeos
