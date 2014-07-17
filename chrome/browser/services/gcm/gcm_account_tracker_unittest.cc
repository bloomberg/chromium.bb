// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/gcm_account_tracker.h"

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "google_apis/gaia/fake_identity_provider.h"
#include "google_apis/gaia/fake_oauth2_token_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

const char kAccountId1[] = "account_1";
const char kAccountId2[] = "account_2";

std::string AccountKeyToObfuscatedId(const std::string email) {
  return "obfid-" + email;
}

std::string GetValidTokenInfoResponse(const std::string account_key) {
  return std::string("{ \"id\": \"") + AccountKeyToObfuscatedId(account_key) +
         "\" }";
}

std::string MakeAccessToken(const std::string& account_key) {
  return "access_token-" + account_key;
}

}  // namespace

class GCMAccountTrackerTest : public testing::Test {
 public:
  GCMAccountTrackerTest();
  virtual ~GCMAccountTrackerTest();

  // Callback for the account tracker.
  void UpdateAccounts(const std::map<std::string, std::string>& accounts);

  // Helpers to pass fake events to the tracker. Tests should have either a pair
  // of Start/FinishAccountSignIn or SignInAccount per account. Don't mix.
  // Call to SignOutAccount is not mandatory.
  void StartAccountSignIn(const std::string& account_key);
  void FinishAccountSignIn(const std::string& account_key);
  void SignInAccount(const std::string& account_key);
  void SignOutAccount(const std::string& account_key);

  // Helpers for dealing with OAuth2 access token requests.
  void IssueAccessToken(const std::string& account_key);
  void IssueError(const std::string& account_key);

  // Test results and helpers.
  void ResetResults();
  bool update_accounts_called() const { return update_accounts_called_; }
  const std::map<std::string, std::string>& accounts() const {
    return accounts_;
  }

  // Accessor to account tracker.
  GCMAccountTracker* tracker() { return tracker_.get(); }

 private:
  std::map<std::string, std::string> accounts_;
  bool update_accounts_called_;

  base::MessageLoop message_loop_;
  net::TestURLFetcherFactory test_fetcher_factory_;
  scoped_ptr<FakeOAuth2TokenService> fake_token_service_;
  scoped_ptr<FakeIdentityProvider> fake_identity_provider_;
  scoped_ptr<GCMAccountTracker> tracker_;
};

GCMAccountTrackerTest::GCMAccountTrackerTest()
    : update_accounts_called_(false) {
  fake_token_service_.reset(new FakeOAuth2TokenService());

  fake_identity_provider_.reset(
      new FakeIdentityProvider(fake_token_service_.get()));

  scoped_ptr<gaia::AccountTracker> gaia_account_tracker(
      new gaia::AccountTracker(fake_identity_provider_.get(),
                               new net::TestURLRequestContextGetter(
                                   message_loop_.message_loop_proxy())));

  tracker_.reset(new GCMAccountTracker(
      gaia_account_tracker.Pass(),
      base::Bind(&GCMAccountTrackerTest::UpdateAccounts,
                 base::Unretained(this))));
}

GCMAccountTrackerTest::~GCMAccountTrackerTest() {
  if (tracker_)
    tracker_->Shutdown();
}

void GCMAccountTrackerTest::UpdateAccounts(
    const std::map<std::string, std::string>& accounts) {
  update_accounts_called_ = true;
  accounts_ = accounts;
}

void GCMAccountTrackerTest::ResetResults() {
  accounts_.clear();
  update_accounts_called_ = false;
}

void GCMAccountTrackerTest::StartAccountSignIn(const std::string& account_key) {
  fake_identity_provider_->LogIn(account_key);
  fake_token_service_->AddAccount(account_key);
}

void GCMAccountTrackerTest::FinishAccountSignIn(
    const std::string& account_key) {
  IssueAccessToken(account_key);

  net::TestURLFetcher* fetcher = test_fetcher_factory_.GetFetcherByID(
      gaia::GaiaOAuthClient::kUrlFetcherId);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenInfoResponse(account_key));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

void GCMAccountTrackerTest::SignInAccount(const std::string& account_key) {
  StartAccountSignIn(account_key);
  FinishAccountSignIn(account_key);
}

void GCMAccountTrackerTest::SignOutAccount(const std::string& account_key) {
  fake_token_service_->RemoveAccount(account_key);
}

void GCMAccountTrackerTest::IssueAccessToken(const std::string& account_key) {
  fake_token_service_->IssueAllTokensForAccount(
      account_key, MakeAccessToken(account_key), base::Time::Max());
}

