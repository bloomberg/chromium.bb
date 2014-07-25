// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/oobe_base_test.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/users/fake_user_manager.h"
#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/fake_shill_manager_client.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace chromeos {

OobeBaseTest::OobeBaseTest()
    : fake_gaia_(new FakeGaia()),
      network_portal_detector_(NULL),
      needs_background_networking_(false) {
  set_exit_when_last_browser_closes(false);
  set_chromeos_user_ = false;
}

OobeBaseTest::~OobeBaseTest() {
}

void OobeBaseTest::SetUp() {
  base::FilePath test_data_dir;
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&FakeGaia::HandleRequest, base::Unretained(fake_gaia_.get())));
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  // Stop IO thread here because no threads are allowed while
  // spawning sandbox host process. See crbug.com/322732.
  embedded_test_server()->StopThread();

  ExtensionApiTest::SetUp();
}

void OobeBaseTest::SetUpInProcessBrowserTestFixture() {
  host_resolver()->AddRule("*", "127.0.0.1");
  network_portal_detector_ = new NetworkPortalDetectorTestImpl();
  NetworkPortalDetector::InitializeForTesting(network_portal_detector_);
  network_portal_detector_->SetDefaultNetworkForTesting(
      FakeShillManagerClient::kFakeEthernetNetworkGuid);

  ExtensionApiTest::SetUpInProcessBrowserTestFixture();
}

void OobeBaseTest::SetUpOnMainThread() {
  // Restart the thread as the sandbox host process has already been spawned.
  embedded_test_server()->RestartThreadAndListen();

  ExtensionApiTest::SetUpOnMainThread();
}

void OobeBaseTest::TearDownOnMainThread() {
  // If the login display is still showing, exit gracefully.
  if (LoginDisplayHostImpl::default_host()) {
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(&chrome::AttemptExit));
    content::RunMessageLoop();
  }

  ExtensionApiTest::TearDownOnMainThread();
}

void OobeBaseTest::SetUpCommandLine(CommandLine* command_line) {
  ExtensionApiTest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(chromeos::switches::kLoginManager);
  command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
  if (!needs_background_networking_)
    command_line->AppendSwitch(::switches::kDisableBackgroundNetworking);
  command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");

  // Create gaia and webstore URL from test server url but using different
  // host names. This is to avoid gaia response being tagged as from
  // webstore in chrome_resource_dispatcher_host_delegate.cc.
  const GURL& server_url = embedded_test_server()->base_url();

  std::string gaia_host("gaia");
  GURL::Replacements replace_gaia_host;
  replace_gaia_host.SetHostStr(gaia_host);
  GURL gaia_url = server_url.ReplaceComponents(replace_gaia_host);
  command_line->AppendSwitchASCII(::switches::kGaiaUrl, gaia_url.spec());
  command_line->AppendSwitchASCII(::switches::kLsoUrl, gaia_url.spec());
  command_line->AppendSwitchASCII(::switches::kGoogleApisUrl,
                                  gaia_url.spec());
  fake_gaia_->Initialize();
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
  bool result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetLoginUI()->GetWebContents(),
      "window.domAutomationController.send(!!(" + expression + "));",
       &result));
  ASSERT_TRUE(result) << expression;
}

content::WebUI* OobeBaseTest::GetLoginUI() {
  return static_cast<chromeos::LoginDisplayHostImpl*>(
      chromeos::LoginDisplayHostImpl::default_host())->GetOobeUI()->web_ui();
}

SigninScreenHandler* OobeBaseTest::GetSigninScreenHandler() {
  return static_cast<chromeos::LoginDisplayHostImpl*>(
      chromeos::LoginDisplayHostImpl::default_host())
      ->GetOobeUI()
      ->signin_screen_handler_for_test();
}

WebUILoginDisplay* OobeBaseTest::GetLoginDisplay() {
  ExistingUserController* controller =
      ExistingUserController::current_controller();
  CHECK(controller);
  return static_cast<WebUILoginDisplay*>(
      controller->login_display());
}

}  // namespace chromeos
