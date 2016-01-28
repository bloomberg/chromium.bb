// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/oobe_base_test.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/session/user_session_manager_test_api.h"
#include "chrome/browser/chromeos/login/test/https_forwarder.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/webui/signin/get_auth_frame.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/fake_shill_manager_client.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/user_manager/fake_user_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace chromeos {

namespace {
const char kGAIAHost[] = "accounts.google.com";
}

// static
const char OobeBaseTest::kFakeUserEmail[] = "fake-email@gmail.com";
const char OobeBaseTest::kFakeUserPassword[] = "fake-password";
const char OobeBaseTest::kFakeUserGaiaId[] = "fake-gaiaId";
const char OobeBaseTest::kFakeSIDCookie[] = "fake-SID-cookie";
const char OobeBaseTest::kFakeLSIDCookie[] = "fake-LSID-cookie";

OobeBaseTest::OobeBaseTest()
    : fake_gaia_(new FakeGaia()),
      network_portal_detector_(NULL),
      needs_background_networking_(false),
      gaia_frame_parent_("signin-frame"),
      initialize_fake_merge_session_(true) {
  set_exit_when_last_browser_closes(false);
  set_chromeos_user_ = false;
}

OobeBaseTest::~OobeBaseTest() {
}

void OobeBaseTest::SetUp() {
  base::FilePath test_data_dir;
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  embedded_test_server()->ServeFilesFromDirectory(test_data_dir);

  RegisterAdditionalRequestHandlers();

  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&FakeGaia::HandleRequest, base::Unretained(fake_gaia_.get())));

  // Don't spin up the IO thread yet since no threads are allowed while
  // spawning sandbox host process. See crbug.com/322732.
  ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

  // Start https wrapper here so that the URLs can be pointed at it in
  // SetUpCommandLine().
  InitHttpsForwarders();

  ExtensionApiTest::SetUp();
}

void OobeBaseTest::SetUpInProcessBrowserTestFixture() {
  host_resolver()->AddRule("*", "127.0.0.1");
  network_portal_detector_ = new NetworkPortalDetectorTestImpl();
  network_portal_detector::InitializeForTesting(network_portal_detector_);
  network_portal_detector_->SetDefaultNetworkForTesting(
      FakeShillManagerClient::kFakeEthernetNetworkGuid);

  ExtensionApiTest::SetUpInProcessBrowserTestFixture();
}

void OobeBaseTest::SetUpOnMainThread() {
  if (initialize_fake_merge_session()) {
    fake_gaia_->SetFakeMergeSessionParams(kFakeUserEmail, kFakeSIDCookie,
                                          kFakeLSIDCookie);
  }

  // Start the accept thread as the sandbox host process has already been
  // spawned.
  embedded_test_server()->StartAcceptingConnections();

  login_screen_load_observer_.reset(new content::WindowedNotificationObserver(
      chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
      content::NotificationService::AllSources()));

  js_checker_.set_web_contents(
      LoginDisplayHost::default_host()->GetWebUILoginView()->GetWebContents());

  test::UserSessionManagerTestApi session_manager_test_api(
      UserSessionManager::GetInstance());
  session_manager_test_api.SetShouldObtainTokenHandleInTests(false);

  LoginDisplayHostImpl::DisableRestrictiveProxyCheckForTest();

  ExtensionApiTest::SetUpOnMainThread();
}

void OobeBaseTest::TearDownOnMainThread() {
  // If the login display is still showing, exit gracefully.
  if (LoginDisplayHost::default_host()) {
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(&chrome::AttemptExit));
    content::RunMessageLoop();
  }
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  ExtensionApiTest::TearDownOnMainThread();
}

void OobeBaseTest::SetUpCommandLine(base::CommandLine* command_line) {
  ExtensionApiTest::SetUpCommandLine(command_line);

  command_line->AppendSwitch(chromeos::switches::kLoginManager);
  command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
  if (!needs_background_networking_)
    command_line->AppendSwitch(::switches::kDisableBackgroundNetworking);
  command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");

  GURL gaia_url = gaia_https_forwarder_.GetURLForSSLHost(std::string());
  command_line->AppendSwitchASCII(::switches::kGaiaUrl, gaia_url.spec());
  command_line->AppendSwitchASCII(::switches::kLsoUrl, gaia_url.spec());
  command_line->AppendSwitchASCII(::switches::kGoogleApisUrl,
                                  gaia_url.spec());

  fake_gaia_->Initialize();
  fake_gaia_->set_issue_oauth_code_cookie(true);
}

