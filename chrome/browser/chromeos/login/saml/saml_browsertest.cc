// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/test/https_forwarder.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/ui/webui_login_display.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/signin/inline_login_ui.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/user_manager/user.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/base/url_util.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "policy/policy_constants.h"
#include "policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace em = enterprise_management;

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using testing::_;
using testing::Return;

namespace chromeos {

namespace {

const char kGAIASIDCookieName[] = "SID";
const char kGAIALSIDCookieName[] = "LSID";

const char kTestAuthSIDCookie1[] = "fake-auth-SID-cookie-1";
const char kTestAuthSIDCookie2[] = "fake-auth-SID-cookie-2";
const char kTestAuthLSIDCookie1[] = "fake-auth-LSID-cookie-1";
const char kTestAuthLSIDCookie2[] = "fake-auth-LSID-cookie-2";
const char kTestAuthCode[] = "fake-auth-code";
const char kTestGaiaUberToken[] = "fake-uber-token";
const char kTestAuthLoginAccessToken[] = "fake-access-token";
const char kTestRefreshToken[] = "fake-refresh-token";
const char kTestSessionSIDCookie[] = "fake-session-SID-cookie";
const char kTestSessionLSIDCookie[] = "fake-session-LSID-cookie";

const char kFirstSAMLUserEmail[] = "bob@example.com";
const char kSecondSAMLUserEmail[] = "alice@example.com";
const char kHTTPSAMLUserEmail[] = "carol@example.com";
const char kNonSAMLUserEmail[] = "dan@example.com";
const char kDifferentDomainSAMLUserEmail[] = "eve@example.test";

const char kSAMLIdPCookieName[] = "saml";
const char kSAMLIdPCookieValue1[] = "value-1";
const char kSAMLIdPCookieValue2[] = "value-2";

const char kRelayState[] = "RelayState";

// FakeSamlIdp serves IdP auth form and the form submission. The form is
// served with the template's RelayState placeholder expanded to the real
// RelayState parameter from request. The form submission redirects back to
// FakeGaia with the same RelayState.
class FakeSamlIdp {
 public:
  FakeSamlIdp();
  ~FakeSamlIdp();

  void SetUp(const std::string& base_path, const GURL& gaia_url);

  void SetLoginHTMLTemplate(const std::string& template_file);
  void SetLoginAuthHTMLTemplate(const std::string& template_file);
  void SetRefreshURL(const GURL& refresh_url);
  void SetCookieValue(const std::string& cookie_value);

  scoped_ptr<HttpResponse> HandleRequest(const HttpRequest& request);

 private:
  scoped_ptr<HttpResponse> BuildHTMLResponse(const std::string& html_template,
                                             const std::string& relay_state,
                                             const std::string& next_path);

  base::FilePath html_template_dir_;

  std::string login_path_;
  std::string login_auth_path_;

  std::string login_html_template_;
  std::string login_auth_html_template_;
  GURL gaia_assertion_url_;
  GURL refresh_url_;
  std::string cookie_value_;

