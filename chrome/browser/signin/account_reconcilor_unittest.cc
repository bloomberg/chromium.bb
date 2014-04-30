// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/fake_signin_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestEmail[] = "user@gmail.com";

class MockAccountReconcilor : public testing::StrictMock<AccountReconcilor> {
 public:
  static KeyedService* Build(content::BrowserContext* context);

  MockAccountReconcilor(ProfileOAuth2TokenService* token_service,
                        SigninManagerBase* signin_manager,
                        SigninClient* client);
  virtual ~MockAccountReconcilor() {}

  MOCK_METHOD1(PerformMergeAction, void(const std::string& account_id));
  MOCK_METHOD1(StartRemoveAction, void(const std::string& account_id));
  MOCK_METHOD3(
      FinishRemoveAction,
      void(const std::string& account_id,
           const GoogleServiceAuthError& error,
           const std::vector<std::pair<std::string, bool> >& accounts));
  MOCK_METHOD2(PerformAddToChromeAction, void(const std::string& account_id,
                                              int session_index));
  MOCK_METHOD0(PerformLogoutAllAccountsAction, void());
};

// static
KeyedService* MockAccountReconcilor::Build(content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  AccountReconcilor* reconcilor = new MockAccountReconcilor(
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      SigninManagerFactory::GetForProfile(profile),
      ChromeSigninClientFactory::GetForProfile(profile));
  reconcilor->Initialize(false /* start_reconcile_if_tokens_available */);
  return reconcilor;
}

MockAccountReconcilor::MockAccountReconcilor(
    ProfileOAuth2TokenService* token_service,
    SigninManagerBase* signin_manager,
    SigninClient* client)
    : testing::StrictMock<AccountReconcilor>(token_service,
                                             signin_manager,
                                             client) {}

}  // namespace

class AccountReconcilorTest : public testing::Test {
 public:
  AccountReconcilorTest();
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  TestingProfile* profile() { return profile_.get(); }
  FakeSigninManagerForTesting* signin_manager() { return signin_manager_; }
  FakeProfileOAuth2TokenService* token_service() { return token_service_; }

  void SetFakeResponse(const std::string& url,
                       const std::string& data,
                       net::HttpStatusCode code,
                       net::URLRequestStatus::Status status) {
    url_fetcher_factory_.SetFakeResponse(GURL(url), data, code, status);
  }

  MockAccountReconcilor* GetMockReconcilor();

  void SimulateMergeSessionCompleted(
      MergeSessionHelper::Observer* observer,
      const std::string& account_id,
      const GoogleServiceAuthError& error);

  void SimulateRefreshTokenFetched(
      AccountReconcilor* reconcilor,
      const std::string& account_id,
      const std::string& refresh_token);

 private:
  content::TestBrowserThreadBundle bundle_;
  scoped_ptr<TestingProfile> profile_;
  FakeSigninManagerForTesting* signin_manager_;
  FakeProfileOAuth2TokenService* token_service_;
  MockAccountReconcilor* mock_reconcilor_;
  net::FakeURLFetcherFactory url_fetcher_factory_;
};

AccountReconcilorTest::AccountReconcilorTest()
    : signin_manager_(NULL),
      token_service_(NULL),
      mock_reconcilor_(NULL),
      url_fetcher_factory_(NULL) {}

void AccountReconcilorTest::SetUp() {
  TestingProfile::Builder builder;
  builder.AddTestingFactory(ProfileOAuth2TokenServiceFactory::GetInstance(),
                            BuildFakeProfileOAuth2TokenService);
  builder.AddTestingFactory(SigninManagerFactory::GetInstance(),
                            FakeSigninManagerBase::Build);
  builder.AddTestingFactory(AccountReconcilorFactory::GetInstance(),
                            MockAccountReconcilor::Build);
  profile_ = builder.Build();

  signin_manager_ =
      static_cast<FakeSigninManagerForTesting*>(
          SigninManagerFactory::GetForProfile(profile()));

  token_service_ =
      static_cast<FakeProfileOAuth2TokenService*>(
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile()));
}

void AccountReconcilorTest::TearDown() {
  // Destroy the profile before all threads are torn down.
  profile_.reset();
}

