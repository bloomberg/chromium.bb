// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace chromeos {

namespace {

const char kTestAuthCode[] = "fake-auth-code";
const char kTestGaiaUberToken[] = "fake-uber-token";
const char kTestAuthLoginAccessToken[] = "fake-access-token";
const char kTestRefreshToken[] = "fake-refresh-token";
const char kTestSessionSIDCookie[] = "fake-session-SID-cookie";
const char kTestSessionLSIDCookie[] = "fake-session-LSID-cookie";

const char kRelayState[] = "RelayState";

const char kDefaultIdpHtml[] =
    "<form id=IdPForm method=post action=\"$Post\">"
      "<input type=hidden name=RelayState value=\"$RelayState\">"
      "User: <input type=text id=Email name=Email>"
      "Password: <input type=password id=Password name=Password>"
      "<input id=Submit type=submit>"
    "</form>";

// FakeSamlIdp serves IdP auth form and the form submission. The form is
// served with the template's RelayState placeholder expanded to the real
// RelayState parameter from request. The form submission redirects back to
// FakeGaia with the same RelayState.
class FakeSamlIdp {
 public:
  FakeSamlIdp() : html_template_(kDefaultIdpHtml) {}
  ~FakeSamlIdp() {}

  void SetUp(const std::string& base_path, const GURL& gaia_url) {
    base_path_= base_path;
    post_path_ = base_path + "Auth";
    gaia_assertion_url_ = gaia_url.Resolve("/SSO");
  }

  scoped_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    // The scheme and host of the URL is actually not important but required to
    // get a valid GURL in order to parse |request.relative_url|.
    GURL request_url = GURL("http://localhost").Resolve(request.relative_url);
    std::string request_path = request_url.path();

    scoped_ptr<BasicHttpResponse> http_response(new BasicHttpResponse());
    if (request_path == base_path_) {
      std::string relay_state;
      net::GetValueForKeyInQuery(request_url, kRelayState, &relay_state);

      std::string response_html = html_template_;
      ReplaceSubstringsAfterOffset(
          &response_html, 0, "$RelayState", relay_state);
      ReplaceSubstringsAfterOffset(
          &response_html, 0, "$Post", post_path_);

      http_response->set_code(net::HTTP_OK);
      http_response->set_content(response_html);
      http_response->set_content_type("text/html");
    } else if (request_path == post_path_) {
      std::string relay_state;
      FakeGaia::GetQueryParameter(request.content, kRelayState, &relay_state);

      GURL redirect_url = gaia_assertion_url_;
      redirect_url = net::AppendQueryParameter(
          redirect_url, "SAMLResponse", "fake_response");
      redirect_url = net::AppendQueryParameter(
          redirect_url, kRelayState, relay_state);

      http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
      http_response->AddCustomHeader("Location", redirect_url.spec());
    } else {
      // Request not understood.
      return scoped_ptr<HttpResponse>();
    }

    return http_response.PassAs<HttpResponse>();
  }

  void set_html_template(const std::string& html_template) {
    html_template_ = html_template;
  }

 private:
  std::string base_path_;
  std::string post_path_;
  std::string html_template_;

  GURL gaia_assertion_url_;
  DISALLOW_COPY_AND_ASSIGN(FakeSamlIdp);
};

}  // namespace

class SamlTest : public InProcessBrowserTest {
 public:
  SamlTest() : saml_load_injected_(false) {}
  virtual ~SamlTest() {}

