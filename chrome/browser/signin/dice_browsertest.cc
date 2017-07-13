// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/browser/signin_manager.h"
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

enum SignoutType {
  kSignoutTypeFirst = 0,

  kAllAccounts = 0,       // Sign out from all accounts.
  kMainAccount = 1,       // Sign out from main account only.
  kSecondaryAccount = 2,  // Sign out from secondary account only.

  kSignoutTypeLast
};

const char kAuthorizationCode[] = "authorization_code";
const char kDiceResponseHeader[] = "X-Chrome-ID-Consistency-Response";
const char kGoogleSignoutResponseHeader[] = "Google-Accounts-SignOut";
const char kMainEmail[] = "main_email@example.com";
const char kMainGaiaID[] = "main_gaia_id";
const char kOAuth2TokenExchangeURL[] = "/oauth2/v4/token";
const char kOAuth2TokenRevokeURL[] = "/o/oauth2/revoke";
const char kSecondaryEmail[] = "secondary_email@example.com";
const char kSecondaryGaiaID[] = "secondary_gaia_id";
const char kSigninURL[] = "/signin";
const char kSignoutURL[] = "/signout";
const char kSignedOutMessage[] = "signed_out";

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
          kMainGaiaID, kMainEmail, kAuthorizationCode));
  http_response->AddCustomHeader("Cache-Control", "no-store");
  return std::move(http_response);
}

// Handler for the signout page on the embedded test server.
// Responds with a Google-Accounts-SignOut header for the main account, the
// secondary account, or both (depending on the SignoutType, which is encoded in
// the query string).
std::unique_ptr<HttpResponse> HandleSignoutURL(const HttpRequest& request) {
  if (!net::test_server::ShouldHandle(request, kSignoutURL))
    return nullptr;

  // Build signout header.
  int query_value;
  EXPECT_TRUE(base::StringToInt(request.GetURL().query(), &query_value));
  SignoutType signout_type = static_cast<SignoutType>(query_value);
  EXPECT_GE(signout_type, kSignoutTypeFirst);
  EXPECT_LT(signout_type, kSignoutTypeLast);
  std::string signout_header_value;
  if (signout_type == kAllAccounts || signout_type == kMainAccount) {
    signout_header_value =
        base::StringPrintf("email=\"%s\", obfuscatedid=\"%s\", sessionindex=1",
                           kMainEmail, kMainGaiaID);
  }
  if (signout_type == kAllAccounts || signout_type == kSecondaryAccount) {
    if (!signout_header_value.empty())
      signout_header_value += ", ";
    signout_header_value +=
        base::StringPrintf("email=\"%s\", obfuscatedid=\"%s\", sessionindex=2",
                           kSecondaryEmail, kSecondaryGaiaID);
  }

  std::unique_ptr<BasicHttpResponse> http_response(new BasicHttpResponse);
  http_response->set_content(kSignedOutMessage);
  http_response->set_content_type("text/plain");
  http_response->AddCustomHeader(kGoogleSignoutResponseHeader,
                                 signout_header_value);
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
      "  \"refresh_token\":\"new_refresh_token\","
      "  \"expires_in\":9999"
      "}";

  http_response->set_content(content);
  http_response->set_content_type("text/plain");
  http_response->AddCustomHeader("Cache-Control", "no-store");
  return std::move(http_response);
}

// Handler for OAuth2 token revocation.
std::unique_ptr<HttpResponse> HandleOAuth2TokenRevokeURL(
    int* out_token_revoked_count,
    const HttpRequest& request) {
  if (!net::test_server::ShouldHandle(request, kOAuth2TokenRevokeURL))
    return nullptr;

  ++(*out_token_revoked_count);

  std::unique_ptr<BasicHttpResponse> http_response(new BasicHttpResponse);
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
        refresh_token_available_(false),
        token_revoked_notification_count_(0),
        token_revoked_count_(0) {
    https_server_.RegisterDefaultHandler(
        base::Bind(&FakeGaia::HandleSigninURL));
    https_server_.RegisterDefaultHandler(
        base::Bind(&FakeGaia::HandleSignoutURL));
    https_server_.RegisterDefaultHandler(
        base::Bind(&FakeGaia::HandleOAuth2TokenExchangeURL, &token_requested_));
    https_server_.RegisterDefaultHandler(base::Bind(
        &FakeGaia::HandleOAuth2TokenRevokeURL, &token_revoked_count_));
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

  // Returns the account tracker service.
  AccountTrackerService* GetAccountTrackerService() {
    return AccountTrackerServiceFactory::GetForProfile(browser()->profile());
  }

  // Returns the signin manager.
  SigninManager* GetSigninManager() {
    return SigninManagerFactory::GetForProfile(browser()->profile());
  }

  // Returns the account ID associated with kMainEmail, kMainGaiaID.
  std::string GetMainAccountID() {
    return GetAccountTrackerService()->PickAccountIdForAccount(kMainGaiaID,
                                                               kMainEmail);
  }

  // Returns the account ID associated with kSecondaryEmail, kSecondaryGaiaID.
  std::string GetSecondaryAccountID() {
    return GetAccountTrackerService()->PickAccountIdForAccount(kSecondaryGaiaID,
                                                               kSecondaryEmail);
  }

  // Signin with a main account and add token for a secondary account.
  void SetupSignedInAccounts() {
    // Signin main account.
    SigninManager* signin_manager = GetSigninManager();
    signin_manager->StartSignInWithRefreshToken(
        "existing_refresh_token", kMainGaiaID, kMainEmail, "password",
        SigninManager::OAuthTokenFetchedCallback());
    ASSERT_TRUE(GetTokenService()->RefreshTokenIsAvailable(GetMainAccountID()));
    ASSERT_EQ(GetMainAccountID(), signin_manager->GetAuthenticatedAccountId());

    // Add a token for a secondary account.
    std::string secondary_account_id =
        GetAccountTrackerService()->SeedAccountInfo(kSecondaryGaiaID,
                                                    kSecondaryEmail);
    GetTokenService()->UpdateCredentials(secondary_account_id, "other_token");
    ASSERT_TRUE(
        GetTokenService()->RefreshTokenIsAvailable(secondary_account_id));
  }

  // Navigate to a Gaia URL setting the Google-Accounts-SignOut header.
  void SignOutWithDice(SignoutType signout_type) {
    NavigateToURL(base::StringPrintf("%s?%i", kSignoutURL, signout_type));
    std::string page_content;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "window.domAutomationController.send(document.body.textContent);",
        &page_content));
    EXPECT_EQ(kSignedOutMessage, page_content);
    base::RunLoop().RunUntilIdle();
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
    command_line->AppendSwitchASCII(switches::kLsoUrl, base_url.spec());
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
    if (account_id == GetMainAccountID())
      refresh_token_available_ = true;
  }

  void OnRefreshTokenRevoked(const std::string& account_id) override {
    ++token_revoked_notification_count_;
  }

  net::EmbeddedTestServer https_server_;
  bool token_requested_;
  bool refresh_token_available_;
  int token_revoked_notification_count_;
  int token_revoked_count_;
};