  DISALLOW_COPY_AND_ASSIGN(FakeSamlIdp);
};

FakeSamlIdp::FakeSamlIdp() {
}

FakeSamlIdp::~FakeSamlIdp() {
}

void FakeSamlIdp::SetUp(const std::string& base_path, const GURL& gaia_url) {
  base::FilePath test_data_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  html_template_dir_ = test_data_dir.Append("login");

  login_path_= base_path;
  login_auth_path_ = base_path + "Auth";
  gaia_assertion_url_ = gaia_url.Resolve("/SSO");
}

void FakeSamlIdp::SetLoginHTMLTemplate(const std::string& template_file) {
  EXPECT_TRUE(base::ReadFileToString(
      html_template_dir_.Append(template_file),
      &login_html_template_));
}

void FakeSamlIdp::SetLoginAuthHTMLTemplate(const std::string& template_file) {
  EXPECT_TRUE(base::ReadFileToString(
      html_template_dir_.Append(template_file),
      &login_auth_html_template_));
}

void FakeSamlIdp::SetRefreshURL(const GURL& refresh_url) {
  refresh_url_ = refresh_url;
}

void FakeSamlIdp::SetCookieValue(const std::string& cookie_value) {
  cookie_value_ = cookie_value;
}

scoped_ptr<HttpResponse> FakeSamlIdp::HandleRequest(
    const HttpRequest& request) {
  // The scheme and host of the URL is actually not important but required to
  // get a valid GURL in order to parse |request.relative_url|.
  GURL request_url = GURL("http://localhost").Resolve(request.relative_url);
  std::string request_path = request_url.path();

  if (request_path == login_path_) {
    std::string relay_state;
    net::GetValueForKeyInQuery(request_url, kRelayState, &relay_state);
    return BuildHTMLResponse(login_html_template_,
                             relay_state,
                             login_auth_path_);
  }

  if (request_path != login_auth_path_) {
    // Request not understood.
    return scoped_ptr<HttpResponse>();
  }

  std::string relay_state;
  FakeGaia::GetQueryParameter(request.content, kRelayState, &relay_state);
  GURL redirect_url = gaia_assertion_url_;

  if (!login_auth_html_template_.empty()) {
    return BuildHTMLResponse(login_auth_html_template_,
                             relay_state,
                             redirect_url.spec());
  }

  redirect_url = net::AppendQueryParameter(
      redirect_url, "SAMLResponse", "fake_response");
  redirect_url = net::AppendQueryParameter(
      redirect_url, kRelayState, relay_state);

  scoped_ptr<BasicHttpResponse> http_response(new BasicHttpResponse());
  http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
  http_response->AddCustomHeader("Location", redirect_url.spec());
  http_response->AddCustomHeader(
      "Set-cookie",
      base::StringPrintf("saml=%s", cookie_value_.c_str()));
  return http_response.PassAs<HttpResponse>();
}

scoped_ptr<HttpResponse> FakeSamlIdp::BuildHTMLResponse(
    const std::string& html_template,
    const std::string& relay_state,
    const std::string& next_path) {
  std::string response_html = html_template;
  ReplaceSubstringsAfterOffset(&response_html, 0, "$RelayState", relay_state);
  ReplaceSubstringsAfterOffset(&response_html, 0, "$Post", next_path);
  ReplaceSubstringsAfterOffset(
      &response_html, 0, "$Refresh", refresh_url_.spec());

  scoped_ptr<BasicHttpResponse> http_response(new BasicHttpResponse());
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(response_html);
  http_response->set_content_type("text/html");

  return http_response.PassAs<HttpResponse>();
}

}  // namespace

class SamlTest : public InProcessBrowserTest {
 public:
  SamlTest() : saml_load_injected_(false) {}
  virtual ~SamlTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

    // Start the GAIA https wrapper here so that the GAIA URLs can be pointed at
    // it in SetUpCommandLine().
    gaia_https_forwarder_.reset(
        new HTTPSForwarder(embedded_test_server()->base_url()));
    ASSERT_TRUE(gaia_https_forwarder_->Start());

    // Start the SAML IdP https wrapper here so that GAIA can be pointed at it
    // in SetUpCommandLine().
    saml_https_forwarder_.reset(
        new HTTPSForwarder(embedded_test_server()->base_url()));
    ASSERT_TRUE(saml_https_forwarder_->Start());

    // Stop IO thread here because no threads are allowed while
    // spawning sandbox host process. See crbug.com/322732.
    embedded_test_server()->StopThread();

    InProcessBrowserTest::SetUp();
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(switches::kForceLoginManagerInTests);
    command_line->AppendSwitch(::switches::kDisableBackgroundNetworking);
    command_line->AppendSwitchASCII(switches::kLoginProfile, "user");

    const GURL gaia_url = gaia_https_forwarder_->GetURL("");
    command_line->AppendSwitchASCII(::switches::kGaiaUrl, gaia_url.spec());
    command_line->AppendSwitchASCII(::switches::kLsoUrl, gaia_url.spec());
    command_line->AppendSwitchASCII(::switches::kGoogleApisUrl,
                                    gaia_url.spec());

    const GURL saml_idp_url = saml_https_forwarder_->GetURL("SAML");
    fake_saml_idp_.SetUp(saml_idp_url.path(), gaia_url);
    fake_gaia_.RegisterSamlUser(kFirstSAMLUserEmail, saml_idp_url);
    fake_gaia_.RegisterSamlUser(kSecondSAMLUserEmail, saml_idp_url);
    fake_gaia_.RegisterSamlUser(
        kHTTPSAMLUserEmail,
        embedded_test_server()->base_url().Resolve("/SAML"));
    fake_gaia_.RegisterSamlUser(kDifferentDomainSAMLUserEmail, saml_idp_url);

