// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/fake_gaia_cookie_manager_service_builder.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_gaia_cookie_manager_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/scoped_account_consistency.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/signin/core/common/signin_features.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/fake_oauth2_token_service_delegate.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockAccountReconcilor : public testing::StrictMock<AccountReconcilor> {
 public:
  static std::unique_ptr<KeyedService> Build(content::BrowserContext* context);

  MockAccountReconcilor(ProfileOAuth2TokenService* token_service,
                        SigninManagerBase* signin_manager,
                        SigninClient* client,
                        GaiaCookieManagerService* cookie_manager_service);
  ~MockAccountReconcilor() override {}

  MOCK_METHOD1(PerformMergeAction, void(const std::string& account_id));
  MOCK_METHOD0(PerformLogoutAllAccountsAction, void());
};

// static
std::unique_ptr<KeyedService> MockAccountReconcilor::Build(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  std::unique_ptr<AccountReconcilor> reconcilor(new MockAccountReconcilor(
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      SigninManagerFactory::GetForProfile(profile),
      ChromeSigninClientFactory::GetForProfile(profile),
      GaiaCookieManagerServiceFactory::GetForProfile(profile)));
  reconcilor->Initialize(false /* start_reconcile_if_tokens_available */);
  return std::move(reconcilor);
}

MockAccountReconcilor::MockAccountReconcilor(
    ProfileOAuth2TokenService* token_service,
    SigninManagerBase* signin_manager,
    SigninClient* client,
    GaiaCookieManagerService* cookie_manager_service)
    : testing::StrictMock<AccountReconcilor>(token_service,
                                             signin_manager,
                                             client,
                                             cookie_manager_service) {}

}  // namespace

class AccountReconcilorTest : public ::testing::Test {
 public:
  AccountReconcilorTest();
  void SetUp() override;

  TestingProfile* profile() { return profile_; }
  FakeSigninManagerForTesting* signin_manager() { return signin_manager_; }
  FakeProfileOAuth2TokenService* token_service() { return token_service_; }
  FakeOAuth2TokenServiceDelegate* token_service_delegate() {
    return static_cast<FakeOAuth2TokenServiceDelegate*>(
        token_service_->GetDelegate());
  }
  TestSigninClient* test_signin_client() { return test_signin_client_; }
  AccountTrackerService* account_tracker() { return account_tracker_; }
  FakeGaiaCookieManagerService* cookie_manager_service() {
    return cookie_manager_service_;
  }
  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

  void SetFakeResponse(const std::string& url,
                       const std::string& data,
                       net::HttpStatusCode code,
                       net::URLRequestStatus::Status status) {
    url_fetcher_factory_.SetFakeResponse(GURL(url), data, code, status);
  }

  MockAccountReconcilor* GetMockReconcilor();

  std::string ConnectProfileToAccount(const std::string& gaia_id,
                                      const std::string& username);

  std::string PickAccountIdForAccount(const std::string& gaia_id,
                                      const std::string& username);

  void SimulateAddAccountToCookieCompleted(
      GaiaCookieManagerService::Observer* observer,
      const std::string& account_id,
      const GoogleServiceAuthError& error);

  void SimulateCookieContentSettingsChanged(
      content_settings::Observer* observer,
      const ContentSettingsPattern& primary_pattern);

  GURL get_check_connection_info_url() {
    return get_check_connection_info_url_;
  }

 private:
  content::TestBrowserThreadBundle bundle_;
  TestingProfile* profile_;
  FakeSigninManagerForTesting* signin_manager_;
  FakeProfileOAuth2TokenService* token_service_;
  TestSigninClient* test_signin_client_;
  AccountTrackerService* account_tracker_;
  FakeGaiaCookieManagerService* cookie_manager_service_;
  MockAccountReconcilor* mock_reconcilor_;
  net::FakeURLFetcherFactory url_fetcher_factory_;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  base::HistogramTester histogram_tester_;
  GURL get_check_connection_info_url_;

  DISALLOW_COPY_AND_ASSIGN(AccountReconcilorTest);
};

AccountReconcilorTest::AccountReconcilorTest()
    : signin_manager_(NULL),
      token_service_(NULL),
      test_signin_client_(NULL),
      cookie_manager_service_(NULL),
      mock_reconcilor_(NULL),
      url_fetcher_factory_(NULL) {}

void AccountReconcilorTest::SetUp() {
  get_check_connection_info_url_ =
      GaiaUrls::GetInstance()->GetCheckConnectionInfoURLWithSource(
          GaiaConstants::kChromeSource);

  testing_profile_manager_.reset(
      new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
  ASSERT_TRUE(testing_profile_manager_.get()->SetUp());

  TestingProfile::TestingFactories factories;
  factories.push_back(std::make_pair(ChromeSigninClientFactory::GetInstance(),
      signin::BuildTestSigninClient));
  factories.push_back(std::make_pair(
      ProfileOAuth2TokenServiceFactory::GetInstance(),
      BuildFakeProfileOAuth2TokenService));
  factories.push_back(
      std::make_pair(GaiaCookieManagerServiceFactory::GetInstance(),
                     BuildFakeGaiaCookieManagerService));
  factories.push_back(std::make_pair(SigninManagerFactory::GetInstance(),
                                     BuildFakeSigninManagerBase));
  factories.push_back(std::make_pair(AccountReconcilorFactory::GetInstance(),
      MockAccountReconcilor::Build));

  profile_ = testing_profile_manager_.get()->CreateTestingProfile(
      "name", std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
      base::UTF8ToUTF16("name"), 0, std::string(), factories);

  test_signin_client_ =
      static_cast<TestSigninClient*>(
          ChromeSigninClientFactory::GetForProfile(profile()));

  token_service_ =
      static_cast<FakeProfileOAuth2TokenService*>(
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile()));

  account_tracker_ =
      AccountTrackerServiceFactory::GetForProfile(profile());

  signin_manager_ =
      static_cast<FakeSigninManagerForTesting*>(
          SigninManagerFactory::GetForProfile(profile()));

  test_signin_client_ =
      static_cast<TestSigninClient*>(
          ChromeSigninClientFactory::GetForProfile(profile()));

  cookie_manager_service_ =
      static_cast<FakeGaiaCookieManagerService*>(
        GaiaCookieManagerServiceFactory::GetForProfile(profile()));
  cookie_manager_service_->Init(&url_fetcher_factory_);

  cookie_manager_service_->SetListAccountsResponseHttpNotFound();
}