  virtual void SetUp() OVERRIDE {
    // Start embedded test server here so that we can get server base url
    // and override Gaia urls in SetupCommandLine.
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

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
    command_line->AppendSwitch(switches::kEnableSamlSignin);

    const GURL& server_url = embedded_test_server()->base_url();

    std::string gaia_host("gaia");
    GURL::Replacements replace_gaia_host;
    replace_gaia_host.SetHostStr(gaia_host);
    gaia_url_ = server_url.ReplaceComponents(replace_gaia_host);

    command_line->AppendSwitchASCII(::switches::kGaiaUrl, gaia_url_.spec());
    command_line->AppendSwitchASCII(::switches::kLsoUrl, gaia_url_.spec());
    command_line->AppendSwitchASCII(::switches::kGoogleApisUrl,
                                    gaia_url_.spec());
    fake_gaia_.Initialize();

    std::string saml_idp_host("saml.idp");
    GURL::Replacements replace_saml_idp_host;
    replace_saml_idp_host.SetHostStr(saml_idp_host);
    GURL saml_idp_url = server_url.ReplaceComponents(replace_saml_idp_host);
    saml_idp_url = saml_idp_url.Resolve("/SAML/SSO");

    fake_saml_idp_.SetUp(saml_idp_url.path(), gaia_url_);
    fake_gaia_.RegisterSamlUser("saml_user", saml_idp_url);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    fake_gaia_.SetAuthTokens(kTestAuthCode,
                             kTestRefreshToken,
                             kTestAuthLoginAccessToken,
                             kTestGaiaUberToken,
                             kTestSessionSIDCookie,
                             kTestSessionLSIDCookie);

    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&FakeGaia::HandleRequest, base::Unretained(&fake_gaia_)));
    embedded_test_server()->RegisterRequestHandler(base::Bind(
        &FakeSamlIdp::HandleRequest, base::Unretained(&fake_saml_idp_)));

    // Restart the thread as the sandbox host process has already been spawned.
    embedded_test_server()->RestartThreadAndListen();
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    // If the login display is still showing, exit gracefully.
    if (LoginDisplayHostImpl::default_host()) {
      base::MessageLoop::current()->PostTask(FROM_HERE,
                                             base::Bind(&chrome::AttemptExit));
      content::RunMessageLoop();
    }
  }

  WebUILoginDisplay* GetLoginDisplay() {
    ExistingUserController* controller =
        ExistingUserController::current_controller();
    CHECK(controller);
    return static_cast<WebUILoginDisplay*>(controller->login_display());
  }

  void WaitForSigninScreen() {
    WizardController::SkipPostLoginScreensForTesting();
    WizardController* wizard_controller =
        chromeos::WizardController::default_controller();
    CHECK(wizard_controller);
    wizard_controller->SkipToLoginForTesting(LoginScreenContext());

    content::WindowedNotificationObserver(
      chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
      content::NotificationService::AllSources()).Wait();
  }

  void StartSamlAndWaitForIdpPageLoad() {
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
    GetLoginDisplay()->ShowSigninScreenForCreds("saml_user", "");

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

  content::WebUI* GetLoginUI() {
    return static_cast<chromeos::LoginDisplayHostImpl*>(
        chromeos::LoginDisplayHostImpl::default_host())->GetOobeUI()->web_ui();
  }

  // Executes Js code in the auth iframe hosted by gaia_auth extension.
  void ExecuteJsInSigninFrame(const std::string& js) {
    ASSERT_TRUE(content::ExecuteScriptInFrame(
        GetLoginUI()->GetWebContents(),
        "//iframe[@id='signin-frame']\n//iframe",
        js));
  }

  FakeSamlIdp* fake_saml_idp() { return &fake_saml_idp_; }

 private:
  GURL gaia_url_;
  FakeGaia fake_gaia_;
  FakeSamlIdp fake_saml_idp_;

  bool saml_load_injected_;

  DISALLOW_COPY_AND_ASSIGN(SamlTest);
};

// Tests that signin frame should have 'saml' class and 'cancel' button is
// visible when SAML IdP page is loaded. And 'cancel' button goes back to
// gaia on clicking.
IN_PROC_BROWSER_TEST_F(SamlTest, SamlUI) {
  StartSamlAndWaitForIdpPageLoad();

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

// Tests the single password scraped flow.
IN_PROC_BROWSER_TEST_F(SamlTest, ScrapedSingle) {
  StartSamlAndWaitForIdpPageLoad();

  // Fill-in the SAML IdP form and submit.
  SetSignFormField("Email", "fake_user");
  SetSignFormField("Password", "fake_password");
  ExecuteJsInSigninFrame("document.getElementById('IdPForm').submit();");

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
  fake_saml_idp()->set_html_template(
    "<form id=IdPForm method=post action=\"$Post\">"
      "<input type=hidden name=RelayState value=\"$RelayState\">"
      "User: <input type=text id=Email name=Email>"
      "Password: <input type=password id=Password name=Password>"
      "Password: <input type=password id=Password1 name=Password1>"
      "<input id=Submit type=submit>"
    "</form>");

  StartSamlAndWaitForIdpPageLoad();

  SetSignFormField("Email", "fake_user");
  SetSignFormField("Password", "fake_password");
  SetSignFormField("Password1", "password1");
  ExecuteJsInSigninFrame("document.getElementById('IdPForm').submit();");

  OobeScreenWaiter(OobeDisplay::SCREEN_CONFIRM_PASSWORD).Wait();

  // Either scraped password should be able to sign-in.
  SendConfirmPassword("password1");
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources()).Wait();
}

// Tests the no password scraped flow.
IN_PROC_BROWSER_TEST_F(SamlTest, ScrapedNone) {
  fake_saml_idp()->set_html_template(
    "<form id=IdPForm method=post action=\"$Post\">"
      "<input type=hidden name=RelayState value=\"$RelayState\">"
      "User: <input type=text id=Email name=Email>"
      "<input id=Submit type=submit>"
    "</form>");

  StartSamlAndWaitForIdpPageLoad();

  SetSignFormField("Email", "fake_user");
  ExecuteJsInSigninFrame("document.getElementById('IdPForm').submit();");

  OobeScreenWaiter(OobeDisplay::SCREEN_MESSAGE_BOX).Wait();
  JsExpect(
      "$('message-box-title').textContent == "
      "loadTimeData.getString('noPasswordWarningTitle')");
}

}  // namespace chromeos