    fake_gaia_.Initialize();
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    SetMergeSessionParams(kFirstSAMLUserEmail,
                          kTestAuthSIDCookie1,
                          kTestAuthLSIDCookie1);

    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&FakeGaia::HandleRequest, base::Unretained(&fake_gaia_)));
    embedded_test_server()->RegisterRequestHandler(base::Bind(
        &FakeSamlIdp::HandleRequest, base::Unretained(&fake_saml_idp_)));

    // Restart the thread as the sandbox host process has already been spawned.
    embedded_test_server()->RestartThreadAndListen();

    login_screen_load_observer_.reset(new content::WindowedNotificationObserver(
        chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
        content::NotificationService::AllSources()));
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    // If the login display is still showing, exit gracefully.
    if (LoginDisplayHostImpl::default_host()) {
      base::MessageLoop::current()->PostTask(FROM_HERE,
                                             base::Bind(&chrome::AttemptExit));
      content::RunMessageLoop();
    }
  }

  void SetMergeSessionParams(const std::string& email,
                             const std::string& auth_sid_cookie,
                             const std::string& auth_lsid_cookie) {
    FakeGaia::MergeSessionParams params;
    params.auth_sid_cookie = auth_sid_cookie;
    params.auth_lsid_cookie = auth_lsid_cookie;
    params.auth_code = kTestAuthCode;
    params.refresh_token = kTestRefreshToken;
    params.access_token = kTestAuthLoginAccessToken;
    params.gaia_uber_token = kTestGaiaUberToken;
    params.session_sid_cookie = kTestSessionSIDCookie;
    params.session_lsid_cookie = kTestSessionLSIDCookie;
    params.email = email;
    fake_gaia_.SetMergeSessionParams(params);
  }

  WebUILoginDisplay* GetLoginDisplay() {
    ExistingUserController* controller =
        ExistingUserController::current_controller();
    CHECK(controller);
    return static_cast<WebUILoginDisplay*>(controller->login_display());
  }

  void WaitForSigninScreen() {
    WizardController* wizard_controller =
        WizardController::default_controller();
    if (wizard_controller) {
      WizardController::SkipPostLoginScreensForTesting();
      wizard_controller->SkipToLoginForTesting(LoginScreenContext());
    }

    login_screen_load_observer_->Wait();
  }

  void StartSamlAndWaitForIdpPageLoad(const std::string& gaia_email) {
    WaitForSigninScreen();

    if (!saml_load_injected_) {
      saml_load_injected_ = true;

      ASSERT_TRUE(content::ExecuteScript(
          GetLoginUI()->GetWebContents(),
          "$('gaia-signin').gaiaAuthHost_.addEventListener('authFlowChange',"
              "function() {"
                "window.domAutomationController.setAutomationId(0);"
                "window.domAutomationController.send("
                    "$('gaia-signin').isSAML() ? 'SamlLoaded' : 'GaiaLoaded');"
              "});"));
    }

    content::DOMMessageQueue message_queue;  // Start observe before SAML.
    GetLoginDisplay()->ShowSigninScreenForCreds(gaia_email, "");

    std::string message;
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
    EXPECT_EQ("\"SamlLoaded\"", message);
  }

  void SetSignFormField(const std::string& field_id,
                        const std::string& field_value) {
    std::string js =
        "(function(){"
          "document.getElementById('$FieldId').value = '$FieldValue';"
          "var e = new Event('input');"
          "document.getElementById('$FieldId').dispatchEvent(e);"
        "})();";
    ReplaceSubstringsAfterOffset(&js, 0, "$FieldId", field_id);
    ReplaceSubstringsAfterOffset(&js, 0, "$FieldValue", field_value);
    ExecuteJsInSigninFrame(js);
  }

  void SendConfirmPassword(const std::string& password_to_confirm) {
    std::string js =
        "$('confirm-password-input').value='$Password';"
        "$('confirm-password').onConfirmPassword_();";
    ReplaceSubstringsAfterOffset(&js, 0, "$Password", password_to_confirm);
    ASSERT_TRUE(content::ExecuteScript(GetLoginUI()->GetWebContents(), js));
  }

  void JsExpect(const std::string& js) {
    bool result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        GetLoginUI()->GetWebContents(),
        "window.domAutomationController.send(!!(" + js + "));",
        &result));
    EXPECT_TRUE(result) << js;
  }

  std::string WaitForAndGetFatalErrorMessage() {
    OobeScreenWaiter(OobeDisplay::SCREEN_FATAL_ERROR).Wait();
    std::string error_message;
    if (!content::ExecuteScriptAndExtractString(
          GetLoginUI()->GetWebContents(),
          "window.domAutomationController.send("
              "$('fatal-error-message').textContent);",
          &error_message)) {
      ADD_FAILURE();
    }
    return error_message;
  }

  content::WebUI* GetLoginUI() {
    return static_cast<LoginDisplayHostImpl*>(
        LoginDisplayHostImpl::default_host())->GetOobeUI()->web_ui();
  }

  // Executes JavaScript code in the auth iframe hosted by gaia_auth extension.
  void ExecuteJsInSigninFrame(const std::string& js) {
    content::RenderFrameHost* frame = InlineLoginUI::GetAuthIframe(
        GetLoginUI()->GetWebContents(), GURL(), "signin-frame");
    ASSERT_TRUE(content::ExecuteScript(frame, js));
  }

  FakeSamlIdp* fake_saml_idp() { return &fake_saml_idp_; }

 protected:
  scoped_ptr<content::WindowedNotificationObserver> login_screen_load_observer_;

 private:
  FakeGaia fake_gaia_;
  FakeSamlIdp fake_saml_idp_;
  scoped_ptr<HTTPSForwarder> gaia_https_forwarder_;
  scoped_ptr<HTTPSForwarder> saml_https_forwarder_;

  bool saml_load_injected_;

  DISALLOW_COPY_AND_ASSIGN(SamlTest);
};