MockAccountReconcilor* AccountReconcilorTest::GetMockReconcilor() {
  if (!mock_reconcilor_) {
    mock_reconcilor_ =
        static_cast<MockAccountReconcilor*>(
            AccountReconcilorFactory::GetForProfile(profile()));
  }

  return mock_reconcilor_;
}

std::string AccountReconcilorTest::ConnectProfileToAccount(
    const std::string& gaia_id,
    const std::string& username) {
  const std::string account_id = PickAccountIdForAccount(gaia_id, username);
#if !defined(OS_CHROMEOS)
  signin_manager()->set_password("password");
#endif
  signin_manager()->SetAuthenticatedAccountInfo(gaia_id, username);
  token_service()->UpdateCredentials(account_id, "refresh_token");
  return account_id;
}

std::string AccountReconcilorTest::PickAccountIdForAccount(
    const std::string& gaia_id,
    const std::string& username) {
  return account_tracker()->PickAccountIdForAccount(gaia_id, username);
}

void AccountReconcilorTest::SimulateAddAccountToCookieCompleted(
    GaiaCookieManagerService::Observer* observer,
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  observer->OnAddAccountToCookieCompleted(account_id, error);
}

void AccountReconcilorTest::SimulateCookieContentSettingsChanged(
    content_settings::Observer* observer,
    const ContentSettingsPattern& primary_pattern) {
  observer->OnContentSettingChanged(
      primary_pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_COOKIES,
      std::string());
}

TEST_F(AccountReconcilorTest, Basic) {
  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);
}

#if !defined(OS_CHROMEOS)

// This method requires the use of the |TestSigninClient| to be created from the
// |ChromeSigninClientFactory| because it overrides the |GoogleSigninSucceeded|
// method with an empty implementation. On MacOS, the normal implementation
// causes the try_bots to time out.
TEST_F(AccountReconcilorTest, SigninManagerRegistration) {
  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);
  ASSERT_FALSE(reconcilor->IsRegisteredWithTokenService());

  account_tracker()->SeedAccountInfo("12345", "user@gmail.com");
  signin_manager()->SignIn("12345", "user@gmail.com", "password");
  ASSERT_TRUE(reconcilor->IsRegisteredWithTokenService());

  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction());

  signin_manager()->SignOut(signin_metrics::SIGNOUT_TEST,
                            signin_metrics::SignoutDelete::IGNORE_METRIC);
  ASSERT_FALSE(reconcilor->IsRegisteredWithTokenService());
}

// This method requires the use of the |TestSigninClient| to be created from the
// |ChromeSigninClientFactory| because it overrides the |GoogleSigninSucceeded|
// method with an empty implementation. On MacOS, the normal implementation
// causes the try_bots to time out.
TEST_F(AccountReconcilorTest, Reauth) {
  const std::string email = "user@gmail.com";
  const std::string account_id =
      ConnectProfileToAccount("12345", email);

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);
  ASSERT_TRUE(reconcilor->IsRegisteredWithTokenService());

  // Simulate reauth.  The state of the reconcilor should not change.
  signin_manager()->OnExternalSigninCompleted(email);
  ASSERT_TRUE(reconcilor->IsRegisteredWithTokenService());
}

#endif  // !defined(OS_CHROMEOS)

TEST_F(AccountReconcilorTest, ProfileAlreadyConnected) {
  ConnectProfileToAccount("12345", "user@gmail.com");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);
  ASSERT_TRUE(reconcilor->IsRegisteredWithTokenService());
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

struct AccountReconcilorTestDiceParam {
  const char* tokens;
  const char* cookies;
  bool is_first_reconcile;
  const char* gaia_api_calls;
  const char* tokens_after_reconcile;
  const char* cookies_after_reconcile;
};

// Pretty prints a AccountReconcilorTestDiceParam. Used by gtest.
void PrintTo(const AccountReconcilorTestDiceParam& param, ::std::ostream* os) {
  *os << "Tokens: " << param.tokens << ". Cookies: " << param.cookies
      << ". First reconcile: " << param.is_first_reconcile;
}

