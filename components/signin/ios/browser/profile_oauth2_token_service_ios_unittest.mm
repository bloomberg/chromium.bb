// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/signin/ios/browser/profile_oauth2_token_service_ios.h"
#include "google_apis/gaia/gaia_urls.h"
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
        token_available_count_(0),
        token_revoked_count_(0),
        tokens_loaded_count_(0),
        access_token_success_(0),
        access_token_failure_(0),
        last_access_token_error_(GoogleServiceAuthError::NONE) {}

  virtual void SetUp() OVERRIDE {
    factory_.SetFakeResponse(GaiaUrls::GetInstance()->oauth2_revoke_url(),
                             "",
                             net::HTTP_OK,
                             net::URLRequestStatus::SUCCESS);
    fake_provider_ = client_.GetIOSProviderAsFake();
    oauth2_service_.Initialize(&client_);
    oauth2_service_.AddObserver(this);
  }

  virtual void TearDown() OVERRIDE {
    oauth2_service_.RemoveObserver(this);
    oauth2_service_.Shutdown();
  }

  // OAuth2TokenService::Consumer implementation.
  virtual void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                 const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE {
    ++access_token_success_;
  }

  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE {
    ++access_token_failure_;
    last_access_token_error_ = error;
  };

  // OAuth2TokenService::Observer implementation.
  virtual void OnRefreshTokenAvailable(const std::string& account_id) OVERRIDE {
    ++token_available_count_;
  }
  virtual void OnRefreshTokenRevoked(const std::string& account_id) OVERRIDE {
    ++token_revoked_count_;
  }
  virtual void OnRefreshTokensLoaded() OVERRIDE { ++tokens_loaded_count_; }

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
  TestSigninClient client_;
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