// Tests that signin frame should have 'saml' class and 'cancel' button is
// visible when SAML IdP page is loaded. And 'cancel' button goes back to
// gaia on clicking.
IN_PROC_BROWSER_TEST_F(SamlTest, SamlUI) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  StartSamlAndWaitForIdpPageLoad(kFirstSAMLUserEmail);

  // Saml flow UI expectations.
  JsExpect("$('gaia-signin').classList.contains('saml')");
  JsExpect("!$('cancel-add-user-button').hidden");

  // Click on 'cancel'.
  content::DOMMessageQueue message_queue;  // Observe before 'cancel'.
  ASSERT_TRUE(content::ExecuteScript(
      GetLoginUI()->GetWebContents(),
      "$('cancel-add-user-button').click();"));

  // Auth flow should change back to Gaia.
  std::string message;
  do {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
  } while (message != "\"GaiaLoaded\"");

  // Saml flow is gone.
  JsExpect("!$('gaia-signin').classList.contains('saml')");
}

// Tests the sign-in flow when the credentials passing API is used.
IN_PROC_BROWSER_TEST_F(SamlTest, CredentialPassingAPI) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_api_login.html");
  fake_saml_idp()->SetLoginAuthHTMLTemplate("saml_api_login_auth.html");
  StartSamlAndWaitForIdpPageLoad(kFirstSAMLUserEmail);

  // Fill-in the SAML IdP form and submit.
  SetSignFormField("Email", "fake_user");
  SetSignFormField("Password", "fake_password");
  ExecuteJsInSigninFrame("document.getElementById('Submit').click();");

  // Login should finish login and a session should start.
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources()).Wait();
}

// Tests the single password scraped flow.
IN_PROC_BROWSER_TEST_F(SamlTest, ScrapedSingle) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  StartSamlAndWaitForIdpPageLoad(kFirstSAMLUserEmail);

  // Fill-in the SAML IdP form and submit.
  SetSignFormField("Email", "fake_user");
  SetSignFormField("Password", "fake_password");
  ExecuteJsInSigninFrame("document.getElementById('Submit').click();");

  // Lands on confirm password screen.
  OobeScreenWaiter(OobeDisplay::SCREEN_CONFIRM_PASSWORD).Wait();

  // Enter an unknown password should go back to confirm password screen.
  SendConfirmPassword("wrong_password");
  OobeScreenWaiter(OobeDisplay::SCREEN_CONFIRM_PASSWORD).Wait();

  // Enter a known password should finish login and start session.
  SendConfirmPassword("fake_password");
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources()).Wait();
}

// Tests the multiple password scraped flow.
IN_PROC_BROWSER_TEST_F(SamlTest, ScrapedMultiple) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_two_passwords.html");

  StartSamlAndWaitForIdpPageLoad(kFirstSAMLUserEmail);

  SetSignFormField("Email", "fake_user");
  SetSignFormField("Password", "fake_password");
  SetSignFormField("Password1", "password1");
  ExecuteJsInSigninFrame("document.getElementById('Submit').click();");

  OobeScreenWaiter(OobeDisplay::SCREEN_CONFIRM_PASSWORD).Wait();

  // Either scraped password should be able to sign-in.
  SendConfirmPassword("password1");
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources()).Wait();
}

// Tests the no password scraped flow.
IN_PROC_BROWSER_TEST_F(SamlTest, ScrapedNone) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_no_passwords.html");

  StartSamlAndWaitForIdpPageLoad(kFirstSAMLUserEmail);

  SetSignFormField("Email", "fake_user");
  ExecuteJsInSigninFrame("document.getElementById('Submit').click();");

  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_LOGIN_FATAL_ERROR_NO_PASSWORD),
            WaitForAndGetFatalErrorMessage());
}