// clang-format off
const AccountReconcilorTestDiceParam kDiceParams[] = {
    // This table encodes the initial state and expectations of a reconcile.
    // The syntax is:
    // - Tokens:
    //   A, B, C: Accounts for which we have a token in Chrome.
    //   *: The next account is the main Chrome account (i.e. in SigninManager).
    //   x: The next account has a token error.
    // - Cookies:
    //   A, B, C: Accounts in the Gaia cookie (returned by ListAccounts).
    // - First Run: true if this is the first reconcile (i.e. Chrome startup).
    // - API calls:
    //   X: Logout all accounts.
    //   A, B, C: Merge account.
    // -------------------------------------------------------------------------
    // Tokens | Cookies | First Run | Gaia calls | Tokens after | Cookies after
    // -------------------------------------------------------------------------

    // First reconcile (Chrome restart): Rebuild the Gaia cookie to match the
    // tokens. Make the Sync account the default account in the Gaia cookie.
    // Sync enabled.
    {  "*AB",   "AB",     true,       "",          "*AB",         "AB"},
    {  "*AB",   "BA",     true,       "XAB",       "*AB",         "AB"},
    {  "*AB",   "A",      true,       "B",         "*AB",         "AB"},
    {  "*AB",   "B",      true,       "XAB",       "*AB",         "AB"},
    {  "*AB",   "",       true,       "AB",        "*AB",         "AB"},
    // Sync enabled, token error on primary.
    {  "*xAB",  "AB",     true,       "X",         "*xA",         ""},
    {  "*xAB",  "BA",     true,       "XB",        "*xAB",        "B"},
    {  "*xAB",  "A",      true,       "X",         "*xA",         ""},
    {  "*xAB",  "B",      true,       "",          "*xAB",        "B"},
    {  "*xAB",  "",       true,       "B",         "*xAB",        "B"},
    // Sync enabled, token error on secondary.
    {  "*AxB",  "AB",     true,       "XA",        "*AxB",        "A"},
    {  "*AxB",  "BA",     true,       "XA",        "*AxB",        "A"},
    {  "*AxB",  "A",      true,       "",          "*AxB",        "A"},
    {  "*AxB",  "B",      true,       "XA",        "*AxB",        "A"},
    {  "*AxB",  "",       true,       "A",         "*AxB",        "A"},
    // Sync enabled, token error on both accounts.
    {  "*xAxB", "AB",     true,       "X",         "*xAxB",       ""},
    {  "*xAxB", "BA",     true,       "X",         "*xAxB",       ""},
    {  "*xAxB", "A",      true,       "X",         "*xAxB",       ""},
    {  "*xAxB", "B",      true,       "X",         "*xAxB",       ""},
    {  "*xAxB", "",       true,       "",          "*xAxB",       ""},
    // Sync disabled.
    {  "AB",    "AB",     true,       "",          "AB",          "AB"},
    {  "AB",    "BA",     true,       "",          "AB",          "BA"},
    {  "AB",    "A",      true,       "B",         "AB",          "AB"},
    {  "AB",    "B",      true,       "A",         "AB",          "BA"},
    {  "AB",    "",       true,       "AB",        "AB",          "AB"},
    // Sync disabled, token error on first account.
    {  "xAB",   "AB",     true,       "XB",        "xAB",         "B"},
    {  "xAB",   "BA",     true,       "XB",        "xAB",         "B"},
    {  "xAB",   "A",      true,       "XB",        "xAB",         "B"},
    {  "xAB",   "B",      true,       "",          "xAB",         "B"},
    {  "xAB",   "",       true,       "B",         "xAB",         "B"},
    // Sync disabled, token error on second account       .
    {  "AxB",   "AB",     true,       "XA",        "AxB",         "A"},
    {  "AxB",   "BA",     true,       "XA",        "AxB",         "A"},
    {  "AxB",   "A",      true,       "",          "AxB",         "A"},
    {  "AxB",   "B",      true,       "XA",        "AxB",         "A"},
    {  "AxB",   "",       true,       "A",         "AxB",         "A"},
    // Sync disabled, token error on both accounts.
    {  "xAxB",  "AB",     true,       "X",         "xAxB",        ""},
    {  "xAxB",  "BA",     true,       "X",         "xAxB",        ""},
    {  "xAxB",  "A",      true,       "X",         "xAxB",        ""},
    {  "xAxB",  "B",      true,       "X",         "xAxB",        ""},
    {  "xAxB",  "",       true,       "",          "xAxB",        ""},

    // Chrome is running: Do not change the order of accounts already present in
    // the Gaia cookies.
    // Sync enabled.
    {  "*AB",   "AB",     false,      "",          "*AB",         "AB"},
    {  "*AB",   "BA",     false,      "",          "*AB",         "BA"},
    {  "*AB",   "A",      false,      "B",         "*AB",         "AB"},
    {  "*AB",   "B",      false,      "A",         "*AB",         "BA"},
    {  "*AB",   "",       false,      "AB",        "*AB",         "AB"},
    // Sync enabled, token error on primary.
    {  "*xAB",  "AB",     false,      "X",         "*xA",         ""},
    {  "*xAB",  "BA",     false,      "XB",        "*xAB",        "B"},
    {  "*xAB",  "A",      false,      "X",         "*xA",         ""},
    {  "*xAB",  "B",      false,      "",          "*xAB",        "B"},
    {  "*xAB",  "",       false,      "B",         "*xAB",        "B"},
    // Sync enabled, token error on secondary.
    {  "*AxB",  "AB",     false,      "XA",        "*AxB",        "A"},
    {  "*AxB",  "BA",     false,      "XA",        "*AxB",        "A"},
    {  "*AxB",  "A",      false,      "",          "*AxB",        "A"},
    {  "*AxB",  "B",      false,      "XA",        "*AxB",        "A"},
    {  "*AxB",  "",       false,      "A",         "*AxB",        "A"},
    // Sync enabled, token error on both accounts.
    {  "*xAxB", "AB",     false,      "X",         "*xAxB",       ""},
    {  "*xAxB", "BA",     false,      "X",         "*xAxB",       ""},
    {  "*xAxB", "A",      false,      "X",         "*xAxB",       ""},
    {  "*xAxB", "B",      false,      "X",         "*xAxB",       ""},
    {  "*xAxB", "",       false,      "",          "*xAxB",       ""},
    // Sync disabled.
    {  "AB",    "AB",     false,      "",          "AB",          "AB"},
    {  "AB",    "BA",     false,      "",          "AB",          "BA"},
    {  "AB",    "A",      false,      "B",         "AB",          "AB"},
    {  "AB",    "B",      false,      "A",         "AB",          "BA"},
    {  "AB",    "",       false,      "AB",        "AB",          "AB"},
    // Sync disabled, token error on first account.
    {  "xAB",   "AB",     false,      "X",         "xA",          ""},
    {  "xAB",   "BA",     false,      "XB",        "xAB",         "B"},
    {  "xAB",   "A",      false,      "X",         "xA",          ""},
    {  "xAB",   "B",      false,      "",          "xAB",         "B"},
    {  "xAB",   "",       false,      "B",         "xAB",         "B"},
    // Sync disabled, token error on second account.
    {  "AxB",   "AB",     false,      "XA",        "AxB",         "A"},
    {  "AxB",   "BA",     false,      "X",         "xB",          ""},
    {  "AxB",   "A",      false,      "",          "AxB",         "A"},
    {  "AxB",   "B",      false,      "X",         "xB",          ""},
    {  "AxB",   "",       false,      "A",         "AxB",         "A"},
    // Sync disabled, token error on both accounts.
    {  "xAxB",  "AB",     false,      "X",         "xAxB",        ""},
    {  "xAxB",  "BA",     false,      "X",         "xAxB",        ""},
    {  "xAxB",  "A",      false,      "X",         "xAxB",        ""},
    {  "xAxB",  "B",      false,      "X",         "xAxB",        ""},
    {  "xAxB",  "",       false,      "",          "xAxB",        ""},

    // Miscellaneous cases.
    // Check that unknown Gaia accounts are signed out.
    {  "",      "A",      true,       "X",         "",            ""},
    {  "",      "A",      false,      "X",         "",            ""},
    {  "*A",    "AB",     true,       "XA",        "*A",          "A"},
    {  "*A",    "AB",     false,      "XA",        "*A",          "A"},
    // Check that Gaia default account is kept in first position.
    {  "AB",    "BC",     true,       "XBA",       "AB",          "BA"},
    {  "AB",    "BC",     false,      "XBA",       "AB",          "BA"},
    // Required for idempotency check.
    {  "",      "",       false,      "",          "",            ""},
    {  "*A",    "A",      false,      "",          "*A",          "A"},
    {  "xB",    "",       false,      "",          "xB",          ""},
    {  "xA",    "",       false,      "",          "xA",          ""},
    {  "*xA",   "",       false,      "",          "*xA",         ""},
    {  "*xAB",  "B",      false,      "",          "*xAB",        "B"},
};
// clang-format on

