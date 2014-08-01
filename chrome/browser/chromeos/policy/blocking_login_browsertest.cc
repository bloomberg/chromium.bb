// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/ui/webui_login_display.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/enterprise_install_attributes.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/chromeos_switches.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/policy_switches.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

namespace em = enterprise_management;

const char kDomain[] = "domain.com";
const char kUsername[] = "user@domain.com";
const char kUsernameOtherDomain[] = "user@other.com";

const char kOAuthTokenCookie[] = "oauth_token=1234";

const char kOAuth2TokenPairData[] =
    "{"
    "  \"refresh_token\": \"1234\","
    "  \"access_token\": \"5678\","
    "  \"expires_in\": 3600"
    "}";

const char kOAuth2AccessTokenData[] =
    "{"
    "  \"access_token\": \"5678\","
    "  \"expires_in\": 3600"
    "}";

const char kDMRegisterRequest[] = "/device_management?request=register";
const char kDMPolicyRequest[] = "/device_management?request=policy";

void CopyLockResult(base::RunLoop* loop,
                    policy::EnterpriseInstallAttributes::LockResult* out,
                    policy::EnterpriseInstallAttributes::LockResult result) {
  *out = result;
  loop->Quit();
}

}  // namespace

struct BlockingLoginTestParam {
  const int steps;
  const char* username;
  const bool enroll_device;
};

class BlockingLoginTest
    : public InProcessBrowserTest,
      public content::NotificationObserver,
      public testing::WithParamInterface<BlockingLoginTestParam> {
 public:
  BlockingLoginTest() : profile_added_(NULL) {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Initialize the test server early, so that we can use its base url for
    // the command line flags.
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

    // Use the login manager screens and the gaia auth extension.
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(switches::kForceLoginManagerInTests);
    command_line->AppendSwitchASCII(switches::kLoginProfile, "user");
    command_line->AppendSwitchASCII(::switches::kAuthExtensionPath,
                                    "gaia_auth");

    // Redirect requests to gaia and the policy server to the test server.
    command_line->AppendSwitchASCII(::switches::kGaiaUrl,
                                    embedded_test_server()->base_url().spec());
    command_line->AppendSwitchASCII(::switches::kLsoUrl,
                                    embedded_test_server()->base_url().spec());
    command_line->AppendSwitchASCII(
        policy::switches::kDeviceManagementUrl,
        embedded_test_server()->GetURL("/device_management").spec());
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    fake_gaia_.Initialize();

    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&BlockingLoginTest::HandleRequest, base::Unretained(this)));
    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&FakeGaia::HandleRequest, base::Unretained(&fake_gaia_)));

    registrar_.Add(this,
                   chrome::NOTIFICATION_PROFILE_ADDED,
                   content::NotificationService::AllSources());
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    RunUntilIdle();
    EXPECT_TRUE(responses_.empty());
    STLDeleteElements(&responses_);
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    ASSERT_EQ(chrome::NOTIFICATION_PROFILE_ADDED, type);
    ASSERT_FALSE(profile_added_);
    profile_added_ = content::Source<Profile>(source).ptr();
  }

  void RunUntilIdle() {
    base::RunLoop().RunUntilIdle();
  }

  policy::BrowserPolicyConnectorChromeOS* browser_policy_connector() {
    return g_browser_process->platform_part()
        ->browser_policy_connector_chromeos();
  }

  void SkipToSigninScreen() {
    WizardController::SkipPostLoginScreensForTesting();
    WizardController* wizard_controller =
        WizardController::default_controller();
    ASSERT_TRUE(wizard_controller);
    wizard_controller->SkipToLoginForTesting(LoginScreenContext());

    content::WindowedNotificationObserver(
        chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
        content::NotificationService::AllSources()).Wait();
    RunUntilIdle();
  }

  void EnrollDevice(const std::string& username) {
    base::RunLoop loop;
    policy::EnterpriseInstallAttributes::LockResult result;
    browser_policy_connector()->GetInstallAttributes()->LockDevice(
        username, policy::DEVICE_MODE_ENTERPRISE, "100200300",
        base::Bind(&CopyLockResult, &loop, &result));
    loop.Run();
    EXPECT_EQ(policy::EnterpriseInstallAttributes::LOCK_SUCCESS, result);
    RunUntilIdle();
  }

  void Login(const std::string& username) {
    content::WindowedNotificationObserver session_started_observer(
        chrome::NOTIFICATION_SESSION_STARTED,
        content::NotificationService::AllSources());

    ExistingUserController* controller =
        ExistingUserController::current_controller();
    ASSERT_TRUE(controller);
    WebUILoginDisplay* login_display =
        static_cast<WebUILoginDisplay*>(controller->login_display());
    ASSERT_TRUE(login_display);

    login_display->ShowSigninScreenForCreds(username, "password");

    // Wait for the session to start after submitting the credentials. This
    // will wait until all the background requests are done.
    session_started_observer.Wait();
  }

  // Handles an HTTP request sent to the test server. This handler either
  // uses a canned response in |responses_| if the request path matches one
  // of the URLs that we mock, otherwise this handler delegates to |fake_gaia_|.
  scoped_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    scoped_ptr<net::test_server::HttpResponse> response;

    GaiaUrls* gaia = GaiaUrls::GetInstance();
    if (request.relative_url == gaia->client_login_to_oauth2_url().path() ||
        request.relative_url == gaia->oauth2_token_url().path() ||
        request.relative_url.find(kDMRegisterRequest) == 0 ||
        request.relative_url.find(kDMPolicyRequest) == 0) {
      if (!responses_.empty()) {
        response.reset(responses_.back());
        responses_.pop_back();
      }
    }

    return response.Pass();
  }

  // Creates a new canned response that will respond with the given HTTP
  // status |code|. That response is appended to |responses_| and will be the
  // next response used.
  // Returns a reference to that response, so that it can be further customized.
  net::test_server::BasicHttpResponse& PushResponse(net::HttpStatusCode code) {
    net::test_server::BasicHttpResponse* response =
        new net::test_server::BasicHttpResponse();
    response->set_code(code);
    responses_.push_back(response);
    return *response;
  }

  // Returns the body of the register response from the policy server.
  std::string GetRegisterResponse() {
    em::DeviceManagementResponse response;
    em::DeviceRegisterResponse* register_response =
        response.mutable_register_response();
    register_response->set_device_management_token("1234");
    register_response->set_enrollment_type(
        em::DeviceRegisterResponse::ENTERPRISE);
    std::string data;
    EXPECT_TRUE(response.SerializeToString(&data));
    return data;
  }

  // Returns the body of the fetch response from the policy server.
  std::string GetPolicyResponse() {
    em::DeviceManagementResponse response;
    response.mutable_policy_response()->add_response();
    std::string data;
    EXPECT_TRUE(response.SerializeToString(&data));
    return data;
  }

 protected:
  Profile* profile_added_;

 private:
  FakeGaia fake_gaia_;
  std::vector<net::test_server::HttpResponse*> responses_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BlockingLoginTest);
};

