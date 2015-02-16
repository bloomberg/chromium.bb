// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/ios/browser/profile_oauth2_token_service_ios.h"

#include "base/run_loop.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_token_service_test_util.h"
#include "ios/public/test/fake_profile_oauth2_token_service_ios_provider.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProfileOAuth2TokenServiceIOSTest : public testing::Test,
                                         public OAuth2TokenService::Consumer,
                                         public OAuth2TokenService::Observer {
 public:
  ProfileOAuth2TokenServiceIOSTest()
      : OAuth2TokenService::Consumer("test_consumer_id"),
        factory_(NULL),
        client_(&prefs_),
        token_available_count_(0),
        token_revoked_count_(0),
        tokens_loaded_count_(0),
        access_token_success_(0),
        access_token_failure_(0),
        last_access_token_error_(GoogleServiceAuthError::NONE) {}

  void SetUp() override {
    prefs_.registry()->RegisterBooleanPref(
        prefs::kTokenServiceExcludeAllSecondaryAccounts, false);
    prefs_.registry()->RegisterListPref(
        prefs::kTokenServiceExcludedSecondaryAccounts);

    factory_.SetFakeResponse(GaiaUrls::GetInstance()->oauth2_revoke_url(),
                             "",
                             net::HTTP_OK,
                             net::URLRequestStatus::SUCCESS);
    fake_provider_ = client_.GetIOSProviderAsFake();
    oauth2_service_.Initialize(&client_, &signin_error_controller_);
    oauth2_service_.AddObserver(this);
  }

  void TearDown() override {
    oauth2_service_.RemoveObserver(this);
    oauth2_service_.Shutdown();
  }

  // OAuth2TokenService::Consumer implementation.
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override {
    ++access_token_success_;
  }

  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override {
    ++access_token_failure_;
    last_access_token_error_ = error;
  };

  // OAuth2TokenService::Observer implementation.
  void OnRefreshTokenAvailable(const std::string& account_id) override {
    ++token_available_count_;
  }
  void OnRefreshTokenRevoked(const std::string& account_id) override {
    ++token_revoked_count_;
  }
  void OnRefreshTokensLoaded() override { ++tokens_loaded_count_; }

  void ResetObserverCounts() {
    token_available_count_ = 0;
    token_revoked_count_ = 0;
    tokens_loaded_count_ = 0;
    token_available_count_ = 0;
    access_token_failure_ = 0;
  }

 protected:
  base::MessageLoop message_loop_;
  net::FakeURLFetcherFactory factory_;
  TestingPrefServiceSimple prefs_;
  TestSigninClient client_;
  SigninErrorController signin_error_controller_;
  ios::FakeProfileOAuth2TokenServiceIOSProvider* fake_provider_;
  ProfileOAuth2TokenServiceIOS oauth2_service_;
  TestingOAuth2TokenServiceConsumer consumer_;
  int token_available_count_;
  int token_revoked_count_;
  int tokens_loaded_count_;
  int access_token_success_;
  int access_token_failure_;
  GoogleServiceAuthError last_access_token_error_;
};

TEST_F(ProfileOAuth2TokenServiceIOSTest, LoadRevokeCredentialsOneAccount) {
  fake_provider_->AddAccount("account_id");
  oauth2_service_.LoadCredentials("account_id");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, token_available_count_);
  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(1U, oauth2_service_.GetAccounts().size());
  EXPECT_TRUE(oauth2_service_.RefreshTokenIsAvailable("account_id"));

  ResetObserverCounts();
  oauth2_service_.RevokeAllCredentials();
  EXPECT_EQ(0, token_available_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(1, token_revoked_count_);
  EXPECT_EQ(0U, oauth2_service_.GetAccounts().size());
  EXPECT_FALSE(oauth2_service_.RefreshTokenIsAvailable("account_id_1"));
}