MockAccountReconcilor* AccountReconcilorTest::GetMockReconcilor() {
  if (!mock_reconcilor_) {
    mock_reconcilor_ =
        static_cast<MockAccountReconcilor*>(
            AccountReconcilorFactory::GetForProfile(profile()));
  }

  return mock_reconcilor_;
}

void AccountReconcilorTest::SimulateMergeSessionCompleted(
    MergeSessionHelper::Observer* observer,
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  observer->MergeSessionCompleted(account_id, error);
}

void AccountReconcilorTest::SimulateRefreshTokenFetched(
    AccountReconcilor* reconcilor,
    const std::string& account_id,
    const std::string& refresh_token) {
  reconcilor->HandleRefreshTokenFetched(account_id, refresh_token);
}

TEST_F(AccountReconcilorTest, Basic) {
  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);
  ASSERT_EQ(token_service(), reconcilor->token_service());
}

#if !defined(OS_CHROMEOS)

TEST_F(AccountReconcilorTest, SigninManagerRegistration) {
  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);
  ASSERT_FALSE(reconcilor->IsPeriodicReconciliationRunning());
  ASSERT_FALSE(reconcilor->IsRegisteredWithTokenService());

  signin_manager()->OnExternalSigninCompleted(kTestEmail);
  ASSERT_TRUE(reconcilor->IsPeriodicReconciliationRunning());
  ASSERT_TRUE(reconcilor->IsRegisteredWithTokenService());

  signin_manager()->SignOut();
  ASSERT_FALSE(reconcilor->IsPeriodicReconciliationRunning());
  ASSERT_FALSE(reconcilor->IsRegisteredWithTokenService());
}

TEST_F(AccountReconcilorTest, Reauth) {
  signin_manager()->SetAuthenticatedUsername(kTestEmail);

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);
  ASSERT_TRUE(reconcilor->IsPeriodicReconciliationRunning());
  ASSERT_TRUE(reconcilor->IsRegisteredWithTokenService());

  // Simulate reauth.  The state of the reconcilor should not change.
  signin_manager()->OnExternalSigninCompleted(kTestEmail);
  ASSERT_TRUE(reconcilor->IsPeriodicReconciliationRunning());
  ASSERT_TRUE(reconcilor->IsRegisteredWithTokenService());
}

#endif  // !defined(OS_CHROMEOS)

TEST_F(AccountReconcilorTest, ProfileAlreadyConnected) {
  signin_manager()->SetAuthenticatedUsername(kTestEmail);

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);
  ASSERT_TRUE(reconcilor->IsPeriodicReconciliationRunning());
  ASSERT_TRUE(reconcilor->IsRegisteredWithTokenService());
}

TEST_F(AccountReconcilorTest, GetAccountsFromCookieSuccess) {
  signin_manager()->SetAuthenticatedUsername(kTestEmail);
  token_service()->UpdateCredentials(kTestEmail, "refresh_token");
  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  SetFakeResponse(GaiaUrls::GetInstance()->list_accounts_url().spec(),
      "[\"f\", [[\"b\", 0, \"n\", \"user@gmail.com\", \"p\", 0, 0, 0, 0, 0]]]",
      net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  reconcilor->StartReconcile();
  ASSERT_FALSE(reconcilor->AreGaiaAccountsSet());

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->AreGaiaAccountsSet());
  const std::vector<std::pair<std::string, bool> >& accounts =
      reconcilor->GetGaiaAccountsForTesting();
  ASSERT_EQ(1u, accounts.size());
  ASSERT_EQ("user@gmail.com", accounts[0].first);
}

TEST_F(AccountReconcilorTest, GetAccountsFromCookieFailure) {
  signin_manager()->SetAuthenticatedUsername(kTestEmail);
  token_service()->UpdateCredentials(kTestEmail, "refresh_token");
  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  SetFakeResponse(GaiaUrls::GetInstance()->list_accounts_url().spec(), "",
      net::HTTP_NOT_FOUND, net::URLRequestStatus::SUCCESS);

  reconcilor->StartReconcile();
  ASSERT_FALSE(reconcilor->AreGaiaAccountsSet());

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(0u, reconcilor->GetGaiaAccountsForTesting().size());
}