IN_PROC_BROWSER_TEST_P(BlockingLoginTest, LoginBlocksForUser) {
  // Verify that there isn't a logged in user when the test starts.
  UserManager* user_manager = UserManager::Get();
  EXPECT_FALSE(user_manager->IsUserLoggedIn());
  EXPECT_FALSE(browser_policy_connector()->IsEnterpriseManaged());
  EXPECT_FALSE(profile_added_);

  // Enroll the device, if enrollment is enabled for this test instance.
  if (GetParam().enroll_device) {
    EnrollDevice(kUsername);

    EXPECT_FALSE(user_manager->IsUserLoggedIn());
    EXPECT_TRUE(browser_policy_connector()->IsEnterpriseManaged());
    EXPECT_EQ(kDomain, browser_policy_connector()->GetEnterpriseDomain());
    EXPECT_FALSE(profile_added_);
    EXPECT_EQ(policy::USER_AFFILIATION_MANAGED,
              browser_policy_connector()->GetUserAffiliation(kUsername));
    RunUntilIdle();
    EXPECT_FALSE(user_manager->IsKnownUser(kUsername));
  }

  // Skip the OOBE, go to the sign-in screen, and wait for the login screen to
  // become visible.
  SkipToSigninScreen();
  EXPECT_FALSE(profile_added_);

  // Prepare the fake HTTP responses.
  if (GetParam().steps < 5) {
    // If this instance is not going to complete the entire flow successfully
    // then the last step will fail.

    // This response body is important to make the gaia fetcher skip its delayed
    // retry behavior, which makes testing harder. If this is sent to the policy
    // fetchers then it will make them fail too.
    PushResponse(net::HTTP_UNAUTHORIZED).set_content("Error=AccountDeleted");
  }

  // Push a response for each step that is going to succeed, in reverse order.
  switch (GetParam().steps) {
    default:
      ADD_FAILURE() << "Invalid step number: " << GetParam().steps;
      return;

    case 5:
      PushResponse(net::HTTP_OK).set_content(GetPolicyResponse());

    case 4:
      PushResponse(net::HTTP_OK).set_content(GetRegisterResponse());

    case 3:
      PushResponse(net::HTTP_OK).set_content(kOAuth2AccessTokenData);

    case 2:
      PushResponse(net::HTTP_OK).set_content(kOAuth2TokenPairData);

    case 1:
      PushResponse(net::HTTP_OK)
          .AddCustomHeader("Set-Cookies", kOAuthTokenCookie);
      break;

    case 0:
      break;
  }

  // Login now. This verifies that logging in with the canned responses (which
  // may include failures) won't be blocked due to the potential failures.
  EXPECT_FALSE(profile_added_);
  Login(GetParam().username);
  EXPECT_TRUE(profile_added_);
  ASSERT_TRUE(user_manager->IsUserLoggedIn());
  EXPECT_TRUE(user_manager->IsCurrentUserNew());
}

const BlockingLoginTestParam kBlockinLoginTestCases[] = {
    { 0, kUsername, true },
    { 1, kUsername, true },
    { 2, kUsername, true },
    { 3, kUsername, true },
    { 4, kUsername, true },
    { 5, kUsername, true },
    { 0, kUsername, false },
    { 1, kUsername, false },
    { 2, kUsername, false },
    { 3, kUsername, false },
    { 4, kUsername, false },
    { 5, kUsername, false },
    { 0, kUsernameOtherDomain, true },
    { 1, kUsernameOtherDomain, true },
    { 2, kUsernameOtherDomain, true },
    { 3, kUsernameOtherDomain, true },
    { 4, kUsernameOtherDomain, true },
    { 5, kUsernameOtherDomain, true },
};

INSTANTIATE_TEST_CASE_P(BlockingLoginTestInstance,
                        BlockingLoginTest,
                        testing::ValuesIn(kBlockinLoginTestCases));

}  // namespace chromeos