// Parameterized version of AccountReconcilorTest.
class AccountReconcilorTestDice
    : public AccountReconcilorTest,
      public ::testing::WithParamInterface<AccountReconcilorTestDiceParam> {
 protected:
  struct Account {
    std::string email;
    std::string gaia_id;
  };

  struct Token {
    std::string gaia_id;
    std::string email;
    bool is_authenticated;
    bool has_error;
  };

  AccountReconcilorTestDice() {
    accounts_['A'] = {"a@gmail.com", "A"};
    accounts_['B'] = {"b@gmail.com", "B"};
    accounts_['C'] = {"c@gmail.com", "C"};
  }

  // Build Tokens from string.
  std::vector<Token> ParseTokenString(const char* token_string) {
    std::vector<Token> parsed_tokens;
    bool is_authenticated = false;
    bool has_error = false;
    for (int i = 0; token_string[i] != '\0'; ++i) {
      char token_code = token_string[i];
      if (token_code == '*') {
        is_authenticated = true;
        continue;
      }
      if (token_code == 'x') {
        has_error = true;
        continue;
      }
      parsed_tokens.push_back({accounts_[token_code].gaia_id,
                               accounts_[token_code].email, is_authenticated,
                               has_error});
      is_authenticated = false;
      has_error = false;
    }
    return parsed_tokens;
  }

  // Checks that the tokens in the TokenService match the tokens.
  void VerifyCurrentTokens(const std::vector<Token>& tokens) {
    EXPECT_EQ(token_service()->GetAccounts().size(), tokens.size());
    bool authenticated_account_found = false;
    for (const Token& token : tokens) {
      std::string account_id =
          PickAccountIdForAccount(token.gaia_id, token.email);
      EXPECT_TRUE(token_service()->RefreshTokenIsAvailable(account_id));
      EXPECT_EQ(
          token.has_error,
          token_service()->GetDelegate()->RefreshTokenHasError(account_id));
      if (token.is_authenticated) {
        EXPECT_EQ(account_id, signin_manager()->GetAuthenticatedAccountId());
        authenticated_account_found = true;
      }
    }
    if (!authenticated_account_found)
      EXPECT_EQ("", signin_manager()->GetAuthenticatedAccountId());
  }

  // Checks that reconcile is idempotent.
  void CheckReconcileIdempotent(const AccountReconcilorTestDiceParam& param) {
    // Simulate another reconcile based on the results of this one: find the
    // corresponding row in the table and check that it does nothing.
    for (const AccountReconcilorTestDiceParam& row : kDiceParams) {
      if (row.is_first_reconcile)
        continue;
      if (strcmp(row.tokens, param.tokens_after_reconcile) != 0)
        continue;
      if (strcmp(row.cookies, param.cookies_after_reconcile) != 0)
        continue;
      EXPECT_STREQ(row.tokens, row.tokens_after_reconcile);
      EXPECT_STREQ(row.cookies, row.cookies_after_reconcile);
      return;
    }

    ADD_FAILURE() << "Could not check that reconcile is idempotent.";
  }

  std::map<char, Account> accounts_;
};

// Checks one row of the kDiceParams table above.
TEST_P(AccountReconcilorTestDice, TableRowTest) {
  // Enable Dice.
  signin::ScopedAccountConsistencyDice scoped_dice;

  // Check that reconcile is idempotent: when called twice in a row it should do
  // nothing on the second call.
  CheckReconcileIdempotent(GetParam());

  // Setup tokens.
  std::vector<Token> tokens_before_reconcile =
      ParseTokenString(GetParam().tokens);
  for (const Token& token : tokens_before_reconcile) {
    std::string account_id =
        PickAccountIdForAccount(token.gaia_id, token.email);
    if (token.is_authenticated)
      ConnectProfileToAccount(token.gaia_id, token.email);
    else
      token_service()->UpdateCredentials(account_id, "refresh_token");
    if (token.has_error) {
      token_service_delegate()->SetLastErrorForAccount(
          account_id, GoogleServiceAuthError(
                          GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
    }
  }
  VerifyCurrentTokens(tokens_before_reconcile);

  // Setup cookies.
  std::string cookies(GetParam().cookies);
  if (cookies.size() == 0) {
    cookie_manager_service()->SetListAccountsResponseNoAccounts();
  } else if (cookies.size() == 1) {
    cookie_manager_service()->SetListAccountsResponseOneAccount(
        accounts_[GetParam().cookies[0]].email.c_str(),
        accounts_[GetParam().cookies[0]].gaia_id.c_str());
  } else {
    ASSERT_EQ(2u, cookies.size());
    cookie_manager_service()->SetListAccountsResponseTwoAccounts(
        accounts_[GetParam().cookies[0]].email.c_str(),
        accounts_[GetParam().cookies[0]].gaia_id.c_str(),
        accounts_[GetParam().cookies[1]].email.c_str(),
        accounts_[GetParam().cookies[1]].gaia_id.c_str());
  }

  // Call list accounts now so that the next call completes synchronously.
  cookie_manager_service()->ListAccounts(nullptr, nullptr, "foo");
  base::RunLoop().RunUntilIdle();

  // Setup expectations.
  testing::InSequence mock_sequence;
  bool logout_action = false;
  for (int i = 0; GetParam().gaia_api_calls[i] != '\0'; ++i) {
    if (GetParam().gaia_api_calls[i] == 'X') {
      logout_action = true;
      EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction())
          .Times(1);
      cookies.clear();
      continue;
    }
    std::string cookie(1, GetParam().gaia_api_calls[i]);
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(cookie)).Times(1);
    cookies += cookie;
  }
  if (!logout_action) {
    EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction())
        .Times(0);
  }

  // Check the expected cookies after reconcile.
  ASSERT_EQ(GetParam().cookies_after_reconcile, cookies);

  // Reconcile.
  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor->first_execution_);
  reconcilor->first_execution_ = GetParam().is_first_reconcile;
  reconcilor->StartReconcile();
  for (int i = 0; GetParam().gaia_api_calls[i] != '\0'; ++i) {
    if (GetParam().gaia_api_calls[i] == 'X')
      continue;
    std::string account_id =
        PickAccountIdForAccount(accounts_[GetParam().gaia_api_calls[i]].gaia_id,
                                accounts_[GetParam().gaia_api_calls[i]].email);
    SimulateAddAccountToCookieCompleted(
        reconcilor, account_id, GoogleServiceAuthError::AuthErrorNone());
  }
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
  VerifyCurrentTokens(ParseTokenString(GetParam().tokens_after_reconcile));
  // Another reconcile is sometimes triggered if Chrome accounts have changed.
  // Allow it to finish.
  cookie_manager_service()->SetListAccountsResponseNoAccounts();
  cookie_manager_service()->set_list_accounts_stale_for_testing(true);
  base::RunLoop().RunUntilIdle();
}