TEST_F(AccountReconcilorTest, ValidateAccountsFromTokens) {
  signin_manager()->SetAuthenticatedUsername(kTestEmail);
  token_service()->UpdateCredentials(kTestEmail, "refresh_token");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  reconcilor->ValidateAccountsFromTokenService();
  ASSERT_FALSE(reconcilor->AreAllRefreshTokensChecked());

  SetFakeResponse("https://www.googleapis.com/oauth2/v1/userinfo",
      "{\"id\":\"foo\"}", net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  token_service()->IssueTokenForAllPendingRequests("access_token",
      base::Time::Now() + base::TimeDelta::FromHours(1));

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->AreAllRefreshTokensChecked());
  ASSERT_EQ(1u, reconcilor->GetValidChromeAccountsForTesting().size());
  ASSERT_EQ(0u, reconcilor->GetInvalidChromeAccountsForTesting().size());
}

TEST_F(AccountReconcilorTest, ValidateAccountsFromTokensFailedUserInfo) {
  signin_manager()->SetAuthenticatedUsername(kTestEmail);
  token_service()->UpdateCredentials(kTestEmail, "refresh_token");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  reconcilor->ValidateAccountsFromTokenService();
  ASSERT_FALSE(reconcilor->AreAllRefreshTokensChecked());

  SetFakeResponse("https://www.googleapis.com/oauth2/v1/userinfo",
      "", net::HTTP_NOT_FOUND, net::URLRequestStatus::SUCCESS);
  token_service()->IssueTokenForAllPendingRequests("access_token",
      base::Time::Now() + base::TimeDelta::FromHours(1));

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->AreAllRefreshTokensChecked());
  ASSERT_EQ(0u, reconcilor->GetValidChromeAccountsForTesting().size());
  ASSERT_EQ(1u, reconcilor->GetInvalidChromeAccountsForTesting().size());
}

TEST_F(AccountReconcilorTest, ValidateAccountsFromTokensFailedTokenRequest) {
  signin_manager()->SetAuthenticatedUsername(kTestEmail);
  token_service()->UpdateCredentials(kTestEmail, "refresh_token");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  reconcilor->ValidateAccountsFromTokenService();
  ASSERT_FALSE(reconcilor->AreAllRefreshTokensChecked());

  token_service()->IssueErrorForAllPendingRequests(
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->AreAllRefreshTokensChecked());
  ASSERT_EQ(0u, reconcilor->GetValidChromeAccountsForTesting().size());
  ASSERT_EQ(1u, reconcilor->GetInvalidChromeAccountsForTesting().size());
}