// Types |bob@example.com| into the GAIA login form but then authenticates as
// |alice@example.com| via SAML. Verifies that the logged-in user is correctly
// identified as Alice.
IN_PROC_BROWSER_TEST_F(SamlTest, UseAutenticatedUserEmailAddress) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  // Type |bob@example.com| into the GAIA login form.
  StartSamlAndWaitForIdpPageLoad(kSecondSAMLUserEmail);

  // Authenticate as alice@example.com via SAML (the |Email| provided here is
  // irrelevant - the authenticated user's e-mail address that FakeGAIA
  // reports was set via SetMergeSessionParams()).
  SetSignFormField("Email", "fake_user");
  SetSignFormField("Password", "fake_password");
  ExecuteJsInSigninFrame("document.getElementById('Submit').click();");

  OobeScreenWaiter(OobeDisplay::SCREEN_CONFIRM_PASSWORD).Wait();

  SendConfirmPassword("fake_password");
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources()).Wait();
  const user_manager::User* user = UserManager::Get()->GetActiveUser();
  ASSERT_TRUE(user);
  EXPECT_EQ(kFirstSAMLUserEmail, user->email());
}

// Verifies that if the authenticated user's e-mail address cannot be retrieved,
// an error message is shown.
IN_PROC_BROWSER_TEST_F(SamlTest, FailToRetrieveAutenticatedUserEmailAddress) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  StartSamlAndWaitForIdpPageLoad(kFirstSAMLUserEmail);

  SetMergeSessionParams("", kTestAuthSIDCookie1, kTestAuthLSIDCookie1);
  SetSignFormField("Email", "fake_user");
  SetSignFormField("Password", "fake_password");
  ExecuteJsInSigninFrame("document.getElementById('Submit').click();");

  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_LOGIN_FATAL_ERROR_NO_EMAIL),
            WaitForAndGetFatalErrorMessage());
}

// Tests the password confirm flow: show error on the first failure and
// fatal error on the second failure.
IN_PROC_BROWSER_TEST_F(SamlTest, PasswordConfirmFlow) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  StartSamlAndWaitForIdpPageLoad(kFirstSAMLUserEmail);

  // Fill-in the SAML IdP form and submit.
  SetSignFormField("Email", "fake_user");
  SetSignFormField("Password", "fake_password");
  ExecuteJsInSigninFrame("document.getElementById('Submit').click();");

  // Lands on confirm password screen with no error message.
  OobeScreenWaiter(OobeDisplay::SCREEN_CONFIRM_PASSWORD).Wait();
  JsExpect("!$('confirm-password').classList.contains('error')");

  // Enter an unknown password for the first time should go back to confirm
  // password screen with error message.
  SendConfirmPassword("wrong_password");
  OobeScreenWaiter(OobeDisplay::SCREEN_CONFIRM_PASSWORD).Wait();
  JsExpect("$('confirm-password').classList.contains('error')");

  // Enter an unknown password 2nd time should go back fatal error message.
  SendConfirmPassword("wrong_password");
  EXPECT_EQ(
      l10n_util::GetStringUTF8(IDS_LOGIN_FATAL_ERROR_PASSWORD_VERIFICATION),
      WaitForAndGetFatalErrorMessage());
}

// Verifies that when GAIA attempts to redirect to a SAML IdP served over http,
// not https, the redirect is blocked and an error message is shown.
IN_PROC_BROWSER_TEST_F(SamlTest, HTTPRedirectDisallowed) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");

  WaitForSigninScreen();
  GetLoginDisplay()->ShowSigninScreenForCreds(kHTTPSAMLUserEmail, "");

  const GURL url = embedded_test_server()->base_url().Resolve("/SAML");
  EXPECT_EQ(l10n_util::GetStringFUTF8(
                IDS_LOGIN_FATAL_ERROR_TEXT_INSECURE_URL,
                base::UTF8ToUTF16(url.spec())),
            WaitForAndGetFatalErrorMessage());
}

// Verifies that when GAIA attempts to redirect to a page served over http, not
// https, via an HTML meta refresh, the redirect is blocked and an error message
// is shown. This guards against regressions of http://crbug.com/359515.
IN_PROC_BROWSER_TEST_F(SamlTest, MetaRefreshToHTTPDisallowed) {
  const GURL url = embedded_test_server()->base_url().Resolve("/SSO");
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_instant_meta_refresh.html");
  fake_saml_idp()->SetRefreshURL(url);

  WaitForSigninScreen();
  GetLoginDisplay()->ShowSigninScreenForCreds(kFirstSAMLUserEmail, "");

  EXPECT_EQ(l10n_util::GetStringFUTF8(
                IDS_LOGIN_FATAL_ERROR_TEXT_INSECURE_URL,
                base::UTF8ToUTF16(url.spec())),
            WaitForAndGetFatalErrorMessage());
}

