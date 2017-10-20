// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/dice_header_helper.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/sync/base/sync_prefs.h"
#include "components/variations/variations_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
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
using signin::AccountConsistencyMethod;

namespace {

constexpr int kAccountReconcilorDelayMs = 10;

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
const char kNoDiceRequestHeader[] = "NoDiceHeader";
const char kOAuth2TokenExchangeURL[] = "/oauth2/v4/token";
const char kOAuth2TokenRevokeURL[] = "/o/oauth2/revoke";
const char kSecondaryEmail[] = "secondary_email@example.com";
const char kSecondaryGaiaID[] = "secondary_gaia_id";
const char kSigninURL[] = "/signin";
const char kSignoutURL[] = "/signout";

}  // namespace

namespace FakeGaia {

// Handler for the signin page on the embedded test server.
// The response has the content of the Dice request header in its body, and has
// the Dice response header.
std::unique_ptr<HttpResponse> HandleSigninURL(
    const base::Callback<void(const std::string&)>& callback,
    const HttpRequest& request) {
  if (!net::test_server::ShouldHandle(request, kSigninURL))
    return nullptr;

  std::string header_value = kNoDiceRequestHeader;
  auto it = request.headers.find(signin::kDiceRequestHeader);
  if (it != request.headers.end())
    header_value = it->second;

  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   base::Bind(callback, header_value));

  std::unique_ptr<BasicHttpResponse> http_response(new BasicHttpResponse);
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
  http_response->AddCustomHeader(kGoogleSignoutResponseHeader,
                                 signout_header_value);
  http_response->AddCustomHeader("Cache-Control", "no-store");
  return std::move(http_response);
}

// Handler for OAuth2 token exchange.
// Checks that the request is well formatted and returns a refresh token in a
// JSON dictionary.
std::unique_ptr<HttpResponse> HandleOAuth2TokenExchangeURL(
    const base::Closure& callback,
    const HttpRequest& request) {
  if (!net::test_server::ShouldHandle(request, kOAuth2TokenExchangeURL))
    return nullptr;

  // Check that the authorization code is somewhere in the request body.
  if (!request.has_content)
    return nullptr;
  if (request.content.find(kAuthorizationCode) == std::string::npos)
    return nullptr;

  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   callback);

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

// Handler for ServiceLogin on the embedded test server.
// Calls the callback with the dice request header, or kNoDiceRequestHeader if
// there is no Dice header.
std::unique_ptr<HttpResponse> HandleServiceLoginURL(
    const base::Callback<void(const std::string&)>& callback,
    const HttpRequest& request) {
  if (!net::test_server::ShouldHandle(request, "/ServiceLogin"))
    return nullptr;

  std::string dice_request_header(kNoDiceRequestHeader);
  auto it = request.headers.find(signin::kDiceRequestHeader);
  if (it != request.headers.end())
    dice_request_header = it->second;
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   base::Bind(callback, dice_request_header));

  std::unique_ptr<BasicHttpResponse> http_response(new BasicHttpResponse);
  http_response->AddCustomHeader("Cache-Control", "no-store");
  return std::move(http_response);
}

}  // namespace FakeGaia

