// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/signin/account_reconcilor.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/fake_signin_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

#if defined(OS_CHROMEOS)
typedef FakeSigninManagerBase FakeSigninManagerForTesting;
#else
typedef FakeSigninManager FakeSigninManagerForTesting;
#endif

const char kTestEmail[] = "user@gmail.com";

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

private:
  content::TestBrowserThreadBundle bundle_;
  scoped_ptr<TestingProfile> profile_;
  FakeSigninManagerForTesting* signin_manager_;
  FakeProfileOAuth2TokenService* token_service_;
  net::FakeURLFetcherFactory url_fetcher_factory_;
};

AccountReconcilorTest::AccountReconcilorTest()
    : signin_manager_(NULL),
      token_service_(NULL),
      url_fetcher_factory_(NULL) {}

void AccountReconcilorTest::SetUp() {
  TestingProfile::Builder builder;
  builder.AddTestingFactory(ProfileOAuth2TokenServiceFactory::GetInstance(),
                            FakeProfileOAuth2TokenService::Build);
  builder.AddTestingFactory(SigninManagerFactory::GetInstance(),
                            FakeSigninManagerForTesting::Build);
  profile_ = builder.Build();

  signin_manager_ =
      static_cast<FakeSigninManagerForTesting*>(
          SigninManagerFactory::GetForProfile(profile()));
  signin_manager_->Initialize(profile(), NULL);

  token_service_ =
      static_cast<FakeProfileOAuth2TokenService*>(
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile()));
}

void AccountReconcilorTest::TearDown() {
  // Destroy the profile before all threads are torn down.
  profile_.reset();
}

}  // namespace

TEST_F(AccountReconcilorTest, Basic) {
  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);
  ASSERT_EQ(profile(), reconcilor->profile());
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
  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  SetFakeResponse("https://accounts.google.com/ListAccounts",
      "[\"foo\", [[\"bar\", 0, \"name\", \"email\", \"photo\", 0, 0, 0]]]",
      net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  reconcilor->GetAccountsFromCookie();
  ASSERT_FALSE(reconcilor->AreGaiaAccountsSet());

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->AreGaiaAccountsSet());
  const std::vector<std::string>& accounts =
      reconcilor->GetGaiaAccountsForTesting();
  ASSERT_EQ(1u, accounts.size());
  ASSERT_EQ("email", accounts[0]);
}

TEST_F(AccountReconcilorTest, GetAccountsFromCookieFailure) {
  signin_manager()->SetAuthenticatedUsername(kTestEmail);
  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  SetFakeResponse("https://accounts.google.com/ListAccounts", "",
      net::HTTP_NOT_FOUND, net::URLRequestStatus::SUCCESS);

  reconcilor->GetAccountsFromCookie();
  ASSERT_FALSE(reconcilor->AreGaiaAccountsSet());

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->AreGaiaAccountsSet());
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

TEST_F(AccountReconcilorTest, StartReconcileAction) {
  signin_manager()->SetAuthenticatedUsername(kTestEmail);
  token_service()->UpdateCredentials(kTestEmail, "refresh_token");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  SetFakeResponse("https://accounts.google.com/ListAccounts",
      "[\"foo\", [[\"b\", 0, \"n\", \"user@gmail.com\", \"p\", 0, 0, 0], "
                 "[\"b\", 0, \"n\", \"other@gmail.com\", \"p\", 0, 0, 0]]]",
      net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  reconcilor->StartReconcileAction();
  ASSERT_FALSE(reconcilor->AreGaiaAccountsSet());
  ASSERT_FALSE(reconcilor->AreAllRefreshTokensChecked());

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->AreGaiaAccountsSet());
  ASSERT_FALSE(reconcilor->AreAllRefreshTokensChecked());
  ASSERT_EQ(2u, reconcilor->GetGaiaAccountsForTesting().size());

  SetFakeResponse("https://www.googleapis.com/oauth2/v1/userinfo",
      "", net::HTTP_NOT_FOUND, net::URLRequestStatus::SUCCESS);
  token_service()->IssueAllTokensForAccount("other@gmail.com", "access_token",
      base::Time::Now() + base::TimeDelta::FromHours(1));

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->AreAllRefreshTokensChecked());

  token_service()->IssueAllTokensForAccount("user@gmail.com", "access_token",
      base::Time::Now() + base::TimeDelta::FromHours(1));

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->AreAllRefreshTokensChecked());
}