class SAMLPolicyTest : public SamlTest {
 public:
  SAMLPolicyTest();
  virtual ~SAMLPolicyTest();

  // SamlTest:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE;
  virtual void SetUpOnMainThread() OVERRIDE;

  void SetSAMLOfflineSigninTimeLimitPolicy(int limit);
  void EnableTransferSAMLCookiesPolicy();

  void ShowGAIALoginForm();
  void LogInWithSAML(const std::string& user_id,
                     const std::string& auth_sid_cookie,
                     const std::string& auth_lsid_cookie);

  std::string GetCookieValue(const std::string& name);

  void GetCookies();

 protected:
  void GetCookiesOnIOThread(
      const scoped_refptr<net::URLRequestContextGetter>& request_context,
      const base::Closure& callback);
  void StoreCookieList(const base::Closure& callback,
                       const net::CookieList& cookie_list);

  policy::DevicePolicyCrosTestHelper test_helper_;

  // FakeDBusThreadManager uses FakeSessionManagerClient.
  FakeDBusThreadManager* fake_dbus_thread_manager_;
  FakeSessionManagerClient* fake_session_manager_client_;
  policy::DevicePolicyBuilder* device_policy_;

  policy::MockConfigurationPolicyProvider provider_;

  net::CookieList cookie_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SAMLPolicyTest);
};

SAMLPolicyTest::SAMLPolicyTest()
    : fake_dbus_thread_manager_(new FakeDBusThreadManager),
      fake_session_manager_client_(new FakeSessionManagerClient),
      device_policy_(test_helper_.device_policy()) {
  fake_dbus_thread_manager_->SetFakeClients();
  fake_dbus_thread_manager_->SetSessionManagerClient(
      scoped_ptr<SessionManagerClient>(fake_session_manager_client_));
}

SAMLPolicyTest::~SAMLPolicyTest() {
}

void SAMLPolicyTest::SetUpInProcessBrowserTestFixture() {
  DBusThreadManager::SetInstanceForTesting(fake_dbus_thread_manager_);
  SamlTest::SetUpInProcessBrowserTestFixture();

  // Initialize device policy.
  test_helper_.InstallOwnerKey();
  test_helper_.MarkAsEnterpriseOwned();
  device_policy_->SetDefaultSigningKey();
  device_policy_->Build();
  fake_session_manager_client_->set_device_policy(device_policy_->GetBlob());
  fake_session_manager_client_->OnPropertyChangeComplete(true);

  // Initialize user policy.
  EXPECT_CALL(provider_, IsInitializationComplete(_))
      .WillRepeatedly(Return(true));
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
}

void SAMLPolicyTest::SetUpOnMainThread() {
  SamlTest::SetUpOnMainThread();

  // Pretend that the test users' OAuth tokens are valid.
  UserManager::Get()->SaveUserOAuthStatus(
      kFirstSAMLUserEmail, user_manager::User::OAUTH2_TOKEN_STATUS_VALID);
  UserManager::Get()->SaveUserOAuthStatus(
      kNonSAMLUserEmail, user_manager::User::OAUTH2_TOKEN_STATUS_VALID);
  UserManager::Get()->SaveUserOAuthStatus(
      kDifferentDomainSAMLUserEmail,
      user_manager::User::OAUTH2_TOKEN_STATUS_VALID);
}

void SAMLPolicyTest::SetSAMLOfflineSigninTimeLimitPolicy(int limit) {
  policy::PolicyMap user_policy;
  user_policy.Set(policy::key::kSAMLOfflineSigninTimeLimit,
                  policy::POLICY_LEVEL_MANDATORY,
                  policy::POLICY_SCOPE_USER,
                  new base::FundamentalValue(limit),
                  NULL);
  provider_.UpdateChromePolicy(user_policy);
  base::RunLoop().RunUntilIdle();
}

void SAMLPolicyTest::EnableTransferSAMLCookiesPolicy() {
  em::ChromeDeviceSettingsProto& proto(device_policy_->payload());
  proto.mutable_saml_settings()->set_transfer_saml_cookies(true);

  base::RunLoop run_loop;
  scoped_ptr<CrosSettings::ObserverSubscription> observer =
      CrosSettings::Get()->AddSettingsObserver(
           kAccountsPrefTransferSAMLCookies,
           run_loop.QuitClosure());
  device_policy_->SetDefaultSigningKey();
  device_policy_->Build();
  fake_session_manager_client_->set_device_policy(device_policy_->GetBlob());
  fake_session_manager_client_->OnPropertyChangeComplete(true);
  run_loop.Run();
}