class DiceBrowserTestBase : public InProcessBrowserTest,
                            public OAuth2TokenService::Observer,
                            public AccountReconcilor::Observer {
 protected:
  ~DiceBrowserTestBase() override {}

  explicit DiceBrowserTestBase(
      AccountConsistencyMethod account_consistency_method)
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        account_consistency_method_(account_consistency_method),
        token_requested_(false),
        refresh_token_available_(false),
        token_revoked_notification_count_(0),
        token_revoked_count_(0),
        reconcilor_blocked_count_(0),
        reconcilor_unblocked_count_(0),
        reconcilor_started_count_(0) {
    https_server_.RegisterDefaultHandler(
        base::Bind(&FakeGaia::HandleSigninURL,
                   base::Bind(&DiceBrowserTestBase::OnSigninRequest,
                              base::Unretained(this))));
    https_server_.RegisterDefaultHandler(
        base::Bind(&FakeGaia::HandleSignoutURL));
    https_server_.RegisterDefaultHandler(
        base::Bind(&FakeGaia::HandleOAuth2TokenExchangeURL,
                   base::Bind(&DiceBrowserTestBase::OnTokenExchangeRequest,
                              base::Unretained(this))));
    https_server_.RegisterDefaultHandler(base::Bind(
        &FakeGaia::HandleOAuth2TokenRevokeURL, &token_revoked_count_));
    https_server_.RegisterDefaultHandler(
        base::Bind(&FakeGaia::HandleServiceLoginURL,
                   base::Bind(&DiceBrowserTestBase::OnServiceLoginRequest,
                              base::Unretained(this))));
    signin::SetDiceAccountReconcilorBlockDelayForTesting(
        kAccountReconcilorDelayMs);
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
    if (signin::IsDiceMigrationEnabled()) {
      EXPECT_EQ(1, reconcilor_blocked_count_);
      WaitForReconcilorUnblockedCount(1);
    } else {
      EXPECT_EQ(0, reconcilor_blocked_count_);
      WaitForReconcilorUnblockedCount(0);
    }
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

    // Append DICE field trial.
    std::string dice_method_name = "";
    switch (account_consistency_method_) {
      case signin::AccountConsistencyMethod::kDiceFixAuthErrors:
        dice_method_name = "dice_fix_auth_errors";
        break;
      case signin::AccountConsistencyMethod::kDiceMigration:
        dice_method_name = "dice_migration";
        break;
      case signin::AccountConsistencyMethod::kDice:
        dice_method_name = "dice";
        break;
      case signin::AccountConsistencyMethod::kDisabled:
      case signin::AccountConsistencyMethod::kMirror:
        NOTREACHED();
        return;
    }
    command_line->AppendSwitchASCII(switches::kForceFieldTrials, "Trial/Group");
    command_line->AppendSwitchASCII(
        variations::switches::kForceFieldTrialParams,
        "Trial.Group:method/" + dice_method_name);
    command_line->AppendSwitchASCII(switches::kEnableFeatures,
                                    "AccountConsistency<Trial");
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    https_server_.StartAcceptingConnections();

    GetTokenService()->AddObserver(this);
    // Wait for the token service to be ready.
    if (!GetTokenService()->AreAllCredentialsLoaded())
      WaitForClosure(&tokens_loaded_quit_closure_);
    ASSERT_TRUE(GetTokenService()->AreAllCredentialsLoaded());

    AccountReconcilor* reconcilor =
        AccountReconcilorFactory::GetForProfile(browser()->profile());

    // Reconcilor starts as soon as the token service finishes loading its
    // credentials. Abort the reconcilor here to make sure tests start in a
    // stable state.
    reconcilor->AbortReconcile();
    reconcilor->AddObserver(this);
  }

  void TearDownOnMainThread() override {
    GetTokenService()->RemoveObserver(this);
    AccountReconcilorFactory::GetForProfile(browser()->profile())
        ->RemoveObserver(this);
  }

  // OAuth2TokenService::Observer:
  void OnRefreshTokenAvailable(const std::string& account_id) override {
    if (account_id == GetMainAccountID()) {
      refresh_token_available_ = true;
      RunClosureIfValid(&refresh_token_available_quit_closure_);
    }
  }

  void OnRefreshTokenRevoked(const std::string& account_id) override {
    ++token_revoked_notification_count_;
  }

  void OnRefreshTokensLoaded() override {
    RunClosureIfValid(&tokens_loaded_quit_closure_);
  }

  // Calls |closure| if it is not null and resets it after.
  void RunClosureIfValid(base::Closure* closure) {
    DCHECK(closure);
    if (!closure->is_null()) {
      closure->Run();
      closure->Reset();
    }
  }

  // Creates and runs a RunLoop until |closure| is called.
  void WaitForClosure(base::Closure* closure) {
    base::RunLoop run_loop;
    *closure = run_loop.QuitClosure();
    run_loop.Run();
  }

  // FakeGaia callbacks:
  void OnSigninRequest(const std::string& dice_request_header) {
    EXPECT_EQ(signin::IsDiceMigrationEnabled(), IsReconcilorBlocked());
    dice_request_header_ = dice_request_header;
  }

  void OnServiceLoginRequest(const std::string& dice_request_header) {
    dice_request_header_ = dice_request_header;
    RunClosureIfValid(&service_login_quit_closure_);
  }

  void OnTokenExchangeRequest() {
    // The token must be exchanged only once.
    EXPECT_FALSE(token_requested_);
    EXPECT_EQ(signin::IsDiceMigrationEnabled(), IsReconcilorBlocked());
    token_requested_ = true;
    RunClosureIfValid(&token_requested_quit_closure_);
  }

  // AccountReconcilor::Observer:
  void OnBlockReconcile() override { ++reconcilor_blocked_count_; }
  void OnUnblockReconcile() override {
    ++reconcilor_unblocked_count_;
    RunClosureIfValid(&unblock_count_quit_closure_);
  }
  void OnStartReconcile() override { ++reconcilor_started_count_; }

  // Returns true if the account reconcilor is currently blocked.
  bool IsReconcilorBlocked() {
    EXPECT_GE(reconcilor_blocked_count_, reconcilor_unblocked_count_);
    EXPECT_LE(reconcilor_blocked_count_, reconcilor_unblocked_count_ + 1);
    return (reconcilor_unblocked_count_ + 1) == reconcilor_blocked_count_;
  }

  // Waits until |reconcilor_unblocked_count_| reaches |count|.
  void WaitForReconcilorUnblockedCount(int count) {
    if (reconcilor_unblocked_count_ == count)
      return;

    ASSERT_EQ(count - 1, reconcilor_unblocked_count_);
    // Wait for the timeout after the request is complete.
    WaitForClosure(&unblock_count_quit_closure_);
    EXPECT_EQ(count, reconcilor_unblocked_count_);
  }

  // Waits until the token request is sent to the server and the response is
  // received.
  void WaitForTokenReceived() {
    // Wait for the request hitting the server.
    if (!token_requested_)
      WaitForClosure(&token_requested_quit_closure_);
    EXPECT_TRUE(token_requested_);
    // Wait for the response coming back.
    if (!refresh_token_available_)
      WaitForClosure(&refresh_token_available_quit_closure_);
    EXPECT_TRUE(refresh_token_available_);
  }

  net::EmbeddedTestServer https_server_;
  AccountConsistencyMethod account_consistency_method_;
  bool token_requested_;
  bool refresh_token_available_;
  int token_revoked_notification_count_;
  int token_revoked_count_;
  int reconcilor_blocked_count_;
  int reconcilor_unblocked_count_;
  int reconcilor_started_count_;
  std::string dice_request_header_;

  // Used for waiting asynchronous events.
  base::Closure token_requested_quit_closure_;
  base::Closure refresh_token_available_quit_closure_;
  base::Closure service_login_quit_closure_;
  base::Closure unblock_count_quit_closure_;
  base::Closure tokens_loaded_quit_closure_;
};