INSTANTIATE_TEST_CASE_P(DiceTable,
                        AccountReconcilorTestDice,
                        ::testing::ValuesIn(kDiceParams));

// Tests that the AccountReconcilor is always registered.
TEST_F(AccountReconcilorTest, DiceTokenServiceRegistration) {
  signin::ScopedAccountConsistencyDice scoped_dice;
  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);
  ASSERT_TRUE(reconcilor->IsRegisteredWithTokenService());

  account_tracker()->SeedAccountInfo("12345", "user@gmail.com");
  signin_manager()->SignIn("12345", "user@gmail.com", "password");
  ASSERT_TRUE(reconcilor->IsRegisteredWithTokenService());

  // Reconcilor should not logout all accounts from the cookies when
  // SigninManager signs out.
  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction()).Times(0);

  signin_manager()->SignOut(signin_metrics::SIGNOUT_TEST,
                            signin_metrics::SignoutDelete::IGNORE_METRIC);
  ASSERT_TRUE(reconcilor->IsRegisteredWithTokenService());
}

// Tests that reconcile starts even when Sync is not enabled.
TEST_F(AccountReconcilorTest, DiceReconcileWhithoutSignin) {
  // Enable Dice.
  signin::ScopedAccountConsistencyDice scoped_dice;

  // Add a token in Chrome but do not sign in.
  const std::string account_id =
      PickAccountIdForAccount("12345", "user@gmail.com");
  token_service()->UpdateCredentials(account_id, "refresh_token");
  cookie_manager_service()->SetListAccountsResponseNoAccounts();

  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  SimulateAddAccountToCookieCompleted(reconcilor, account_id,
                                      GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
}

// Checks that nothing happens when there is no Chrome account and no Gaia
// cookie.
TEST_F(AccountReconcilorTest, DiceReconcileNoop) {
  // Enable Dice.
  signin::ScopedAccountConsistencyDice scoped_dice;

  // No Chrome account and no cookie.
  cookie_manager_service()->SetListAccountsResponseNoAccounts();
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(testing::_)).Times(0);
  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction()).Times(0);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
}

// Tests that the first Gaia account is re-used when possible.
TEST_F(AccountReconcilorTest, DiceReconcileReuseGaiaFirstAccount) {
  // Enable Dice.
  signin::ScopedAccountConsistencyDice scoped_dice;

  // Add accounts 1 and 2 to the token service.
  const std::string account_id_1 =
      PickAccountIdForAccount("12345", "user@gmail.com");
  const std::string account_id_2 =
      PickAccountIdForAccount("67890", "other@gmail.com");
  token_service()->UpdateCredentials(account_id_1, "refresh_token");
  token_service()->UpdateCredentials(account_id_2, "refresh_token");

  ASSERT_EQ(2u, token_service()->GetAccounts().size());
  ASSERT_EQ(account_id_1, token_service()->GetAccounts()[0]);
  ASSERT_EQ(account_id_2, token_service()->GetAccounts()[1]);

  // Add accounts 2 and 3 to the Gaia cookie.
  const std::string account_id_3 =
      PickAccountIdForAccount("9999", "foo@gmail.com");
  cookie_manager_service()->SetListAccountsResponseTwoAccounts(
      "other@gmail.com", "67890", "foo@gmail.com", "9999");

  testing::InSequence mock_sequence;
  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction());
  // Account 2 is added first.
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id_2));
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id_1));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  base::RunLoop().RunUntilIdle();
  SimulateAddAccountToCookieCompleted(reconcilor, account_id_1,
                                      GoogleServiceAuthError::AuthErrorNone());
  SimulateAddAccountToCookieCompleted(reconcilor, account_id_2,
                                      GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
}

// Tests that the first account is kept in cache and reused when cookies are
// lost.
TEST_F(AccountReconcilorTest, DiceLastKnownFirstAccount) {
  // Enable Dice.
  signin::ScopedAccountConsistencyDice scoped_dice;

  // Add accounts to the token service and the Gaia cookie in a different order.
  const std::string account_id_1 =
      PickAccountIdForAccount("12345", "user@gmail.com");
  const std::string account_id_2 =
      PickAccountIdForAccount("67890", "other@gmail.com");
  cookie_manager_service()->SetListAccountsResponseTwoAccounts(
      "other@gmail.com", "67890", "user@gmail.com", "12345");
  token_service()->UpdateCredentials(account_id_1, "refresh_token");
  token_service()->UpdateCredentials(account_id_2, "refresh_token");

  ASSERT_EQ(2u, token_service()->GetAccounts().size());
  ASSERT_EQ(account_id_1, token_service()->GetAccounts()[0]);
  ASSERT_EQ(account_id_2, token_service()->GetAccounts()[1]);

  // Do one reconcile. It should do nothing but to populating the last known
  // account.
  {
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(testing::_)).Times(0);
    EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction())
        .Times(0);

    AccountReconcilor* reconcilor = GetMockReconcilor();
    reconcilor->StartReconcile();
    ASSERT_TRUE(reconcilor->is_reconcile_started_);
    base::RunLoop().RunUntilIdle();
    ASSERT_FALSE(reconcilor->is_reconcile_started_);
    ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
  }

  // Delete the cookies.
  cookie_manager_service()->SetListAccountsResponseNoAccounts();
  cookie_manager_service()->set_list_accounts_stale_for_testing(true);

  // Reconcile again and check that account_id_2 is added first.
  {
    testing::InSequence mock_sequence;

    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id_2))
        .Times(1);
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id_1))
        .Times(1);
    EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction())
        .Times(0);

    AccountReconcilor* reconcilor = GetMockReconcilor();
    reconcilor->StartReconcile();
    ASSERT_TRUE(reconcilor->is_reconcile_started_);
    base::RunLoop().RunUntilIdle();
    SimulateAddAccountToCookieCompleted(
        reconcilor, account_id_2, GoogleServiceAuthError::AuthErrorNone());
    SimulateAddAccountToCookieCompleted(
        reconcilor, account_id_1, GoogleServiceAuthError::AuthErrorNone());
    ASSERT_FALSE(reconcilor->is_reconcile_started_);
    ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
  }
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// Tests that reconcile cannot start before the tokens are loaded, and is
// automatically started when tokens are loaded.
TEST_F(AccountReconcilorTest, TokensNotLoaded) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  cookie_manager_service()->SetListAccountsResponseNoAccounts();
  token_service()->set_all_credentials_loaded_for_testing(false);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();