void SAMLPolicyTest::ShowGAIALoginForm() {
  login_screen_load_observer_->Wait();
  ASSERT_TRUE(content::ExecuteScript(
      GetLoginUI()->GetWebContents(),
      "$('gaia-signin').gaiaAuthHost_.addEventListener('ready', function() {"
      "  window.domAutomationController.setAutomationId(0);"
      "  window.domAutomationController.send('ready');"
      "});"
      "$('add-user-button').click();"));
  content::DOMMessageQueue message_queue;
  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"ready\"", message);
}

void SAMLPolicyTest::LogInWithSAML(const std::string& user_id,
                                   const std::string& auth_sid_cookie,
                                   const std::string& auth_lsid_cookie) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  StartSamlAndWaitForIdpPageLoad(user_id);

  SetMergeSessionParams(user_id, auth_sid_cookie, auth_lsid_cookie);
  SetSignFormField("Email", "fake_user");
  SetSignFormField("Password", "fake_password");
  ExecuteJsInSigninFrame("document.getElementById('Submit').click();");

  OobeScreenWaiter(OobeDisplay::SCREEN_CONFIRM_PASSWORD).Wait();

  SendConfirmPassword("fake_password");
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources()).Wait();
}

std::string SAMLPolicyTest::GetCookieValue(const std::string& name) {
  for (net::CookieList::const_iterator it = cookie_list_.begin();
       it != cookie_list_.end(); ++it) {
    if (it->Name() == name)
      return it->Value();
  }
  return std::string();
}

void SAMLPolicyTest::GetCookies() {
  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUserUnsafe(
      UserManager::Get()->GetActiveUser());
  ASSERT_TRUE(profile);
  base::RunLoop run_loop;
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&SAMLPolicyTest::GetCookiesOnIOThread,
                 base::Unretained(this),
                 scoped_refptr<net::URLRequestContextGetter>(
                     profile->GetRequestContext()),
                 run_loop.QuitClosure()));
  run_loop.Run();
}

void SAMLPolicyTest::GetCookiesOnIOThread(
    const scoped_refptr<net::URLRequestContextGetter>& request_context,
    const base::Closure& callback) {
  request_context->GetURLRequestContext()->cookie_store()->
      GetCookieMonster()->GetAllCookiesAsync(base::Bind(
          &SAMLPolicyTest::StoreCookieList,
          base::Unretained(this),
          callback));
}

void SAMLPolicyTest::StoreCookieList(
    const base::Closure& callback,
    const net::CookieList& cookie_list) {
  cookie_list_ = cookie_list;
  content::BrowserThread::PostTask(content::BrowserThread::UI,
                                   FROM_HERE,
                                   callback);
}

IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, PRE_NoSAML) {
  // Set the offline login time limit for SAML users to zero.
  SetSAMLOfflineSigninTimeLimitPolicy(0);

  WaitForSigninScreen();

  // Log in without SAML.
  GetLoginDisplay()->ShowSigninScreenForCreds(kNonSAMLUserEmail, "password");

  content::WindowedNotificationObserver(
    chrome::NOTIFICATION_SESSION_STARTED,
    content::NotificationService::AllSources()).Wait();
}

// Verifies that the offline login time limit does not affect a user who
// authenticated without SAML.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, NoSAML) {
  login_screen_load_observer_->Wait();
  // Verify that offline login is allowed.
  JsExpect("window.getComputedStyle(document.querySelector("
           "    '#pod-row .signin-button-container')).display == 'none'");
}

IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, PRE_SAMLNoLimit) {
  // Remove the offline login time limit for SAML users.
  SetSAMLOfflineSigninTimeLimitPolicy(-1);

  LogInWithSAML(kFirstSAMLUserEmail, kTestAuthSIDCookie1, kTestAuthLSIDCookie1);
}

// Verifies that when no offline login time limit is set, a user who
// authenticated with SAML is allowed to log in offline.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, SAMLNoLimit) {
  login_screen_load_observer_->Wait();
  // Verify that offline login is allowed.
  JsExpect("window.getComputedStyle(document.querySelector("
           "    '#pod-row .signin-button-container')).display == 'none'");
}

IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, PRE_SAMLZeroLimit) {
  // Set the offline login time limit for SAML users to zero.
  SetSAMLOfflineSigninTimeLimitPolicy(0);

  LogInWithSAML(kFirstSAMLUserEmail, kTestAuthSIDCookie1, kTestAuthLSIDCookie1);
}