class DiceBrowserTest : public DiceBrowserTestBase {
 public:
  DiceBrowserTest() : DiceBrowserTestBase(AccountConsistencyMethod::kDice) {}
};

class DiceFixAuthErrorsBrowserTest : public DiceBrowserTestBase {
 public:
  DiceFixAuthErrorsBrowserTest()
      : DiceBrowserTestBase(AccountConsistencyMethod::kDiceFixAuthErrors) {}
};

// Checks that signin on Gaia triggers the fetch for a refresh token.
IN_PROC_BROWSER_TEST_F(DiceBrowserTest, Signin) {
  EXPECT_EQ(0, reconcilor_started_count_);

  // Navigate to Gaia and sign in.
  NavigateToURL(kSigninURL);

  // Check that the Dice request header was sent.
  std::string client_id = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  EXPECT_EQ(
      base::StringPrintf("version=%s,client_id=%s,signin_mode=all_accounts,"
                         "signout_mode=show_confirmation",
                         signin::kDiceProtocolVersion, client_id.c_str()),
      dice_request_header_);

  // Check that the token was requested and added to the token service.
  WaitForTokenReceived();
  EXPECT_TRUE(GetTokenService()->RefreshTokenIsAvailable(GetMainAccountID()));
  // Sync should not be enabled.
  EXPECT_TRUE(GetSigninManager()->GetAuthenticatedAccountId().empty());

  EXPECT_EQ(1, reconcilor_blocked_count_);
  WaitForReconcilorUnblockedCount(1);
  EXPECT_EQ(1, reconcilor_started_count_);
}