TEST_F(AccountReconcilorTest, StartReconcileNoop) {
  signin_manager()->SetAuthenticatedUsername(kTestEmail);
  token_service()->UpdateCredentials(kTestEmail, "refresh_token");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  SetFakeResponse(GaiaUrls::GetInstance()->list_accounts_url().spec(),
      "[\"f\", [[\"b\", 0, \"n\", \"user@gmail.com\", \"p\", 0, 0, 0, 0, 1]]]",
      net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  SetFakeResponse("https://www.googleapis.com/oauth2/v1/userinfo",
      "{\"id\":\"foo\"}", net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  reconcilor->StartReconcile();
  ASSERT_FALSE(reconcilor->AreGaiaAccountsSet());
  ASSERT_FALSE(reconcilor->AreAllRefreshTokensChecked());

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->AreGaiaAccountsSet());
  ASSERT_EQ(1u, reconcilor->GetGaiaAccountsForTesting().size());
  ASSERT_FALSE(reconcilor->AreAllRefreshTokensChecked());

  token_service()->IssueAllTokensForAccount("user@gmail.com", "access_token",
      base::Time::Now() + base::TimeDelta::FromHours(1));

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->AreAllRefreshTokensChecked());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

// This is test is needed until chrome changes to use gaia obfuscated id.
// The signin manager and token service use the gaia "email" property, which
// preserves dots in usernames and preserves case. gaia::ParseListAccountsData()
// however uses gaia "displayEmail" which does not preserve case, and then
// passes the string through gaia::CanonicalizeEmail() which removes dots.  This
// tests makes sure that an email like "Dot.S@hmail.com", as seen by the
// token service, will be considered the same as "dots@gmail.com" as returned
// by gaia::ParseListAccountsData().
TEST_F(AccountReconcilorTest, StartReconcileNoopWithDots) {
  signin_manager()->SetAuthenticatedUsername("Dot.S@gmail.com");
  token_service()->UpdateCredentials("Dot.S@gmail.com", "refresh_token");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  SetFakeResponse(GaiaUrls::GetInstance()->list_accounts_url().spec(),
      "[\"f\", [[\"b\", 0, \"n\", \"dot.s@gmail.com\", \"p\", 0, 0, 0, 0, 1]]]",
      net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  SetFakeResponse("https://www.googleapis.com/oauth2/v1/userinfo",
      "{\"id\":\"foo\"}", net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  reconcilor->StartReconcile();
  ASSERT_FALSE(reconcilor->AreGaiaAccountsSet());
  ASSERT_FALSE(reconcilor->AreAllRefreshTokensChecked());

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->AreGaiaAccountsSet());
  ASSERT_EQ(1u, reconcilor->GetGaiaAccountsForTesting().size());
  ASSERT_STREQ("dots@gmail.com",
               reconcilor->GetGaiaAccountsForTesting()[0].first.c_str());
  ASSERT_FALSE(reconcilor->AreAllRefreshTokensChecked());

  token_service()->IssueAllTokensForAccount("Dot.S@gmail.com", "access_token",
      base::Time::Now() + base::TimeDelta::FromHours(1));

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->AreAllRefreshTokensChecked());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorTest, StartReconcileNoopMultiple) {
  signin_manager()->SetAuthenticatedUsername("user@gmail.com");
  token_service()->UpdateCredentials("user@gmail.com", "refresh_token");
  token_service()->UpdateCredentials("other@gmail.com", "refresh_token");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  SetFakeResponse(GaiaUrls::GetInstance()->list_accounts_url().spec(),
      "[\"f\", [[\"b\", 0, \"n\", \"user@gmail.com\", \"p\", 0, 0, 0, 0, 1], "
               "[\"b\", 0, \"n\", \"other@gmail.com\", \"p\", 0, 0, 0, 0, 1]]]",
      net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  SetFakeResponse("https://www.googleapis.com/oauth2/v1/userinfo",
      "{\"id\":\"foo\"}", net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  reconcilor->StartReconcile();
  ASSERT_FALSE(reconcilor->AreGaiaAccountsSet());
  ASSERT_FALSE(reconcilor->AreAllRefreshTokensChecked());

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->AreGaiaAccountsSet());
  ASSERT_FALSE(reconcilor->AreAllRefreshTokensChecked());
  ASSERT_EQ(2u, reconcilor->GetGaiaAccountsForTesting().size());

  token_service()->IssueAllTokensForAccount("other@gmail.com", "access_token",
      base::Time::Now() + base::TimeDelta::FromHours(1));

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->AreAllRefreshTokensChecked());

  token_service()->IssueAllTokensForAccount("user@gmail.com", "access_token",
      base::Time::Now() + base::TimeDelta::FromHours(1));

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->AreAllRefreshTokensChecked());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorTest, StartReconcileAddToCookie) {
  signin_manager()->SetAuthenticatedUsername("user@gmail.com");
  token_service()->UpdateCredentials("user@gmail.com", "refresh_token");
  token_service()->UpdateCredentials("other@gmail.com", "refresh_token");

  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction("other@gmail.com"));

  SetFakeResponse(GaiaUrls::GetInstance()->list_accounts_url().spec(),
      "[\"f\", [[\"b\", 0, \"n\", \"user@gmail.com\", \"p\", 0, 0, 0, 0, 1]]]",
      net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  SetFakeResponse("https://www.googleapis.com/oauth2/v1/userinfo",
      "{\"id\":\"foo\"}", net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();
  token_service()->IssueAllTokensForAccount("other@gmail.com", "access_token",
      base::Time::Now() + base::TimeDelta::FromHours(1));
  token_service()->IssueAllTokensForAccount("user@gmail.com", "access_token",
      base::Time::Now() + base::TimeDelta::FromHours(1));

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  SimulateMergeSessionCompleted(reconcilor, "other@gmail.com",
                                GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorTest, StartReconcileAddToChrome) {
  signin_manager()->SetAuthenticatedUsername("user@gmail.com");
  token_service()->UpdateCredentials("user@gmail.com", "refresh_token");

  EXPECT_CALL(*GetMockReconcilor(),
              PerformAddToChromeAction("other@gmail.com", 1));

  SetFakeResponse(GaiaUrls::GetInstance()->list_accounts_url().spec(),
      "[\"f\", [[\"b\", 0, \"n\", \"user@gmail.com\", \"p\", 0, 0, 0, 0, 1], "
               "[\"b\", 0, \"n\", \"other@gmail.com\", \"p\", 0, 0, 0, 0, 1]]]",
      net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  SetFakeResponse("https://www.googleapis.com/oauth2/v1/userinfo",
      "{\"id\":\"foo\"}", net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();
  token_service()->IssueAllTokensForAccount("user@gmail.com", "access_token",
      base::Time::Now() + base::TimeDelta::FromHours(1));

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  SimulateRefreshTokenFetched(reconcilor, "other@gmail.com", "");
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorTest, StartReconcileBadPrimary) {
  signin_manager()->SetAuthenticatedUsername("user@gmail.com");
  token_service()->UpdateCredentials("user@gmail.com", "refresh_token");
  token_service()->UpdateCredentials("other@gmail.com", "refresh_token");

  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction());
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction("user@gmail.com"));
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction("other@gmail.com"));

  SetFakeResponse(GaiaUrls::GetInstance()->list_accounts_url().spec(),
      "[\"f\", [[\"b\", 0, \"n\", \"other@gmail.com\", \"p\", 0, 0, 0, 0, 1], "
               "[\"b\", 0, \"n\", \"user@gmail.com\", \"p\", 0, 0, 0, 0, 1]]]",
      net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  SetFakeResponse("https://www.googleapis.com/oauth2/v1/userinfo",
      "{\"id\":\"foo\"}", net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();
  token_service()->IssueAllTokensForAccount("other@gmail.com", "access_token",
      base::Time::Now() + base::TimeDelta::FromHours(1));
  token_service()->IssueAllTokensForAccount("user@gmail.com", "access_token",
      base::Time::Now() + base::TimeDelta::FromHours(1));

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  SimulateMergeSessionCompleted(reconcilor, "other@gmail.com",
                                GoogleServiceAuthError::AuthErrorNone());
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  SimulateMergeSessionCompleted(reconcilor, "user@gmail.com",
                                GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorTest, StartReconcileOnlyOnce) {
  signin_manager()->SetAuthenticatedUsername(kTestEmail);
  token_service()->UpdateCredentials(kTestEmail, "refresh_token");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  SetFakeResponse(GaiaUrls::GetInstance()->list_accounts_url().spec(),
      "[\"f\", [[\"b\", 0, \"n\", \"user@gmail.com\", \"p\", 0, 0, 0, 0, 1]]]",
      net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  SetFakeResponse("https://www.googleapis.com/oauth2/v1/userinfo",
      "{\"id\":\"foo\"}", net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  token_service()->IssueAllTokensForAccount("user@gmail.com", "access_token",
      base::Time::Now() + base::TimeDelta::FromHours(1));

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorTest, StartReconcileWithSessionInfoExpiredDefault) {
  signin_manager()->SetAuthenticatedUsername("user@gmail.com");
  token_service()->UpdateCredentials("user@gmail.com", "refresh_token");
  token_service()->UpdateCredentials("other@gmail.com", "refresh_token");

  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction("user@gmail.com"));

  SetFakeResponse(GaiaUrls::GetInstance()->list_accounts_url().spec(),
      "[\"f\", [[\"b\", 0, \"n\", \"user@gmail.com\", \"p\", 0, 0, 0, 0, 0],"
               "[\"b\", 0, \"n\", \"other@gmail.com\", \"p\", 0, 0, 0, 0, 1]]]",
      net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  SetFakeResponse("https://www.googleapis.com/oauth2/v1/userinfo",
      "{\"id\":\"foo\"}", net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  token_service()->IssueAllTokensForAccount("user@gmail.com", "access_token",
      base::Time::Now() + base::TimeDelta::FromHours(1));
  token_service()->IssueAllTokensForAccount("other@gmail.com", "access_token",
      base::Time::Now() + base::TimeDelta::FromHours(1));

  base::RunLoop().RunUntilIdle();
  SimulateMergeSessionCompleted(reconcilor, "user@gmail.com",
                                GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}