TEST_F(ProfileOAuth2TokenServiceIOSTest,
       LoadRevokeCredentialsMultipleAccounts) {
  fake_provider_->AddAccount("account_id_1");
  fake_provider_->AddAccount("account_id_2");
  fake_provider_->AddAccount("account_id_3");
  oauth2_service_.LoadCredentials("account_id_1");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, token_available_count_);
  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(3U, oauth2_service_.GetAccounts().size());
  EXPECT_TRUE(oauth2_service_.RefreshTokenIsAvailable("account_id_1"));
  EXPECT_TRUE(oauth2_service_.RefreshTokenIsAvailable("account_id_2"));
  EXPECT_TRUE(oauth2_service_.RefreshTokenIsAvailable("account_id_3"));

  ResetObserverCounts();
  oauth2_service_.RevokeAllCredentials();
  EXPECT_EQ(0, token_available_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(3, token_revoked_count_);
  EXPECT_EQ(0U, oauth2_service_.GetAccounts().size());
  EXPECT_FALSE(oauth2_service_.RefreshTokenIsAvailable("account_id_1"));
  EXPECT_FALSE(oauth2_service_.RefreshTokenIsAvailable("account_id_2"));
  EXPECT_FALSE(oauth2_service_.RefreshTokenIsAvailable("account_id_3"));
}

