// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/arc/arc_service_launcher.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/extensions/quick_unlock_private/quick_unlock_private_api.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/chromeos/login/screens/recommend_apps/recommend_apps_fetcher.h"
#include "chrome/browser/chromeos/login/screens/recommend_apps/recommend_apps_fetcher_delegate.h"
#include "chrome/browser/chromeos/login/screens/recommend_apps/scoped_test_recommend_apps_fetcher_factory.h"
#include "chrome/browser/chromeos/login/screens/sync_consent_screen.h"
#include "chrome/browser/chromeos/login/screens/update_screen.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/test_predicate_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "chrome/browser/ui/webui/chromeos/login/app_downloading_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/assistant_optin_flow_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/discover_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/eula_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/fingerprint_setup_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/recommend_apps_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/update_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/update_engine_client.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_session_runner.h"
#include "components/arc/test/fake_arc_session.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace chromeos {
namespace {

enum class ArcState { kNotAvailable, kAcceptTerms, kDeclineTerms };

std::string ArcStateToString(ArcState arc_state) {
  switch (arc_state) {
    case ArcState::kNotAvailable:
      return "not-available";
    case ArcState::kAcceptTerms:
      return "accept-terms";
    case ArcState::kDeclineTerms:
      return "decline-terms";
  }
  NOTREACHED();
  return "unknown";
}

class FakeRecommendAppsFetcher : public RecommendAppsFetcher {
 public:
  explicit FakeRecommendAppsFetcher(RecommendAppsFetcherDelegate* delegate)
      : delegate_(delegate) {}
  ~FakeRecommendAppsFetcher() override = default;

  // RecommendAppsFetcher:
  void Start() override {
    base::Value app(base::Value::Type::DICTIONARY);
    app.SetKey("package_name", base::Value("test.package"));
    base::Value app_list(base::Value::Type::LIST);
    app_list.GetList().emplace_back(std::move(app));
    delegate_->OnLoadSuccess(std::move(app_list));
  }
  void Retry() override { NOTREACHED(); }

 private:
  RecommendAppsFetcherDelegate* const delegate_;
};

std::unique_ptr<RecommendAppsFetcher> CreateRecommendAppsFetcher(
    RecommendAppsFetcherDelegate* delegate) {
  return std::make_unique<FakeRecommendAppsFetcher>(delegate);
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
      public ::testing::WithParamInterface<std::tuple<bool, bool, ArcState>> {
 public:
  struct Parameters {
    bool is_tablet;
    bool is_quick_unlock_enabled;
    ArcState arc_state;

    std::string ToString() const {
      return std::string("{is_tablet: ") + (is_tablet ? "true" : "false") +
             ", is_quick_unlock_enabled: " +
             (is_quick_unlock_enabled ? "true" : "false") +
             ", arc_state: " + ArcStateToString(arc_state) + "}";
    }
  };

  OobeInteractiveUITest() = default;
  ~OobeInteractiveUITest() override = default;

  void SetUp() override {
    params_ = Parameters();
    std::tie(params_->is_tablet, params_->is_quick_unlock_enabled,
             params_->arc_state) = GetParam();
    LOG(INFO) << "OobeInteractiveUITest() started with params "
              << params_->ToString();
    if (params_->arc_state != ArcState::kNotAvailable)
      feature_list_.InitAndEnableFeature(switches::kAssistantFeature);
    OobeBaseTest::SetUp();
  }

  void TearDown() override {
    quick_unlock::EnabledForTesting(false);
    OobeBaseTest::TearDown();
    params_.reset();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);

    if (params_->is_tablet)
      command_line->AppendSwitch(ash::switches::kAshEnableTabletMode);

    if (params_->arc_state != ArcState::kNotAvailable) {
      arc::SetArcAvailableCommandLineForTesting(command_line);
      // Prevent encryption migration screen from showing up after user login
      // with ARC available.
      command_line->AppendSwitch(switches::kDisableEncryptionMigration);
    }
  }

  void SetUpInProcessBrowserTestFixture() override {
    OobeBaseTest::SetUpInProcessBrowserTestFixture();

    if (params_->is_quick_unlock_enabled)
      quick_unlock::EnabledForTesting(true);

    if (params_->arc_state != ArcState::kNotAvailable) {
      recommend_apps_fetcher_factory_ =
          std::make_unique<ScopedTestRecommendAppsFetcherFactory>(
              base::BindRepeating(&CreateRecommendAppsFetcher));
      arc_tos_server_.RegisterRequestHandler(base::BindRepeating(
          &OobeInteractiveUITest::HandleRequest, base::Unretained(this)));
    }
  }

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();

    if (params_->is_tablet)
      TabletModeClient::Get()->OnTabletModeToggled(true);

    if (params_->arc_state != ArcState::kNotAvailable) {
      // Init ArcSessionManager for testing.
      arc::ArcServiceLauncher::Get()->ResetForTesting();
      arc::ArcSessionManager::Get()->SetArcSessionRunnerForTesting(
          std::make_unique<arc::ArcSessionRunner>(
              base::BindRepeating(arc::FakeArcSession::Create)));

      test::OobeJS().Evaluate(base::StringPrintf(
          "login.ArcTermsOfServiceScreen.setTosHostNameForTesting('%s');",
          arc_tos_server_.GetURL("/arc-tos").spec().c_str()));
    }
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

  void TearDownInProcessBrowserTestFixture() override {
    recommend_apps_fetcher_factory_.reset();
    OobeBaseTest::TearDownInProcessBrowserTestFixture();
  }

  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    auto response = std::make_unique<BasicHttpResponse>();
    if (request.relative_url != "/arc-tos/about/play-terms.html") {
      response->set_code(net::HTTP_NOT_FOUND);
    } else {
      response->set_code(net::HTTP_OK);
      response->set_content("<html><body>Test Terms of Service</body></html>");
      response->set_content_type("text/html");
    }
    return response;
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
    test::TestPredicateWaiter(base::BindRepeating([]() {
      return !LoginDisplayHost::default_host();
    })).Wait();
    LOG(INFO) << "OobeInteractiveUITest: LoginDisplayHost is down.";
  }