#if !defined(OS_CHROMEOS)
  // No reconcile when tokens are not loaded, except on ChromeOS where reconcile
  // can start as long as the token service is not empty.
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  // When tokens are loaded, reconcile starts automatically.
  token_service()->LoadCredentials(account_id);
#endif

  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id));
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  base::RunLoop().RunUntilIdle();
  SimulateAddAccountToCookieCompleted(reconcilor, account_id,
                                      GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
}

TEST_F(AccountReconcilorTest, GetAccountsFromCookieSuccess) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  cookie_manager_service()->SetListAccountsResponseOneAccountWithExpiry(
      "user@gmail.com", "12345", true);
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id));

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
  reconcilor->StartReconcile();
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_RUNNING,
            reconcilor->GetState());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_RUNNING, reconcilor->GetState());

  std::vector<gaia::ListedAccount> accounts;
  std::vector<gaia::ListedAccount> signed_out_accounts;
  ASSERT_TRUE(cookie_manager_service()->ListAccounts(
      &accounts, &signed_out_accounts, GaiaConstants::kChromeSource));
  ASSERT_EQ(1u, accounts.size());
  ASSERT_EQ(account_id, accounts[0].id);
  ASSERT_EQ(0u, signed_out_accounts.size());
}

TEST_F(AccountReconcilorTest, GetAccountsFromCookieFailure) {
  ConnectProfileToAccount("12345", "user@gmail.com");
  cookie_manager_service()->SetListAccountsResponseWebLoginRequired();

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
  reconcilor->StartReconcile();
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_RUNNING,
            reconcilor->GetState());
  base::RunLoop().RunUntilIdle();

  std::vector<gaia::ListedAccount> accounts;
  std::vector<gaia::ListedAccount> signed_out_accounts;
  ASSERT_FALSE(cookie_manager_service()->ListAccounts(
      &accounts, &signed_out_accounts, GaiaConstants::kChromeSource));
  ASSERT_EQ(0u, accounts.size());
  ASSERT_EQ(0u, signed_out_accounts.size());

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_ERROR,
            reconcilor->GetState());
}

TEST_F(AccountReconcilorTest, StartReconcileNoop) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  cookie_manager_service()->SetListAccountsResponseOneAccount(
      "user@gmail.com", "12345");

  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  histogram_tester()->ExpectTotalCount(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun", 1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
      signin_metrics::ACCOUNTS_SAME,
      1);
}

TEST_F(AccountReconcilorTest, StartReconcileCookiesDisabled) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  token_service()->UpdateCredentials(account_id, "refresh_token");
  test_signin_client()->set_are_signin_cookies_allowed(false);

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  reconcilor->StartReconcile();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();
  std::vector<gaia::ListedAccount> accounts;
  // This will be the first call to ListAccounts.
  ASSERT_FALSE(cookie_manager_service()->ListAccounts(
      &accounts, nullptr, GaiaConstants::kChromeSource));
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorTest, StartReconcileContentSettings) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  token_service()->UpdateCredentials(account_id, "refresh_token");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  test_signin_client()->set_are_signin_cookies_allowed(false);
  SimulateCookieContentSettingsChanged(reconcilor,
                                       ContentSettingsPattern::Wildcard());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  test_signin_client()->set_are_signin_cookies_allowed(true);
  SimulateCookieContentSettingsChanged(reconcilor,
                                       ContentSettingsPattern::Wildcard());
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorTest, StartReconcileContentSettingsGaiaUrl) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  token_service()->UpdateCredentials(account_id, "refresh_token");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  SimulateCookieContentSettingsChanged(
      reconcilor,
      ContentSettingsPattern::FromURL(GaiaUrls::GetInstance()->gaia_url()));
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorTest, StartReconcileContentSettingsNonGaiaUrl) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  token_service()->UpdateCredentials(account_id, "refresh_token");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  SimulateCookieContentSettingsChanged(
      reconcilor,
      ContentSettingsPattern::FromURL(GURL("http://www.example.com")));
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorTest, StartReconcileContentSettingsInvalidPattern) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  token_service()->UpdateCredentials(account_id, "refresh_token");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  std::unique_ptr<ContentSettingsPattern::BuilderInterface> builder =
      ContentSettingsPattern::CreateBuilder();
  builder->Invalid();

  SimulateCookieContentSettingsChanged(reconcilor, builder->Build());
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
}

// This test is needed until chrome changes to use gaia obfuscated id.
// The signin manager and token service use the gaia "email" property, which
// preserves dots in usernames and preserves case. gaia::ParseListAccountsData()
// however uses gaia "displayEmail" which does not preserve case, and then
// passes the string through gaia::CanonicalizeEmail() which removes dots.  This
// tests makes sure that an email like "Dot.S@hmail.com", as seen by the
// token service, will be considered the same as "dots@gmail.com" as returned
// by gaia::ParseListAccountsData().
TEST_F(AccountReconcilorTest, StartReconcileNoopWithDots) {
  if (account_tracker()->GetMigrationState() !=
      AccountTrackerService::MIGRATION_NOT_STARTED) {
    return;
  }

  const std::string account_id =
      ConnectProfileToAccount("12345", "Dot.S@gmail.com");
  cookie_manager_service()->SetListAccountsResponseOneAccount(
      "dot.s@gmail.com", "12345");
  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  reconcilor->StartReconcile();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
      signin_metrics::ACCOUNTS_SAME,
      1);
}