TEST_F(ProfileOAuth2TokenServiceIOSTest, ReloadCredentials) {
  fake_provider_->AddAccount("account_id_1");
  fake_provider_->AddAccount("account_id_2");
  fake_provider_->AddAccount("account_id_3");
  oauth2_service_.LoadCredentials("account_id_1");
  base::RunLoop().RunUntilIdle();

  // Change the accounts.
  ResetObserverCounts();
  fake_provider_->ClearAccounts();
  fake_provider_->AddAccount("account_id_1");
  fake_provider_->AddAccount("account_id_4");
  oauth2_service_.ReloadCredentials();

  EXPECT_EQ(1, token_available_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(2, token_revoked_count_);
  EXPECT_EQ(2U, oauth2_service_.GetAccounts().size());
  EXPECT_TRUE(oauth2_service_.RefreshTokenIsAvailable("account_id_1"));
  EXPECT_FALSE(oauth2_service_.RefreshTokenIsAvailable("account_id_2"));
  EXPECT_FALSE(oauth2_service_.RefreshTokenIsAvailable("account_id_3"));
  EXPECT_TRUE(oauth2_service_.RefreshTokenIsAvailable("account_id_4"));
}

TEST_F(ProfileOAuth2TokenServiceIOSTest,
       ReloadCredentialsIgnoredIfNoPrimaryAccountId) {
  // Change the accounts.
  ResetObserverCounts();
  fake_provider_->AddAccount("account_id_1");
  fake_provider_->AddAccount("account_id_2");
  base::RunLoop().RunUntilIdle();

  oauth2_service_.ReloadCredentials();

  EXPECT_EQ(0, token_available_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(0U, oauth2_service_.GetAccounts().size());
  EXPECT_FALSE(oauth2_service_.RefreshTokenIsAvailable("account_id_1"));
  EXPECT_FALSE(oauth2_service_.RefreshTokenIsAvailable("account_id_2"));
}

TEST_F(ProfileOAuth2TokenServiceIOSTest,
       ReloadCredentialsWithPrimaryAccountId) {
  // Change the accounts.
  ResetObserverCounts();
  fake_provider_->AddAccount("account_id_1");
  fake_provider_->AddAccount("account_id_2");
  base::RunLoop().RunUntilIdle();

  oauth2_service_.ReloadCredentials("account_id_1");
  EXPECT_EQ(2, token_available_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(2U, oauth2_service_.GetAccounts().size());
  EXPECT_TRUE(oauth2_service_.RefreshTokenIsAvailable("account_id_1"));
  EXPECT_TRUE(oauth2_service_.RefreshTokenIsAvailable("account_id_2"));
}

TEST_F(ProfileOAuth2TokenServiceIOSTest, ExcludeAllSecondaryAccounts) {
  // Change the accounts.
  ResetObserverCounts();
  fake_provider_->AddAccount("account_id_1");
  fake_provider_->AddAccount("account_id_2");
  base::RunLoop().RunUntilIdle();

  oauth2_service_.ExcludeAllSecondaryAccounts();
  oauth2_service_.ReloadCredentials("account_id_1");
  EXPECT_EQ(1, token_available_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(1U, oauth2_service_.GetAccounts().size());
  EXPECT_TRUE(oauth2_service_.RefreshTokenIsAvailable("account_id_1"));
  EXPECT_FALSE(oauth2_service_.RefreshTokenIsAvailable("account_id_2"));
}

TEST_F(ProfileOAuth2TokenServiceIOSTest, StartRequestSuccess) {
  fake_provider_->AddAccount("account_id_1");
  oauth2_service_.LoadCredentials("account_id_1");
  base::RunLoop().RunUntilIdle();

  // Fetch access tokens.
  ResetObserverCounts();
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert("scope");
  scoped_ptr<OAuth2TokenService::Request> request(
      oauth2_service_.StartRequest("account_id_1", scopes, this));
  EXPECT_EQ(0, access_token_success_);
  EXPECT_EQ(0, access_token_failure_);

  ResetObserverCounts();
  fake_provider_->IssueAccessTokenForAllRequests();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, access_token_success_);
  EXPECT_EQ(0, access_token_failure_);
}

TEST_F(ProfileOAuth2TokenServiceIOSTest, StartRequestFailure) {
  fake_provider_->AddAccount("account_id_1");
  oauth2_service_.LoadCredentials("account_id_1");
  base::RunLoop().RunUntilIdle();

  // Fetch access tokens.
  ResetObserverCounts();
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert("scope");
  scoped_ptr<OAuth2TokenService::Request> request(
      oauth2_service_.StartRequest("account_id_1", scopes, this));
  EXPECT_EQ(0, access_token_success_);
  EXPECT_EQ(0, access_token_failure_);

  ResetObserverCounts();
  fake_provider_->IssueAccessTokenErrorForAllRequests();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, access_token_success_);
  EXPECT_EQ(1, access_token_failure_);
}

TEST_F(ProfileOAuth2TokenServiceIOSTest, ExcludeSecondaryAccounts) {
  fake_provider_->AddAccount("account_id_1");
  fake_provider_->AddAccount("account_id_2");
  fake_provider_->AddAccount("account_id_3");
  oauth2_service_.LoadCredentials("account_id_1");
  base::RunLoop().RunUntilIdle();

  // Ignore one account should remove it from the list of accounts.
  ResetObserverCounts();
  oauth2_service_.ExcludeSecondaryAccount("account_id_2");
  oauth2_service_.ReloadCredentials();

  EXPECT_EQ(0, token_available_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(1, token_revoked_count_);
  EXPECT_EQ(2U, oauth2_service_.GetAccounts().size());
  EXPECT_TRUE(oauth2_service_.RefreshTokenIsAvailable("account_id_1"));
  EXPECT_FALSE(oauth2_service_.RefreshTokenIsAvailable("account_id_2"));
  EXPECT_TRUE(oauth2_service_.RefreshTokenIsAvailable("account_id_3"));

  // Clear ignored account and the refresh token should be available again.
  ResetObserverCounts();
  oauth2_service_.IncludeSecondaryAccount("account_id_2");
  oauth2_service_.ReloadCredentials();

  EXPECT_EQ(1, token_available_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(3U, oauth2_service_.GetAccounts().size());
  EXPECT_TRUE(oauth2_service_.RefreshTokenIsAvailable("account_id_1"));
  EXPECT_TRUE(oauth2_service_.RefreshTokenIsAvailable("account_id_2"));
  EXPECT_TRUE(oauth2_service_.RefreshTokenIsAvailable("account_id_3"));
}

// Unit test for for http://crbug.com/453470 .
TEST_F(ProfileOAuth2TokenServiceIOSTest, ExcludeSecondaryAccountTwice) {
  fake_provider_->AddAccount("account_id_1");
  fake_provider_->AddAccount("account_id_2");
  oauth2_service_.LoadCredentials("account_id_1");
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(oauth2_service_.RefreshTokenIsAvailable("account_id_1"));
  EXPECT_TRUE(oauth2_service_.RefreshTokenIsAvailable("account_id_2"));

  // Ignore |account_id_2| twice.
  oauth2_service_.ExcludeSecondaryAccount("account_id_2");
  oauth2_service_.ExcludeSecondaryAccount("account_id_2");
  oauth2_service_.ReloadCredentials();

  // Include |account_id_2| once should add the account back.
  ResetObserverCounts();
  oauth2_service_.IncludeSecondaryAccount("account_id_2");
  oauth2_service_.ReloadCredentials();

  EXPECT_EQ(1, token_available_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(2U, oauth2_service_.GetAccounts().size());
  EXPECT_TRUE(oauth2_service_.RefreshTokenIsAvailable("account_id_1"));
  EXPECT_TRUE(oauth2_service_.RefreshTokenIsAvailable("account_id_2"));
}

TEST_F(ProfileOAuth2TokenServiceIOSTest,
       LoadRevokeCredentialsClearsExcludedAccounts) {
  fake_provider_->AddAccount("account_id_1");
  fake_provider_->AddAccount("account_id_2");
  fake_provider_->AddAccount("account_id_3");

  std::vector<std::string> excluded_accounts;
  excluded_accounts.push_back("account_id_2");
  oauth2_service_.ExcludeSecondaryAccounts(excluded_accounts);
  oauth2_service_.ReloadCredentials("account_id_1");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, token_available_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(2U, oauth2_service_.GetAccounts().size());
  EXPECT_TRUE(oauth2_service_.RefreshTokenIsAvailable("account_id_1"));
  EXPECT_FALSE(oauth2_service_.RefreshTokenIsAvailable("account_id_2"));
  EXPECT_TRUE(oauth2_service_.RefreshTokenIsAvailable("account_id_3"));

  ResetObserverCounts();
  oauth2_service_.RevokeAllCredentials();
  EXPECT_TRUE(oauth2_service_.GetExcludedSecondaryAccounts().empty());
}

// Used for test StartRequestInRemoveAccount.
class MarkedForRemovalTester : public OAuth2TokenService::Consumer {
 public:
  MarkedForRemovalTester(const std::string& account_id,
                         OAuth2TokenService* oauth2_service)
      : Consumer(account_id),
        oauth2_service_(oauth2_service),
        request_(),
        token_failures_() {}

  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override {}
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override {
    token_failures_.push_back(error);
    if (error.state() == GoogleServiceAuthError::REQUEST_CANCELED) {
      // Restart a request on |account_id| here, which should fail as we are in
      // a RemoveAccount and |account_id| should be marked for removal.
      OAuth2TokenService::ScopeSet scopes;
      request_ = oauth2_service_->StartRequest(id(), scopes, this);
    }
  }
  const std::vector<GoogleServiceAuthError>& token_failures() const {
    return token_failures_;
  }

 private:
  OAuth2TokenService* oauth2_service_;
  scoped_ptr<OAuth2TokenService::Request> request_;
  std::vector<GoogleServiceAuthError> token_failures_;
};

TEST_F(ProfileOAuth2TokenServiceIOSTest, StartRequestInRemoveAccount) {
  fake_provider_->AddAccount("account_id");
  oauth2_service_.ReloadCredentials("account_id");
  base::RunLoop().RunUntilIdle();

  MarkedForRemovalTester tester("account_id", &oauth2_service_);
  OAuth2TokenService::ScopeSet scopes;
  scoped_ptr<OAuth2TokenService::Request> request(
      oauth2_service_.StartRequest("account_id", scopes, &tester));

  // Trigger a RemoveAccount on "account_id".
  oauth2_service_.RevokeAllCredentials();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(tester.token_failures().size(), 2u);
  // Request cancelled by |CancelRequestsForAccount| in |RemoveAccount|.
  EXPECT_EQ(tester.token_failures()[0].state(),
            GoogleServiceAuthError::REQUEST_CANCELED);
  // Request failing as the account is marked for removal.
  EXPECT_EQ(tester.token_failures()[1].state(),
            GoogleServiceAuthError::USER_NOT_SIGNED_UP);
}