  void WaitForOobeWelcomeScreen() {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
        content::NotificationService::AllSources());
    observer.Wait();
    OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  }

  void RunWelcomeScreenChecks() {
#if defined(GOOGLE_CHROME_BUILD)
    constexpr int kNumberOfVideosPlaying = 1;
#else
    constexpr int kNumberOfVideosPlaying = 0;
#endif
    test::OobeJS().ExpectVisiblePath({"connect", "welcomeScreen"});
    test::OobeJS().ExpectHiddenPath({"connect", "accessibilityScreen"});
    test::OobeJS().ExpectHiddenPath({"connect", "languageScreen"});
    test::OobeJS().ExpectHiddenPath({"connect", "timezoneScreen"});

    test::OobeJS().ExpectEQ(
        "(() => {let cnt = 0; for (let v of "
        "$('connect').$.welcomeScreen.root.querySelectorAll('video')) "
        "{  cnt += v.paused ? 0 : 1; }; return cnt; })()",
        kNumberOfVideosPlaying);
  }

  void TapWelcomeNext() {
    test::OobeJS().TapOnPath({"connect", "welcomeScreen", "welcomeNextButton"});
  }

  void WaitForNetworkSelectionScreen() {
    OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
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
    OobeScreenWaiter(EulaView::kScreenId).Wait();
    LOG(INFO) << "OobeInteractiveUITest: Switched to 'eula' screen.";
  }

  void RunEulaScreenChecks() {
    // Wait for actual EULA to appear.
    test::OobeJS()
        .CreateVisibilityWaiter(true, {"oobe-eula-md", "eulaDialog"})
        ->Wait();
    test::OobeJS().ExpectTrue("!$('oobe-eula-md').$.acceptButton.disabled");
  }

  void TapEulaAccept() {
    test::OobeJS().TapOnPath({"oobe-eula-md", "acceptButton"});
  }

  void WaitForUpdateScreen() {
    OobeScreenWaiter(UpdateView::kScreenId).Wait();
    test::OobeJS().CreateVisibilityWaiter(true, {"update"})->Wait();

    LOG(INFO) << "OobeInteractiveUITest: Switched to 'update' screen.";
  }

  void ExitUpdateScreenNoUpdate() {
    UpdateScreen* screen = static_cast<UpdateScreen*>(
        WizardController::default_controller()->GetScreen(
            UpdateView::kScreenId));
    UpdateEngineClient::Status status;
    status.status = UpdateEngineClient::UPDATE_STATUS_ERROR;
    screen->UpdateStatusChanged(status);
  }

  void WaitForGaiaSignInScreen() {
    OobeScreenWaiter(GaiaView::kScreenId).Wait();
    LOG(INFO) << "OobeInteractiveUITest: Switched to 'gaia-signin' screen.";
  }

  void LogInAsRegularUser() {
    LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetView<GaiaScreenHandler>()
        ->ShowSigninScreenForTest(FakeGaiaMixin::kFakeUserEmail,
                                  FakeGaiaMixin::kFakeUserPassword,
                                  FakeGaiaMixin::kEmptyUserServices);
    LOG(INFO) << "OobeInteractiveUITest: Logged in.";
  }

  void WaitForSyncConsentScreen() {
    LOG(INFO) << "OobeInteractiveUITest: Waiting for 'sync-consent' screen.";
    OobeScreenWaiter(SyncConsentScreenView::kScreenId).Wait();
  }

  void ExitScreenSyncConsent() {
    SyncConsentScreen* screen = static_cast<SyncConsentScreen*>(
        WizardController::default_controller()->GetScreen(
            SyncConsentScreenView::kScreenId));

    screen->SetProfileSyncDisabledByPolicyForTesting(true);
    screen->OnStateChanged(nullptr);
    LOG(INFO) << "OobeInteractiveUITest: Waiting for 'sync-consent' screen "
                 "to close.";
    OobeScreenExitWaiter(SyncConsentScreenView::kScreenId).Wait();
  }

  void WaitForFingerprintScreen() {
    LOG(INFO)
        << "OobeInteractiveUITest: Waiting for 'fingerprint-setup' screen.";
    OobeScreenWaiter(FingerprintSetupScreenView::kScreenId).Wait();
    LOG(INFO) << "OobeInteractiveUITest: Waiting for fingerprint setup screen "
                 "to show.";
    test::OobeJS().CreateVisibilityWaiter(true, {"fingerprint-setup"})->Wait();
    LOG(INFO) << "OobeInteractiveUITest: Waiting for fingerprint setup screen "
                 "to initializes.";
    test::OobeJS()
        .CreateVisibilityWaiter(true, {"fingerprint-setup-impl"})
        ->Wait();
    LOG(INFO) << "OobeInteractiveUITest: Waiting for fingerprint setup screen "
                 "to show setupFingerprint.";
    test::OobeJS()
        .CreateVisibilityWaiter(true,
                                {"fingerprint-setup-impl", "setupFingerprint"})
        ->Wait();
  }

  void RunFingerprintScreenChecks() {
    test::OobeJS().ExpectVisible("fingerprint-setup");
    test::OobeJS().ExpectVisible("fingerprint-setup-impl");
    test::OobeJS().ExpectVisiblePath(
        {"fingerprint-setup-impl", "setupFingerprint"});
    test::OobeJS().TapOnPath(
        {"fingerprint-setup-impl", "showSensorLocationButton"});
    test::OobeJS().ExpectHiddenPath(
        {"fingerprint-setup-impl", "setupFingerprint"});
    LOG(INFO) << "OobeInteractiveUITest: Waiting for fingerprint setup "
                 "to switch to placeFinger.";
    test::OobeJS()
        .CreateVisibilityWaiter(true, {"fingerprint-setup-impl", "placeFinger"})
        ->Wait();
  }

  void ExitFingerprintPinSetupScreen() {
    test::OobeJS().ExpectVisiblePath({"fingerprint-setup-impl", "placeFinger"});
    // This might be the last step in flow. Synchronious execute gets stuck as
    // WebContents may be destroyed in the process. So it may never return.
    // So we use ExecuteAsync() here.
    test::OobeJS().ExecuteAsync(
        "$('fingerprint-setup-impl').$.setupFingerprintLater.click()");
    LOG(INFO) << "OobeInteractiveUITest: Waiting for fingerprint setup screen "
                 "to close.";
    OobeScreenExitWaiter(FingerprintSetupScreenView::kScreenId).Wait();
    LOG(INFO) << "OobeInteractiveUITest: 'fingerprint-setup' screen done.";
  }

  void WaitForDiscoverScreen() {
    OobeScreenWaiter(DiscoverScreenView::kScreenId).Wait();
    LOG(INFO) << "OobeInteractiveUITest: Switched to 'discover' screen.";
  }

  void RunDiscoverScreenChecks() {
    test::OobeJS().ExpectVisible("discover");
    test::OobeJS().ExpectVisible("discover-impl");
    test::OobeJS().ExpectTrue(
        "!$('discover-impl').root.querySelector('discover-pin-setup-module')."
        "hidden");
    test::OobeJS().ExpectTrue(
        "!$('discover-impl').root.querySelector('discover-pin-setup-module').$."
        "setup.hidden");
    EXPECT_TRUE(quick_unlock_private_get_auth_token_password_.has_value());
    EXPECT_EQ(quick_unlock_private_get_auth_token_password_,
              FakeGaiaMixin::kFakeUserPassword);
  }

  void ExitDiscoverPinSetupScreen() {
    // This might be the last step in flow. Synchronious execute gets stuck as
    // WebContents may be destroyed in the process. So it may never return.
    // So we use ExecuteAsync() here.
    test::OobeJS().ExecuteAsync(
        "$('discover-impl').root.querySelector('discover-pin-setup-module')."
        "$.setupSkipButton.click()");
    OobeScreenExitWaiter(DiscoverScreenView::kScreenId).Wait();
    LOG(INFO) << "OobeInteractiveUITest: 'discover' screen done.";
  }

  // Waits for the ARC terms of service screen to be shown, it accepts or
  // declines the terms based in |accept_terms| value, and waits for the flow to
  // leave the ARC terms of service screen.
  void HandleArcTermsOfServiceScreen(bool accept_terms) {
    OobeScreenWaiter(ArcTermsOfServiceScreenView::kScreenId).Wait();
    LOG(INFO) << "OobeInteractiveUITest: Switched to 'arc-tos' screen.";

    test::OobeJS()
        .CreateEnabledWaiter(true, {"arc-tos-root", "arc-tos-next-button"})
        ->Wait();
    test::OobeJS().Evaluate(
        test::GetOobeElementPath({"arc-tos-root", "arc-tos-next-button"}) +
        ".click()");
    test::OobeJS()
        .CreateVisibilityWaiter(true, {"arc-tos-root", "arc-location-service"})
        ->Wait();
    test::OobeJS()
        .CreateVisibilityWaiter(true, {"arc-tos-root", "arc-tos-accept-button"})
        ->Wait();

    const std::string button_to_click =
        accept_terms ? "arc-tos-accept-button" : "arc-tos-skip-button";
    test::OobeJS().Evaluate(
        test::GetOobeElementPath({"arc-tos-root", button_to_click}) +
        ".click()");

    OobeScreenExitWaiter(ArcTermsOfServiceScreenView::kScreenId).Wait();
    LOG(INFO) << "OobeInteractiveUITest: 'arc-tos' screen done.";
  }

  // Waits for the recommend apps screen to be shown, selects the single app
  // reported by FakeRecommendAppsFetcher, and requests the apps install. It
  // will wait for the flow to progress away from the RecommendAppsScreen before
  // returning.
  // This assumes that ARC terms of service have bee accepted in
  // HandleArcTermsOfServiceScreen.
  void HandleRecommendAppsScreen() {
    OobeScreenWaiter(RecommendAppsScreenView::kScreenId).Wait();
    LOG(INFO) << "OobeInteractiveUITest: Switched to 'recommend-apps' screen.";

    test::OobeJS()
        .CreateDisplayedWaiter(true, {"recommend-apps-screen", "app-list-view"})
        ->Wait();

    std::string toggle_apps_script = base::StringPrintf(
        "(function() {"
        "  if (!document.getElementById('recommend-apps-container'))"
        "    return false;"
        "  var items ="
        "      Array.from(document.getElementById('recommend-apps-container')"
        "         .querySelectorAll('.item') || [])"
        "         .filter(i => '%s' == i.getAttribute('data-packagename'));"
        "  if (items.length == 0)"
        "    return false;"
        "  items.forEach(i => i.querySelector('.image-picker').click());"
        "  return true;"
        "})();",
        "test.package");

    const std::string webview_path =
        test::GetOobeElementPath({"recommend-apps-screen", "app-list-view"});
    const std::string script = base::StringPrintf(
        "(function() {"
        "  var toggleApp = function() {"
        "    %s.executeScript({code: \"%s\"}, r => {"
        "      if (!r || !r[0]) {"
        "        setTimeout(toggleApp, 50);"
        "        return;"
        "      }"
        "      window.domAutomationController.send(true);"
        "    });"
        "  };"
        "  toggleApp();"
        "})();",
        webview_path.c_str(), toggle_apps_script.c_str());

    bool result;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        LoginDisplayHost::default_host()->GetOobeWebContents(), script,
        &result));
    EXPECT_TRUE(result);

    const std::initializer_list<base::StringPiece> install_button = {
        "recommend-apps-screen", "recommend-apps-install-button"};
    test::OobeJS().CreateEnabledWaiter(true, install_button)->Wait();
    test::OobeJS().TapOnPath(install_button);

    OobeScreenExitWaiter(RecommendAppsScreenView::kScreenId).Wait();
    LOG(INFO) << "OobeInteractiveUITest: 'recommend-apps' screen done.";
  }

  // Waits for AppDownloadingScreen to be shown, clicks 'Continue' button, and
  // waits until the flow moves to the next screen. This assumes that an app was
  // selected in HandleRecommendAppsScreen.
  void HandleAppDownloadingScreen() {
    OobeScreenWaiter(AppDownloadingScreenView::kScreenId).Wait();
    LOG(INFO) << "OobeInteractiveUITest: Switched to 'app-downloading' screen.";

    const std::initializer_list<base::StringPiece> continue_button = {
        "app-downloading-screen", "app-downloading-continue-setup-button"};
    test::OobeJS().TapOnPath(continue_button);

    OobeScreenExitWaiter(AppDownloadingScreenView::kScreenId).Wait();
    LOG(INFO) << "OobeInteractiveUITest: 'app-downloading' screen done.";
  }

  // Waits for AssistantOptInFlowScreen to be shown, skips the opt-in, and waits
  // for the flow to move away from the screen.
  // Note that due to test setup, the screen will fail to load assistant value
  // proposal error (as the URL is not faked in this test), and display an
  // error, This is good enough for this tests, whose goal is to verify the
  // screen is shown, and how the setup progresses after the screen. The actual
  // assistant opt-in flow is tested separately.
  void HandleAssistantOptInScreen() {
    OobeScreenWaiter(AssistantOptInFlowScreenView::kScreenId).Wait();
    LOG(INFO) << "OobeInteractiveUITest: Switched to 'assistant-optin' screen.";

    test::OobeJS()
        .CreateVisibilityWaiter(true, {"assistant-optin-flow-card", "loading"})
        ->Wait();

    std::initializer_list<base::StringPiece> skip_button_path = {
        "assistant-optin-flow-card", "loading", "skip-button"};
    test::OobeJS().CreateEnabledWaiter(true, skip_button_path)->Wait();
    test::OobeJS().TapOnPath(skip_button_path);

    OobeScreenExitWaiter(AssistantOptInFlowScreenView::kScreenId).Wait();
    LOG(INFO) << "OobeInteractiveUITest: 'assistant-optin' screen done.";
  }

  void SimpleEndToEnd();

  base::Optional<std::string> quick_unlock_private_get_auth_token_password_;
  base::Optional<Parameters> params_;
  base::test::ScopedFeatureList feature_list_;

 private:
  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};

  net::EmbeddedTestServer arc_tos_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  EmbeddedTestServerSetupMixin arc_tos_server_setup_{&mixin_host_,
                                                     &arc_tos_server_};

  std::unique_ptr<ScopedTestRecommendAppsFetcherFactory>
      recommend_apps_fetcher_factory_;

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

  // Arc terms of service content gets preloaded when GAIA screen is shown, wait
  // for the preload to finish before proceeding - requesting reload (which may
  // happen when ARC terms of service screen is show) before the preload is done
  // may cause flaky load failures.
  // TODO(https://crbug/com/959902): Fix ARC terms of service screen to better
  //     handle this case.
  if (params_->arc_state != ArcState::kNotAvailable) {
    test::OobeJS()
        .CreateHasClassWaiter(true, "arc-tos-loaded",
                              {"arc-tos-root", "arc-tos-dialog"})
        ->Wait();
  }

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

  if (params_->is_tablet) {
    WaitForDiscoverScreen();
    RunDiscoverScreenChecks();
    ExitDiscoverPinSetupScreen();
  }

  if (params_->arc_state != ArcState::kNotAvailable) {
    HandleArcTermsOfServiceScreen(params_->arc_state == ArcState::kAcceptTerms);
  }

  if (params_->arc_state == ArcState::kAcceptTerms) {
    HandleRecommendAppsScreen();
    HandleAppDownloadingScreen();
  }

  if (params_->arc_state != ArcState::kNotAvailable) {
    HandleAssistantOptInScreen();
  }

  WaitForLoginDisplayHostShutdown();
}

IN_PROC_BROWSER_TEST_P(OobeInteractiveUITest, SimpleEndToEnd) {
  SimpleEndToEnd();
}

INSTANTIATE_TEST_SUITE_P(
    OobeInteractiveUITestImpl,
    OobeInteractiveUITest,
    testing::Combine(testing::Bool(),
                     testing::Bool(),
                     testing::Values(ArcState::kNotAvailable,
                                     ArcState::kAcceptTerms,
                                     ArcState::kDeclineTerms)));

}  //  namespace chromeos