// Verifies that when the offline login time limit is exceeded for a user who
// authenticated via SAML, that user is forced to log in online the next time.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, SAMLZeroLimit) {
  login_screen_load_observer_->Wait();
  // Verify that offline login is not allowed.
  JsExpect("window.getComputedStyle(document.querySelector("
           "    '#pod-row .signin-button-container')).display != 'none'");
}

IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, PRE_PRE_TransferCookiesAffiliated) {
  fake_saml_idp()->SetCookieValue(kSAMLIdPCookieValue1);
  LogInWithSAML(kFirstSAMLUserEmail, kTestAuthSIDCookie1, kTestAuthLSIDCookie1);

  GetCookies();
  EXPECT_EQ(kTestAuthSIDCookie1, GetCookieValue(kGAIASIDCookieName));
  EXPECT_EQ(kTestAuthLSIDCookie1, GetCookieValue(kGAIALSIDCookieName));
  EXPECT_EQ(kSAMLIdPCookieValue1, GetCookieValue(kSAMLIdPCookieName));
}

// Verifies that when the DeviceTransferSAMLCookies policy is not enabled, SAML
// IdP cookies are not transferred to a user's profile on subsequent login, even
// if the user belongs to the domain that the device is enrolled into. Also
// verifies that GAIA cookies are not transferred.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, PRE_TransferCookiesAffiliated) {
  fake_saml_idp()->SetCookieValue(kSAMLIdPCookieValue2);
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  ShowGAIALoginForm();
  LogInWithSAML(kFirstSAMLUserEmail, kTestAuthSIDCookie2, kTestAuthLSIDCookie2);

  GetCookies();
  EXPECT_EQ(kTestAuthSIDCookie1, GetCookieValue(kGAIASIDCookieName));
  EXPECT_EQ(kTestAuthLSIDCookie1, GetCookieValue(kGAIALSIDCookieName));
  EXPECT_EQ(kSAMLIdPCookieValue1, GetCookieValue(kSAMLIdPCookieName));
}

// Verifies that when the DeviceTransferSAMLCookies policy is enabled, SAML IdP
// cookies are transferred to a user's profile on subsequent login when the user
// belongs to the domain that the device is enrolled into. Also verifies that
// GAIA cookies are not transferred.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, TransferCookiesAffiliated) {
  fake_saml_idp()->SetCookieValue(kSAMLIdPCookieValue2);
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  ShowGAIALoginForm();

  EnableTransferSAMLCookiesPolicy();
  LogInWithSAML(kFirstSAMLUserEmail, kTestAuthSIDCookie2, kTestAuthLSIDCookie2);

  GetCookies();
  EXPECT_EQ(kTestAuthSIDCookie1, GetCookieValue(kGAIASIDCookieName));
  EXPECT_EQ(kTestAuthLSIDCookie1, GetCookieValue(kGAIALSIDCookieName));
  EXPECT_EQ(kSAMLIdPCookieValue2, GetCookieValue(kSAMLIdPCookieName));
}

IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, PRE_TransferCookiesUnaffiliated) {
  fake_saml_idp()->SetCookieValue(kSAMLIdPCookieValue1);
  LogInWithSAML(kDifferentDomainSAMLUserEmail,
                kTestAuthSIDCookie1,
                kTestAuthLSIDCookie1);

  GetCookies();
  EXPECT_EQ(kTestAuthSIDCookie1, GetCookieValue(kGAIASIDCookieName));
  EXPECT_EQ(kTestAuthLSIDCookie1, GetCookieValue(kGAIALSIDCookieName));
  EXPECT_EQ(kSAMLIdPCookieValue1, GetCookieValue(kSAMLIdPCookieName));
}

// Verifies that even if the DeviceTransferSAMLCookies policy is enabled, SAML
// IdP are not transferred to a user's profile on subsequent login if the user
// does not belong to the domain that the device is enrolled into. Also verifies
// that GAIA cookies are not transferred.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, TransferCookiesUnaffiliated) {
  fake_saml_idp()->SetCookieValue(kSAMLIdPCookieValue2);
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  ShowGAIALoginForm();

  EnableTransferSAMLCookiesPolicy();
  LogInWithSAML(kDifferentDomainSAMLUserEmail,
                kTestAuthSIDCookie1,
                kTestAuthLSIDCookie1);

  GetCookies();
  EXPECT_EQ(kTestAuthSIDCookie1, GetCookieValue(kGAIASIDCookieName));
  EXPECT_EQ(kTestAuthLSIDCookie1, GetCookieValue(kGAIALSIDCookieName));
  EXPECT_EQ(kSAMLIdPCookieValue1, GetCookieValue(kSAMLIdPCookieName));
}

}  // namespace chromeos