// This test is flaky on Mac, see https://crbug.com/765093
#if defined(OS_MACOSX)
#define MAYBE_Reauth DISABLED_Reauth
#else
#define MAYBE_Reauth Reauth
#endif
// Checks that re-auth on Gaia triggers the fetch for a refresh token.
IN_PROC_BROWSER_TEST_F(DiceBrowserTest, MAYBE_Reauth) {
  EXPECT_EQ(0, reconcilor_started_count_);

  // Start from a signed-in state.
  SetupSignedInAccounts();
  EXPECT_EQ(1, reconcilor_started_count_);

  // Navigate to Gaia and sign in again with the main account.
  NavigateToURL(kSigninURL);

  // Check that the Dice request header was sent.
  std::string client_id = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  EXPECT_EQ(
      base::StringPrintf("version=%s,client_id=%s,signin_mode=all_accounts,"
                         "signout_mode=show_confirmation",
                         signin::kDiceProtocolVersion, client_id.c_str()),
      dice_request_header_);

  // Check that the token was requested and added to the token service.
  WaitForTokenReceived();
  EXPECT_EQ(GetMainAccountID(),
            GetSigninManager()->GetAuthenticatedAccountId());
  // Old token must be revoked silently.
  EXPECT_EQ(0, token_revoked_notification_count_);
  EXPECT_EQ(1, token_revoked_count_);

  EXPECT_EQ(1, reconcilor_blocked_count_);
  WaitForReconcilorUnblockedCount(1);
  EXPECT_EQ(2, reconcilor_started_count_);
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
  EXPECT_EQ(1, reconcilor_blocked_count_);
  WaitForReconcilorUnblockedCount(1);
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
  EXPECT_EQ(1, reconcilor_blocked_count_);
  WaitForReconcilorUnblockedCount(1);
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
  EXPECT_EQ(1, reconcilor_blocked_count_);
  WaitForReconcilorUnblockedCount(1);
}

// Checks that Dice request header is not set from request from WebUI.
// See https://crbug.com/428396
IN_PROC_BROWSER_TEST_F(DiceBrowserTest, NoDiceFromWebUI) {

  // Navigate to Gaia and from the native tab, which uses an extension.
  ui_test_utils::NavigateToURL(browser(), GURL("chrome:chrome-signin"));

  // Check that the request had no Dice request header.
  if (dice_request_header_.empty())
    WaitForClosure(&service_login_quit_closure_);
  EXPECT_EQ(kNoDiceRequestHeader, dice_request_header_);
  EXPECT_EQ(0, reconcilor_blocked_count_);
  WaitForReconcilorUnblockedCount(0);
}

// Checks that signin on Gaia does not trigger the fetch of refresh token when
// there is no authentication error.
IN_PROC_BROWSER_TEST_F(DiceFixAuthErrorsBrowserTest, SigninNoAuthError) {
  // Start from a signed-in state.
  SetupSignedInAccounts();
  syncer::SyncPrefs(browser()->profile()->GetPrefs()).SetFirstSetupComplete();

  // Navigate to Gaia and sign in.
  NavigateToURL(kSigninURL);

  // Check that the Dice request header was not sent.
  EXPECT_EQ(kNoDiceRequestHeader, dice_request_header_);
  EXPECT_EQ(0, reconcilor_blocked_count_);
  WaitForReconcilorUnblockedCount(0);
}

// Checks that signin on Gaia does not triggers the fetch for a refresh token
// when the user is not signed into Chrome.
IN_PROC_BROWSER_TEST_F(DiceFixAuthErrorsBrowserTest, NotSignedInChrome) {
  // Setup authentication error.
  syncer::SyncPrefs(browser()->profile()->GetPrefs()).SetSyncAuthError(true);
  syncer::SyncPrefs(browser()->profile()->GetPrefs()).SetFirstSetupComplete();

  // Navigate to Gaia and sign in.
  NavigateToURL(kSigninURL);

  // Check that the Dice request header was not sent.
  EXPECT_EQ(kNoDiceRequestHeader, dice_request_header_);
  EXPECT_EQ(0, reconcilor_blocked_count_);
  WaitForReconcilorUnblockedCount(0);
}