void OobeBaseTest::InitHttpsForwarders() {
  ASSERT_TRUE(gaia_https_forwarder_.Initialize(
      kGAIAHost, embedded_test_server()->base_url()));
}

void OobeBaseTest::RegisterAdditionalRequestHandlers() {
}

void OobeBaseTest::SimulateNetworkOffline() {
  NetworkPortalDetector::CaptivePortalState offline_state;
  offline_state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE;
  network_portal_detector_->SetDetectionResultsForTesting(
      FakeShillManagerClient::kFakeEthernetNetworkGuid,
      offline_state);
  network_portal_detector_->NotifyObserversForTesting();
}

base::Closure OobeBaseTest::SimulateNetworkOfflineClosure() {
  return base::Bind(&OobeBaseTest::SimulateNetworkOffline,
                    base::Unretained(this));
}

void OobeBaseTest::SimulateNetworkOnline() {
  NetworkPortalDetector::CaptivePortalState online_state;
  online_state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;
  online_state.response_code = 204;
  network_portal_detector_->SetDetectionResultsForTesting(
      FakeShillManagerClient::kFakeEthernetNetworkGuid,
      online_state);
  network_portal_detector_->NotifyObserversForTesting();
}

base::Closure OobeBaseTest::SimulateNetworkOnlineClosure() {
  return base::Bind(&OobeBaseTest::SimulateNetworkOnline,
                    base::Unretained(this));
}

void OobeBaseTest::SimulateNetworkPortal() {
  NetworkPortalDetector::CaptivePortalState portal_state;
  portal_state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL;
  network_portal_detector_->SetDetectionResultsForTesting(
      FakeShillManagerClient::kFakeEthernetNetworkGuid,
      portal_state);
  network_portal_detector_->NotifyObserversForTesting();
}

base::Closure OobeBaseTest::SimulateNetworkPortalClosure() {
  return base::Bind(&OobeBaseTest::SimulateNetworkPortal,
                    base::Unretained(this));
}

void OobeBaseTest::JsExpect(const std::string& expression) {
  JS().ExpectTrue(expression);
}

content::WebUI* OobeBaseTest::GetLoginUI() {
  return LoginDisplayHost::default_host()->GetOobeUI()->web_ui();
}

WebUILoginDisplay* OobeBaseTest::GetLoginDisplay() {
  ExistingUserController* controller =
      ExistingUserController::current_controller();
  CHECK(controller);
  return static_cast<WebUILoginDisplay*>(
      controller->login_display());
}

void OobeBaseTest::WaitForGaiaPageLoad() {
  WaitForSigninScreen();
  WaitForGaiaPageReload();
}

void OobeBaseTest::WaitForGaiaPageReload() {
  JS()
      .Evaluate(
          "$('gaia-signin').gaiaAuthHost_.addEventListener('ready',"
          "function f() {"
          "$(\'gaia-signin\').gaiaAuthHost_.removeEventListener(\'ready\', f);"
          "window.domAutomationController.setAutomationId(0);"
          "window.domAutomationController.send('GaiaReady');"
          "});");

  content::DOMMessageQueue message_queue;
  std::string message;
  do {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
  } while (message != "\"GaiaReady\"");
}

void OobeBaseTest::WaitForSigninScreen() {
  WizardController* wizard_controller = WizardController::default_controller();
  if (wizard_controller)
    wizard_controller->SkipToLoginForTesting(LoginScreenContext());

  WizardController::SkipPostLoginScreensForTesting();

  login_screen_load_observer_->Wait();
}

void OobeBaseTest::ExecuteJsInSigninFrame(const std::string& js) {
  content::RenderFrameHost* frame =
      signin::GetAuthFrame(GetLoginUI()->GetWebContents(), gaia_frame_parent_);
  ASSERT_TRUE(content::ExecuteScript(frame, js));
}

void OobeBaseTest::SetSignFormField(const std::string& field_id,
                                    const std::string& field_value) {
  std::string js =
      "(function(){"
      "document.getElementById('$FieldId').value = '$FieldValue';"
      "var e = new Event('input');"
      "document.getElementById('$FieldId').dispatchEvent(e);"
      "})();";
  base::ReplaceSubstringsAfterOffset(&js, 0, "$FieldId", field_id);
  base::ReplaceSubstringsAfterOffset(&js, 0, "$FieldValue", field_value);
  ExecuteJsInSigninFrame(js);
}

}  // namespace chromeos