// This test is flaky on Windows, see https://crbug.com/741652
#if defined(OS_WIN)
#define MAYBE_Signin DISABLED_Signin
#else
#define MAYBE_Signin Signin
#endif
// Checks that signin on Gaia triggers the fetch for a refresh token.
IN_PROC_BROWSER_TEST_F(DiceBrowserTest, MAYBE_Signin) {
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
  EXPECT_TRUE(GetTokenService()->RefreshTokenIsAvailable(GetMainAccountID()));
  // Sync should not be enabled.
  EXPECT_TRUE(GetSigninManager()->GetAuthenticatedAccountId().empty());
}

// Checks that re-auth on Gaia triggers the fetch for a refresh token.
IN_PROC_BROWSER_TEST_F(DiceBrowserTest, Reauth) {
  // Start from a signed-in state.
  SetupSignedInAccounts();

  // Navigate to Gaia and sign in again with the main account.
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
  EXPECT_EQ(GetMainAccountID(),
            GetSigninManager()->GetAuthenticatedAccountId());
  // Old token must be revoked silently.
  EXPECT_EQ(0, token_revoked_notification_count_);
  EXPECT_EQ(1, token_revoked_count_);
}

// Checks that the Dice signout flow works and deletes all tokens.
IN_PROC_BROWSER_TEST_F(DiceBrowserTest, SignoutMainAccount) {
  // Start from a signed-in state.
  SetupSignedInAccounts();

  // Signout from main account.
  SignOutWithDice(kMainAccount);

  // Check that the user is signed out and all tokens are deleted.
  EXPECT_TRUE(GetSigninManager()->GetAuthenticatedAccountId().empty());
  EXPECT_FALSE(GetTokenService()->RefreshTokenIsAvailable(GetMainAccountID()));
  EXPECT_FALSE(
      GetTokenService()->RefreshTokenIsAvailable(GetSecondaryAccountID()));
  EXPECT_EQ(2, token_revoked_notification_count_);
  EXPECT_EQ(2, token_revoked_count_);
}

// Checks that signing out from a secondary account does not delete the main
// token.
IN_PROC_BROWSER_TEST_F(DiceBrowserTest, SignoutSecondaryAccount) {
  // Start from a signed-in state.
  SetupSignedInAccounts();

  // Signout from secondary account.
  SignOutWithDice(kSecondaryAccount);

  // Check that the user is still signed in from main account, but secondary
  // token is deleted.
  EXPECT_EQ(GetMainAccountID(),
            GetSigninManager()->GetAuthenticatedAccountId());
  EXPECT_TRUE(GetTokenService()->RefreshTokenIsAvailable(GetMainAccountID()));
  EXPECT_FALSE(
      GetTokenService()->RefreshTokenIsAvailable(GetSecondaryAccountID()));
  EXPECT_EQ(1, token_revoked_notification_count_);
  EXPECT_EQ(1, token_revoked_count_);
}

// Checks that the Dice signout flow works and deletes all tokens.
IN_PROC_BROWSER_TEST_F(DiceBrowserTest, SignoutAllAccounts) {
  // Start from a signed-in state.
  SetupSignedInAccounts();

  // Signout from all accounts.
  SignOutWithDice(kAllAccounts);

  // Check that the user is signed out and all tokens are deleted.
  EXPECT_TRUE(GetSigninManager()->GetAuthenticatedAccountId().empty());
  EXPECT_FALSE(GetTokenService()->RefreshTokenIsAvailable(GetMainAccountID()));
  EXPECT_FALSE(
      GetTokenService()->RefreshTokenIsAvailable(GetSecondaryAccountID()));
  EXPECT_EQ(2, token_revoked_notification_count_);
  EXPECT_EQ(2, token_revoked_count_);
}