TEST_F(AccountReconcilorTest, StartReconcileNoopMultiple) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  const std::string account_id2 =
      PickAccountIdForAccount("67890", "other@gmail.com");
  cookie_manager_service()->SetListAccountsResponseTwoAccounts(
      "user@gmail.com", "12345", "other@gmail.com", "67890");
  token_service()->UpdateCredentials(account_id2, "refresh_token");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  reconcilor->StartReconcile();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  histogram_tester()->ExpectTotalCount(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun", 1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
      signin_metrics::ACCOUNTS_SAME,
      1);
}

TEST_F(AccountReconcilorTest, StartReconcileAddToCookie) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  token_service()->UpdateCredentials(account_id, "refresh_token");
  cookie_manager_service()->SetListAccountsResponseOneAccount(
      "user@gmail.com", "12345");

  const std::string account_id2 =
      PickAccountIdForAccount("67890", "other@gmail.com");
  token_service()->UpdateCredentials(account_id2, "refresh_token");

  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id2));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  SimulateAddAccountToCookieCompleted(reconcilor, account_id2,
                                      GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
      signin_metrics::ACCOUNTS_SAME,
      1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.AddedToCookieJar.FirstRun", 1, 1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.RemovedFromCookieJar.FirstRun", 0, 1);

  base::HistogramTester::CountsMap expected_counts;
  expected_counts["Signin.Reconciler.Duration.Success"] = 1;
  EXPECT_THAT(histogram_tester()->GetTotalCountsForPrefix(
      "Signin.Reconciler.Duration.Success"),
      testing::ContainerEq(expected_counts));
}

#if !defined(OS_CHROMEOS)
// This test does not run on ChromeOS because it calls
// FakeSigninManagerForTesting::SignOut() which doesn't exist for ChromeOS.

TEST_F(AccountReconcilorTest, SignoutAfterErrorDoesNotRecordUma) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  token_service()->UpdateCredentials(account_id, "refresh_token");
  cookie_manager_service()->SetListAccountsResponseOneAccount(
      "user@gmail.com", "12345");

  const std::string account_id2 =
      PickAccountIdForAccount("67890", "other@gmail.com");
  token_service()->UpdateCredentials(account_id2, "refresh_token");

  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id2));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  GoogleServiceAuthError
    error(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  SimulateAddAccountToCookieCompleted(reconcilor, account_id2, error);
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction());
  signin_manager()->SignOut(signin_metrics::SIGNOUT_TEST,
                            signin_metrics::SignoutDelete::IGNORE_METRIC);

  base::HistogramTester::CountsMap expected_counts;
  expected_counts["Signin.Reconciler.Duration.Failure"] = 1;
  EXPECT_THAT(histogram_tester()->GetTotalCountsForPrefix(
      "Signin.Reconciler.Duration.Failure"),
      testing::ContainerEq(expected_counts));
}

#endif  // !defined(OS_CHROMEOS)

TEST_F(AccountReconcilorTest, StartReconcileRemoveFromCookie) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  token_service()->UpdateCredentials(account_id, "refresh_token");
  cookie_manager_service()->SetListAccountsResponseTwoAccounts(
      "user@gmail.com", "12345", "other@gmail.com", "67890");

  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction());
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();
  SimulateAddAccountToCookieCompleted(reconcilor, account_id,
                                      GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
      signin_metrics::ACCOUNTS_SAME,
      1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.AddedToCookieJar.FirstRun", 0, 1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.RemovedFromCookieJar.FirstRun", 1, 1);
}

TEST_F(AccountReconcilorTest, StartReconcileAddToCookieTwice) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  const std::string account_id2 =
      PickAccountIdForAccount("67890", "other@gmail.com");
  const std::string account_id3 =
      PickAccountIdForAccount("34567", "third@gmail.com");

  cookie_manager_service()->SetListAccountsResponseOneAccount(
      "user@gmail.com", "12345");
  token_service()->UpdateCredentials(account_id2, "refresh_token");

  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id2));
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id3));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  SimulateAddAccountToCookieCompleted(
      reconcilor, account_id2, GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
      signin_metrics::ACCOUNTS_SAME,
      1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.AddedToCookieJar.FirstRun", 1, 1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.RemovedFromCookieJar.FirstRun", 0, 1);

  // Do another pass after I've added a third account to the token service
  cookie_manager_service()->SetListAccountsResponseTwoAccounts(
      "user@gmail.com", "12345", "other@gmail.com", "67890");
  cookie_manager_service()->set_list_accounts_stale_for_testing(true);

  // This will cause the reconcilor to fire.
  token_service()->UpdateCredentials(account_id3, "refresh_token");
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  SimulateAddAccountToCookieCompleted(
      reconcilor, account_id3, GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
      signin_metrics::ACCOUNTS_SAME,
      1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.AddedToCookieJar.FirstRun", 1, 1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.RemovedFromCookieJar.FirstRun", 0, 1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.DifferentPrimaryAccounts.SubsequentRun",
      signin_metrics::ACCOUNTS_SAME,
      1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.AddedToCookieJar.SubsequentRun", 1, 1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.RemovedFromCookieJar.SubsequentRun", 0, 1);
}

TEST_F(AccountReconcilorTest, StartReconcileBadPrimary) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  const std::string account_id2 =
      PickAccountIdForAccount("67890", "other@gmail.com");

  token_service()->UpdateCredentials(account_id2, "refresh_token");
  cookie_manager_service()->SetListAccountsResponseTwoAccounts(
      "other@gmail.com", "67890", "user@gmail.com", "12345");

  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction());
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id));
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id2));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  SimulateAddAccountToCookieCompleted(reconcilor, account_id2,
                                      GoogleServiceAuthError::AuthErrorNone());
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  SimulateAddAccountToCookieCompleted(reconcilor, account_id,
                                      GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
      signin_metrics::COOKIE_AND_TOKEN_PRIMARIES_DIFFERENT,
      1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.AddedToCookieJar.FirstRun", 0, 1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.RemovedFromCookieJar.FirstRun", 0, 1);
}

