// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace {

const char kAuthorizationCode[] = "authorization_code";
const char kDiceResponseHeader[] = "X-Chrome-ID-Consistency-Response";
const char kEmail[] = "email@example.com";
const char kGaiaID[] = "gaia_id";
const char kOAuth2TokenExchangeURL[] = "/oauth2/v4/token";
const char kSigninURL[] = "/signin";

}  // namespace

namespace FakeGaia {

// Handler for the signin page on the embedded test server.
// The response has the content of the Dice request header in its body, and has
// the Dice response header.
std::unique_ptr<HttpResponse> HandleSigninURL(const HttpRequest& request) {
  if (!net::test_server::ShouldHandle(request, kSigninURL))
    return nullptr;

  std::string header_value = "None";
  auto it = request.headers.find(signin::kDiceRequestHeader);
  if (it != request.headers.end())
    header_value = it->second;

  std::unique_ptr<BasicHttpResponse> http_response(new BasicHttpResponse);
  http_response->set_content(header_value);
  http_response->set_content_type("text/plain");
  http_response->AddCustomHeader(
      kDiceResponseHeader,
      base::StringPrintf(
          "action=SIGNIN,authuser=1,id=%s,email=%s,authorization_code=%s",
          kGaiaID, kEmail, kAuthorizationCode));
  http_response->AddCustomHeader("Cache-Control", "no-store");
  return std::move(http_response);
}

// Handler for OAuth2 token exchange.
// Checks that the request is well formatted and returns a refresh token in a
// JSON dictionary.
std::unique_ptr<HttpResponse> HandleOAuth2TokenExchangeURL(
    bool* out_token_requested,
    const HttpRequest& request) {
  if (!net::test_server::ShouldHandle(request, kOAuth2TokenExchangeURL))
    return nullptr;

  // Check that the authorization code is somewhere in the request body.
  if (!request.has_content)
    return nullptr;
  if (request.content.find(kAuthorizationCode) == std::string::npos)
    return nullptr;

  // The token must be exchanged only once.
  EXPECT_FALSE(*out_token_requested);

  *out_token_requested = true;

  std::unique_ptr<BasicHttpResponse> http_response(new BasicHttpResponse);
  std::string content =
      "{"
      "  \"access_token\":\"access_token\","
      "  \"refresh_token\":\"refresh_token\","
      "  \"expires_in\":9999"
      "}";

  http_response->set_content(content);
  http_response->set_content_type("text/plain");
  http_response->AddCustomHeader("Cache-Control", "no-store");
  return std::move(http_response);
}

}  // namespace FakeGaia

class DiceBrowserTest : public InProcessBrowserTest,
                        public OAuth2TokenService::Observer {
 protected:
  ~DiceBrowserTest() override {}

  DiceBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        token_requested_(false),
        refresh_token_available_(false) {
    https_server_.RegisterDefaultHandler(
        base::Bind(&FakeGaia::HandleSigninURL));
    https_server_.RegisterDefaultHandler(
        base::Bind(&FakeGaia::HandleOAuth2TokenExchangeURL, &token_requested_));
  }

  // Navigates to the given path on the test server.
  void NavigateToURL(const std::string& path) {
    ui_test_utils::NavigateToURL(browser(), https_server_.GetURL(path));
  }

  // Returns the token service.
  ProfileOAuth2TokenService* GetTokenService() {
    return ProfileOAuth2TokenServiceFactory::GetForProfile(
        browser()->profile());
  }

  // Returns the account ID associated with kEmail, kGaiaID.
  std::string GetAccountID() {
    return AccountTrackerServiceFactory::GetForProfile(browser()->profile())
        ->PickAccountIdForAccount(kGaiaID, kEmail);
  }

  // InProcessBrowserTest:
  void SetUp() override {
    ASSERT_TRUE(https_server_.InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    const GURL& base_url = https_server_.base_url();
    command_line->AppendSwitchASCII(switches::kGaiaUrl, base_url.spec());
    command_line->AppendSwitchASCII(switches::kGoogleApisUrl, base_url.spec());
    switches::EnableAccountConsistencyDiceForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    https_server_.StartAcceptingConnections();
    GetTokenService()->AddObserver(this);
  }

  void TearDownOnMainThread() override {
    GetTokenService()->RemoveObserver(this);
  }

  // OAuth2TokenService::Observer:
  void OnRefreshTokenAvailable(const std::string& account_id) override {
    if (account_id == GetAccountID())
      refresh_token_available_ = true;
  }

  net::EmbeddedTestServer https_server_;
  bool token_requested_;
  bool refresh_token_available_;
};

// Checks that signin on Gaia triggers the fetch for a refresh token.
IN_PROC_BROWSER_TEST_F(DiceBrowserTest, Signin) {
  // Navigate to Gaia and sign in.
  NavigateToURL(kSigninURL);

  // Check that the Dice request header was sent.
  std::string header_value;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(document.body.textContent);",
      &header_value));
  EXPECT_EQ(base::StringPrintf(
                "client_id=%s",
                GaiaUrls::GetInstance()->oauth2_chrome_client_id().c_str()),
            header_value);

  // Check that the token was requested and added to the token service.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(token_requested_);
  EXPECT_TRUE(refresh_token_available_);
  EXPECT_TRUE(GetTokenService()->RefreshTokenIsAvailable(GetAccountID()));
}