void GCMAccountTrackerTest::IssueError(const std::string& account_key) {
  fake_token_service_->IssueErrorForAllPendingRequestsForAccount(
      account_key,
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
}

TEST_F(GCMAccountTrackerTest, NoAccounts) {
  EXPECT_FALSE(update_accounts_called());
  tracker()->Start();
  // Callback should not be called if there where no accounts provided.
  EXPECT_FALSE(update_accounts_called());
  EXPECT_TRUE(accounts().empty());
  tracker()->Stop();
}

// Verifies that callback is called after a token is issued for a single account
// with a specific scope. In this scenario, the underlying account tracker is
// still working when the CompleteCollectingTokens is called for the first time.
TEST_F(GCMAccountTrackerTest, SingleAccount) {
  StartAccountSignIn(kAccountId1);

  tracker()->Start();
  // We don't have any accounts to report, but given the inner account tracker
  // is still working we don't make a call with empty accounts list.
  EXPECT_FALSE(update_accounts_called());

  // This concludes the work of inner account tracker.
  FinishAccountSignIn(kAccountId1);
  IssueAccessToken(kAccountId1);

  EXPECT_TRUE(update_accounts_called());

  std::map<std::string, std::string> expected_accounts;
  expected_accounts[kAccountId1] = MakeAccessToken(kAccountId1);
  EXPECT_EQ(expected_accounts, accounts());
  tracker()->Stop();
}

TEST_F(GCMAccountTrackerTest, MultipleAccounts) {
  StartAccountSignIn(kAccountId1);
  StartAccountSignIn(kAccountId2);

  tracker()->Start();
  EXPECT_FALSE(update_accounts_called());

  FinishAccountSignIn(kAccountId1);
  IssueAccessToken(kAccountId1);
  EXPECT_FALSE(update_accounts_called());

  FinishAccountSignIn(kAccountId2);
  IssueAccessToken(kAccountId2);
  EXPECT_TRUE(update_accounts_called());

  std::map<std::string, std::string> expected_accounts;
  expected_accounts[kAccountId1] = MakeAccessToken(kAccountId1);
  expected_accounts[kAccountId2] = MakeAccessToken(kAccountId2);
  EXPECT_EQ(expected_accounts, accounts());

  tracker()->Stop();
}

TEST_F(GCMAccountTrackerTest, AccountAdded) {
  tracker()->Start();
  ResetResults();

  SignInAccount(kAccountId1);
  EXPECT_FALSE(update_accounts_called());

  IssueAccessToken(kAccountId1);
  EXPECT_TRUE(update_accounts_called());

  std::map<std::string, std::string> expected_accounts;
  expected_accounts[kAccountId1] = MakeAccessToken(kAccountId1);
  EXPECT_EQ(expected_accounts, accounts());

  tracker()->Stop();
}

TEST_F(GCMAccountTrackerTest, AccountRemoved) {
  SignInAccount(kAccountId1);
  SignInAccount(kAccountId2);

  tracker()->Start();
  IssueAccessToken(kAccountId1);
  IssueAccessToken(kAccountId2);
  EXPECT_TRUE(update_accounts_called());

  ResetResults();
  EXPECT_FALSE(update_accounts_called());

  SignOutAccount(kAccountId2);
  EXPECT_TRUE(update_accounts_called());

  std::map<std::string, std::string> expected_accounts;
  expected_accounts[kAccountId1] = MakeAccessToken(kAccountId1);
  EXPECT_EQ(expected_accounts, accounts());

  tracker()->Stop();
}

TEST_F(GCMAccountTrackerTest, GetTokenFailed) {
  SignInAccount(kAccountId1);
  SignInAccount(kAccountId2);

  tracker()->Start();
  IssueAccessToken(kAccountId1);
  EXPECT_FALSE(update_accounts_called());

  IssueError(kAccountId2);
  EXPECT_TRUE(update_accounts_called());

  std::map<std::string, std::string> expected_accounts;
  expected_accounts[kAccountId1] = MakeAccessToken(kAccountId1);
  EXPECT_EQ(expected_accounts, accounts());

  tracker()->Stop();
}

TEST_F(GCMAccountTrackerTest, GetTokenFailedAccountRemoved) {
  SignInAccount(kAccountId1);
  SignInAccount(kAccountId2);

  tracker()->Start();
  IssueAccessToken(kAccountId1);
  IssueError(kAccountId2);

  ResetResults();
  SignOutAccount(kAccountId2);
  EXPECT_TRUE(update_accounts_called());

  std::map<std::string, std::string> expected_accounts;
  expected_accounts[kAccountId1] = MakeAccessToken(kAccountId1);
  EXPECT_EQ(expected_accounts, accounts());

  tracker()->Stop();
}

TEST_F(GCMAccountTrackerTest, AccountRemovedWhileRequestsPending) {
  SignInAccount(kAccountId1);
  SignInAccount(kAccountId2);

  tracker()->Start();
  IssueAccessToken(kAccountId1);
  EXPECT_FALSE(update_accounts_called());

  SignOutAccount(kAccountId2);
  IssueAccessToken(kAccountId2);
  EXPECT_TRUE(update_accounts_called());

  std::map<std::string, std::string> expected_accounts;
  expected_accounts[kAccountId1] = MakeAccessToken(kAccountId1);
  EXPECT_EQ(expected_accounts, accounts());

  tracker()->Stop();
}

// TODO(fgorski): Add test for adding account after removal >> make sure it does
// not mark removal.

}  // namespace gcm