TEST_F(AccountReconcilorTest, StartReconcileOnlyOnce) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  cookie_manager_service()->SetListAccountsResponseOneAccount(
      "user@gmail.com", "12345");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorTest, Lock) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  cookie_manager_service()->SetListAccountsResponseOneAccount("user@gmail.com",
                                                              "12345");
  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  EXPECT_EQ(0, reconcilor->account_reconcilor_lock_count_);

  class TestAccountReconcilorObserver : public AccountReconcilor::Observer {
   public:
    void OnStartReconcile() override { ++started_count_; }
    void OnBlockReconcile() override { ++blocked_count_; }
    void OnUnblockReconcile() override { ++unblocked_count_; }

    int started_count_ = 0;
    int blocked_count_ = 0;
    int unblocked_count_ = 0;
  };

  TestAccountReconcilorObserver observer;
  ScopedObserver<AccountReconcilor, AccountReconcilor::Observer>
      scoped_observer(&observer);
  scoped_observer.Add(reconcilor);

  // Lock prevents reconcile from starting, as long as one instance is alive.
  std::unique_ptr<AccountReconcilor::Lock> lock_1 =
      base::MakeUnique<AccountReconcilor::Lock>(reconcilor);
  EXPECT_EQ(1, reconcilor->account_reconcilor_lock_count_);
  reconcilor->StartReconcile();
  // lock_1 is blocking the reconcile.
  EXPECT_FALSE(reconcilor->is_reconcile_started_);
  {
    AccountReconcilor::Lock lock_2(reconcilor);
    EXPECT_EQ(2, reconcilor->account_reconcilor_lock_count_);
    EXPECT_FALSE(reconcilor->is_reconcile_started_);
    lock_1.reset();
    // lock_1 is no longer blocking, but lock_2 is still alive.
    EXPECT_EQ(1, reconcilor->account_reconcilor_lock_count_);
    EXPECT_FALSE(reconcilor->is_reconcile_started_);
    EXPECT_EQ(0, observer.started_count_);
    EXPECT_EQ(0, observer.unblocked_count_);
    EXPECT_EQ(1, observer.blocked_count_);
  }

  // All locks are deleted, reconcile starts.
  EXPECT_EQ(0, reconcilor->account_reconcilor_lock_count_);
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  EXPECT_EQ(1, observer.started_count_);
  EXPECT_EQ(1, observer.unblocked_count_);
  EXPECT_EQ(1, observer.blocked_count_);

  // Lock aborts current reconcile, and restarts it later.
  {
    AccountReconcilor::Lock lock(reconcilor);
    EXPECT_EQ(1, reconcilor->account_reconcilor_lock_count_);
    EXPECT_FALSE(reconcilor->is_reconcile_started_);
  }
  EXPECT_EQ(0, reconcilor->account_reconcilor_lock_count_);
  EXPECT_TRUE(reconcilor->is_reconcile_started_);
  EXPECT_EQ(2, observer.started_count_);
  EXPECT_EQ(2, observer.unblocked_count_);
  EXPECT_EQ(2, observer.blocked_count_);

  // Reconcile can complete successfully after being restarted.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorTest, StartReconcileWithSessionInfoExpiredDefault) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  const std::string account_id2 =
      PickAccountIdForAccount("67890", "other@gmail.com");
  token_service()->UpdateCredentials(account_id2, "refresh_token");
  cookie_manager_service()->SetListAccountsResponseTwoAccountsWithExpiry(
      "user@gmail.com", "12345", true, "other@gmail.com", "67890", false);

  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id));

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();
  SimulateAddAccountToCookieCompleted(reconcilor, account_id,
                                      GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorTest, AddAccountToCookieCompletedWithBogusAccount) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  cookie_manager_service()->SetListAccountsResponseOneAccountWithExpiry(
      "user@gmail.com", "12345", true);

  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id));

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();

  // If an unknown account id is sent, it should not upset the state.
  SimulateAddAccountToCookieCompleted(reconcilor, "bogus_account_id",
                                      GoogleServiceAuthError::AuthErrorNone());
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  SimulateAddAccountToCookieCompleted(reconcilor, account_id,
                                      GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorTest, NoLoopWithBadPrimary) {
  // Connect profile to a primary account and then add a secondary account.
  const std::string account_id1 =
  ConnectProfileToAccount("12345", "user@gmail.com");
  const std::string account_id2 =
      PickAccountIdForAccount("67890", "other@gmail.com");
  token_service()->UpdateCredentials(account_id2, "refresh_token");

  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction());
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id1));
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id2));

  // The primary account is in auth error, so it is not in the cookie.
  cookie_manager_service()->SetListAccountsResponseOneAccountWithExpiry(
      "other@gmail.com", "67890", true);

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  reconcilor->StartReconcile();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  GoogleServiceAuthError
      error(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);

  // The primary cannot be added to cookie, so it fails.
  SimulateAddAccountToCookieCompleted(
      reconcilor, account_id1, error);
  SimulateAddAccountToCookieCompleted(reconcilor, account_id2,
                                      GoogleServiceAuthError::AuthErrorNone());
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_TRUE(reconcilor->error_during_last_reconcile_);
  testing::Mock::VerifyAndClearExpectations(GetMockReconcilor());

  // Now that we've tried once, the token service knows that the primary
  // account has an auth error.
  token_service_delegate()->SetLastErrorForAccount(account_id1, error);

  // A second attempt to reconcile should be a noop.
  reconcilor->StartReconcile();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  testing::Mock::VerifyAndClearExpectations(GetMockReconcilor());
}

TEST_F(AccountReconcilorTest, WontMergeAccountsWithError) {
  // Connect profile to a primary account and then add a secondary account.
  const std::string account_id1 =
  ConnectProfileToAccount("12345", "user@gmail.com");
  const std::string account_id2 =
      PickAccountIdForAccount("67890", "other@gmail.com");
  token_service()->UpdateCredentials(account_id2, "refresh_token");

  // Mark the secondary account in auth error state.
  token_service_delegate()->SetLastErrorForAccount(
      account_id2,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  // The cookie starts empty.
  cookie_manager_service()->SetListAccountsResponseNoAccounts();

  // Since the cookie jar starts empty, the reconcilor should attempt to merge
  // accounts into it.  However, it should only try accounts not in auth
  // error state.
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id1));

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  reconcilor->StartReconcile();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  SimulateAddAccountToCookieCompleted(
      reconcilor, account_id1, GoogleServiceAuthError::AuthErrorNone());
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_FALSE(reconcilor->error_during_last_reconcile_);
}
