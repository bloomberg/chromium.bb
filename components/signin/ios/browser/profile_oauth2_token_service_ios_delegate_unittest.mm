// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/ios/browser/profile_oauth2_token_service_ios_delegate.h"

#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/signin/ios/browser/fake_profile_oauth2_token_service_ios_provider.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_token_service_test_util.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProfileOAuth2TokenServiceIOSDelegateTest
    : public testing::Test,
      public OAuth2AccessTokenConsumer,
      public OAuth2TokenService::Observer,
      public SigninErrorController::Observer {
 public:
  ProfileOAuth2TokenServiceIOSDelegateTest()
      : factory_(NULL),
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

    factory_.SetFakeResponse(GaiaUrls::GetInstance()->oauth2_revoke_url(), "",
                             net::HTTP_OK, net::URLRequestStatus::SUCCESS);
    oauth2_service_delegate_.reset(new ProfileOAuth2TokenServiceIOSDelegate(
        &client_, &fake_provider_, &signin_error_controller_));
    oauth2_service_delegate_->AddObserver(this);
    signin_error_controller_.AddObserver(this);
  }

  void TearDown() override {
    signin_error_controller_.RemoveObserver(this);
    oauth2_service_delegate_->RemoveObserver(this);
    oauth2_service_delegate_->Shutdown();
  }

  // OAuth2AccessTokenConsumer implementation.
  void OnGetTokenSuccess(const std::string& access_token,
                         const base::Time& expiration_time) override {
    ++access_token_success_;
  }

  void OnGetTokenFailure(const GoogleServiceAuthError& error) override {
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

  void OnErrorChanged() override { ++error_changed_count_; }

  void ResetObserverCounts() {
    token_available_count_ = 0;
    token_revoked_count_ = 0;
    tokens_loaded_count_ = 0;
    token_available_count_ = 0;
    access_token_failure_ = 0;
    error_changed_count_ = 0;
  }

 protected:
  base::MessageLoop message_loop_;
  net::FakeURLFetcherFactory factory_;
  TestingPrefServiceSimple prefs_;
  TestSigninClient client_;
  SigninErrorController signin_error_controller_;
  FakeProfileOAuth2TokenServiceIOSProvider fake_provider_;
  scoped_ptr<ProfileOAuth2TokenServiceIOSDelegate> oauth2_service_delegate_;
  TestingOAuth2TokenServiceConsumer consumer_;
  int token_available_count_;
  int token_revoked_count_;
  int tokens_loaded_count_;
  int access_token_success_;
  int access_token_failure_;
  int error_changed_count_;
  GoogleServiceAuthError last_access_token_error_;
};

TEST_F(ProfileOAuth2TokenServiceIOSDelegateTest,
       LoadRevokeCredentialsOneAccount) {
  fake_provider_.AddAccount("account_id");
  oauth2_service_delegate_->LoadCredentials("account_id");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, token_available_count_);
  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(1U, oauth2_service_delegate_->GetAccounts().size());
  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable("account_id"));

  ResetObserverCounts();
  oauth2_service_delegate_->RevokeAllCredentials();
  EXPECT_EQ(0, token_available_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(1, token_revoked_count_);
  EXPECT_EQ(0U, oauth2_service_delegate_->GetAccounts().size());
  EXPECT_FALSE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_1"));
}

TEST_F(ProfileOAuth2TokenServiceIOSDelegateTest,
       LoadRevokeCredentialsMultipleAccounts) {
  fake_provider_.AddAccount("account_id_1");
  fake_provider_.AddAccount("account_id_2");
  fake_provider_.AddAccount("account_id_3");
  oauth2_service_delegate_->LoadCredentials("account_id_1");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, token_available_count_);
  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(3U, oauth2_service_delegate_->GetAccounts().size());
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_1"));
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_2"));
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_3"));

  ResetObserverCounts();
  oauth2_service_delegate_->RevokeAllCredentials();
  EXPECT_EQ(0, token_available_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(3, token_revoked_count_);
  EXPECT_EQ(0U, oauth2_service_delegate_->GetAccounts().size());
  EXPECT_FALSE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_1"));
  EXPECT_FALSE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_2"));
  EXPECT_FALSE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_3"));
}