// Checks that a refresh token is not requested when accounts don't match.
IN_PROC_BROWSER_TEST_F(DiceFixAuthErrorsBrowserTest, SigninAccountMismatch) {
  // Sign in to Chrome with secondary account, with authentication error.
  SigninManager* signin_manager = GetSigninManager();
  signin_manager->StartSignInWithRefreshToken(
      "existing_refresh_token", kSecondaryGaiaID, kSecondaryEmail, "password",
      SigninManager::OAuthTokenFetchedCallback());
  ASSERT_TRUE(
      GetTokenService()->RefreshTokenIsAvailable(GetSecondaryAccountID()));
  ASSERT_EQ(GetSecondaryAccountID(),
            signin_manager->GetAuthenticatedAccountId());
  syncer::SyncPrefs(browser()->profile()->GetPrefs()).SetSyncAuthError(true);
  syncer::SyncPrefs(browser()->profile()->GetPrefs()).SetFirstSetupComplete();

  // Navigate to Gaia and sign in with the main account (account mismatch).
  NavigateToURL(kSigninURL);

  // Check that the Dice request header was sent.
  std::string client_id = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  EXPECT_EQ(base::StringPrintf("version=%s,client_id=%s,sync_account_id=%s,"
                               "signin_mode=sync_account,"
                               "signout_mode=no_confirmation",
                               signin::kDiceProtocolVersion, client_id.c_str(),
                               GetSecondaryAccountID().c_str()),
            dice_request_header_);

  // Check that the token was not requested and the authenticated account did
  // not change.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(token_requested_);
  EXPECT_FALSE(refresh_token_available_);
  EXPECT_EQ(GetSecondaryAccountID(),
            GetSigninManager()->GetAuthenticatedAccountId());
  EXPECT_EQ(0, reconcilor_blocked_count_);
  WaitForReconcilorUnblockedCount(0);
}

// Checks that signin on Gaia triggers the fetch for a refresh token when there
// is an authentication error and the user is re-authenticating on the web.
// This test is similar to DiceBrowserTest.Reauth.
IN_PROC_BROWSER_TEST_F(DiceFixAuthErrorsBrowserTest, ReauthFixAuthError) {
  // Start from a signed-in state with authentication error.
  SetupSignedInAccounts();
  syncer::SyncPrefs(browser()->profile()->GetPrefs()).SetSyncAuthError(true);
  syncer::SyncPrefs(browser()->profile()->GetPrefs()).SetFirstSetupComplete();

  // Navigate to Gaia and sign in again with the main account.
  NavigateToURL(kSigninURL);

  // Check that the Dice request header was sent.
  std::string client_id = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  EXPECT_EQ(base::StringPrintf("version=%s,client_id=%s,sync_account_id=%s,"
                               "signin_mode=sync_account,"
                               "signout_mode=no_confirmation",
                               signin::kDiceProtocolVersion, client_id.c_str(),
                               GetMainAccountID().c_str()),
            dice_request_header_);

  // Check that the token was requested and added to the token service.
  WaitForTokenReceived();
  EXPECT_EQ(GetMainAccountID(),
            GetSigninManager()->GetAuthenticatedAccountId());
  // Old token must be revoked silently.
  EXPECT_EQ(0, token_revoked_notification_count_);
  EXPECT_EQ(1, token_revoked_count_);
  EXPECT_EQ(0, reconcilor_blocked_count_);
  WaitForReconcilorUnblockedCount(0);
}

// Checks that the Dice signout flow is disabled.
IN_PROC_BROWSER_TEST_F(DiceFixAuthErrorsBrowserTest, Signout) {
  // Start from a signed-in state.
  SetupSignedInAccounts();

  // Signout from main account on the web.
  SignOutWithDice(kMainAccount);

  // Check that the user is still signed in Chrome.
  EXPECT_EQ(GetMainAccountID(),
            GetSigninManager()->GetAuthenticatedAccountId());
  EXPECT_TRUE(GetTokenService()->RefreshTokenIsAvailable(GetMainAccountID()));
  EXPECT_TRUE(
      GetTokenService()->RefreshTokenIsAvailable(GetSecondaryAccountID()));
  EXPECT_EQ(0, token_revoked_notification_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(0, reconcilor_blocked_count_);
  WaitForReconcilorUnblockedCount(0);
}