TEST_F(ProfileOAuth2TokenServiceIOSDelegateTest, ReloadCredentials) {
  fake_provider_.AddAccount("account_id_1");
  fake_provider_.AddAccount("account_id_2");
  fake_provider_.AddAccount("account_id_3");
  oauth2_service_delegate_->LoadCredentials("account_id_1");
  base::RunLoop().RunUntilIdle();

  // Change the accounts.
  ResetObserverCounts();
  fake_provider_.ClearAccounts();
  fake_provider_.AddAccount("account_id_1");
  fake_provider_.AddAccount("account_id_4");
  oauth2_service_delegate_->ReloadCredentials();

  EXPECT_EQ(1, token_available_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(2, token_revoked_count_);
  EXPECT_EQ(2U, oauth2_service_delegate_->GetAccounts().size());
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_1"));
  EXPECT_FALSE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_2"));
  EXPECT_FALSE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_3"));
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_4"));
}

TEST_F(ProfileOAuth2TokenServiceIOSDelegateTest,
       ReloadCredentialsIgnoredIfNoPrimaryAccountId) {
  // Change the accounts.
  ResetObserverCounts();
  fake_provider_.AddAccount("account_id_1");
  fake_provider_.AddAccount("account_id_2");
  base::RunLoop().RunUntilIdle();

  oauth2_service_delegate_->ReloadCredentials();

  EXPECT_EQ(0, token_available_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(0U, oauth2_service_delegate_->GetAccounts().size());
  EXPECT_FALSE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_1"));
  EXPECT_FALSE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_2"));
}

TEST_F(ProfileOAuth2TokenServiceIOSDelegateTest,
       ReloadCredentialsWithPrimaryAccountId) {
  // Change the accounts.
  ResetObserverCounts();
  fake_provider_.AddAccount("account_id_1");
  fake_provider_.AddAccount("account_id_2");
  base::RunLoop().RunUntilIdle();

  oauth2_service_delegate_->ReloadCredentials("account_id_1");
  EXPECT_EQ(2, token_available_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(2U, oauth2_service_delegate_->GetAccounts().size());
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_1"));
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_2"));
}

TEST_F(ProfileOAuth2TokenServiceIOSDelegateTest, ExcludeAllSecondaryAccounts) {
  // Change the accounts.
  ResetObserverCounts();
  fake_provider_.AddAccount("account_id_1");
  fake_provider_.AddAccount("account_id_2");
  base::RunLoop().RunUntilIdle();

  oauth2_service_delegate_->ExcludeAllSecondaryAccounts();
  oauth2_service_delegate_->ReloadCredentials("account_id_1");
  EXPECT_EQ(1, token_available_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(1U, oauth2_service_delegate_->GetAccounts().size());
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_1"));
  EXPECT_FALSE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_2"));
}

TEST_F(ProfileOAuth2TokenServiceIOSDelegateTest, StartRequestSuccess) {
  fake_provider_.AddAccount("account_id_1");
  oauth2_service_delegate_->LoadCredentials("account_id_1");
  base::RunLoop().RunUntilIdle();

  // Fetch access tokens.
  ResetObserverCounts();
  std::vector<std::string> scopes;
  scopes.push_back("scope");
  scoped_ptr<OAuth2AccessTokenFetcher> fetcher1(
      oauth2_service_delegate_->CreateAccessTokenFetcher(
          "account_id_1", oauth2_service_delegate_->GetRequestContext(), this));
  fetcher1->Start("foo", "bar", scopes);
  EXPECT_EQ(0, access_token_success_);
  EXPECT_EQ(0, access_token_failure_);

  ResetObserverCounts();
  fake_provider_.IssueAccessTokenForAllRequests();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, access_token_success_);
  EXPECT_EQ(0, access_token_failure_);
}

TEST_F(ProfileOAuth2TokenServiceIOSDelegateTest, StartRequestFailure) {
  fake_provider_.AddAccount("account_id_1");
  oauth2_service_delegate_->LoadCredentials("account_id_1");
  base::RunLoop().RunUntilIdle();

  // Fetch access tokens.
  ResetObserverCounts();
  std::vector<std::string> scopes;
  scopes.push_back("scope");
  scoped_ptr<OAuth2AccessTokenFetcher> fetcher1(
      oauth2_service_delegate_->CreateAccessTokenFetcher(
          "account_id_1", oauth2_service_delegate_->GetRequestContext(), this));
  fetcher1->Start("foo", "bar", scopes);
  EXPECT_EQ(0, access_token_success_);
  EXPECT_EQ(0, access_token_failure_);

  ResetObserverCounts();
  fake_provider_.IssueAccessTokenErrorForAllRequests();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, access_token_success_);
  EXPECT_EQ(1, access_token_failure_);
}

TEST_F(ProfileOAuth2TokenServiceIOSDelegateTest, ExcludeSecondaryAccounts) {
  fake_provider_.AddAccount("account_id_1");
  fake_provider_.AddAccount("account_id_2");
  fake_provider_.AddAccount("account_id_3");
  oauth2_service_delegate_->LoadCredentials("account_id_1");
  base::RunLoop().RunUntilIdle();

  // Ignore one account should remove it from the list of accounts.
  ResetObserverCounts();
  oauth2_service_delegate_->ExcludeSecondaryAccount("account_id_2");
  oauth2_service_delegate_->ReloadCredentials();

  EXPECT_EQ(0, token_available_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(1, token_revoked_count_);
  EXPECT_EQ(2U, oauth2_service_delegate_->GetAccounts().size());
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_1"));
  EXPECT_FALSE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_2"));
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_3"));

  // Clear ignored account and the refresh token should be available again.
  ResetObserverCounts();
  oauth2_service_delegate_->IncludeSecondaryAccount("account_id_2");
  oauth2_service_delegate_->ReloadCredentials();

  EXPECT_EQ(1, token_available_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(3U, oauth2_service_delegate_->GetAccounts().size());
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_1"));
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_2"));
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_3"));
}

// Unit test for for http://crbug.com/453470 .
TEST_F(ProfileOAuth2TokenServiceIOSDelegateTest, ExcludeSecondaryAccountTwice) {
  fake_provider_.AddAccount("account_id_1");
  fake_provider_.AddAccount("account_id_2");
  oauth2_service_delegate_->LoadCredentials("account_id_1");
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_1"));
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_2"));

  // Ignore |account_id_2| twice.
  oauth2_service_delegate_->ExcludeSecondaryAccount("account_id_2");
  oauth2_service_delegate_->ExcludeSecondaryAccount("account_id_2");
  oauth2_service_delegate_->ReloadCredentials();

  // Include |account_id_2| once should add the account back.
  ResetObserverCounts();
  oauth2_service_delegate_->IncludeSecondaryAccount("account_id_2");
  oauth2_service_delegate_->ReloadCredentials();

  EXPECT_EQ(1, token_available_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(2U, oauth2_service_delegate_->GetAccounts().size());
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_1"));
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_2"));
}

TEST_F(ProfileOAuth2TokenServiceIOSDelegateTest,
       LoadRevokeCredentialsClearsExcludedAccounts) {
  fake_provider_.AddAccount("account_id_1");
  fake_provider_.AddAccount("account_id_2");
  fake_provider_.AddAccount("account_id_3");

  std::vector<std::string> excluded_accounts;
  excluded_accounts.push_back("account_id_2");
  oauth2_service_delegate_->ExcludeSecondaryAccounts(excluded_accounts);
  oauth2_service_delegate_->ReloadCredentials("account_id_1");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, token_available_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(2U, oauth2_service_delegate_->GetAccounts().size());
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_1"));
  EXPECT_FALSE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_2"));
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable("account_id_3"));

  ResetObserverCounts();
  oauth2_service_delegate_->RevokeAllCredentials();
  EXPECT_TRUE(oauth2_service_delegate_->GetExcludedSecondaryAccounts().empty());
}

// Verifies that UpdateAuthError does nothing after the credentials have been
// revoked.
TEST_F(ProfileOAuth2TokenServiceIOSDelegateTest,
       UpdateAuthErrorAfterRevokeCredentials) {
  fake_provider_.AddAccount("account_id_1");
  oauth2_service_delegate_->LoadCredentials("account_id_1");
  base::RunLoop().RunUntilIdle();

  ResetObserverCounts();
  GoogleServiceAuthError cancelled_error(
      GoogleServiceAuthError::REQUEST_CANCELED);
  oauth2_service_delegate_->UpdateAuthError("account_id_1", cancelled_error);
  EXPECT_EQ(1, error_changed_count_);

  oauth2_service_delegate_->RevokeAllCredentials();
  ResetObserverCounts();
  oauth2_service_delegate_->UpdateAuthError("account_id_1", cancelled_error);
  EXPECT_EQ(0, error_changed_count_);
}
