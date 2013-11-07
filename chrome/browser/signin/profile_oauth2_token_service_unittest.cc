// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/signin/fake_signin_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service_unittest.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "google_apis/gaia/oauth2_token_service_test_util.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

// Defining constant here to handle backward compatiblity tests, but this
// constant is no longer used in current versions of chrome.
static const char kLSOService[] = "lso";
static const char kEmail[] = "user@gmail.com";

class ProfileOAuth2TokenServiceTest : public TokenServiceTestHarness,
                                      public OAuth2TokenService::Observer {
 public:
  ProfileOAuth2TokenServiceTest()
      : oauth2_service_(NULL),
        token_available_count_(0),
        token_revoked_count_(0),
        tokens_loaded_count_(0) {
  }

  virtual void SetUp() OVERRIDE {
    TokenServiceTestHarness::SetUp();
    UpdateCredentialsOnService();
    oauth2_service_ = ProfileOAuth2TokenServiceFactory::GetForProfile(
        profile());
    oauth2_service_->AddObserver(this);
  }

  virtual void TearDown() OVERRIDE {
    oauth2_service_->RemoveObserver(this);
    TokenServiceTestHarness::TearDown();
  }

  // OAuth2TokenService::Observer implementation.
  virtual void OnRefreshTokenAvailable(const std::string& account_id) OVERRIDE {
    ++token_available_count_;
  }
  virtual void OnRefreshTokenRevoked(const std::string& account_id) OVERRIDE {
    ++token_revoked_count_;
  }
  virtual void OnRefreshTokensLoaded() OVERRIDE {
    ++tokens_loaded_count_;
  }

  void ResetObserverCounts() {
    token_available_count_ = 0;
    token_revoked_count_ = 0;
    tokens_loaded_count_ = 0;
  }

  void ExpectNoNotifications() {
    EXPECT_EQ(0, token_available_count_);
    EXPECT_EQ(0, token_revoked_count_);
    EXPECT_EQ(0, tokens_loaded_count_);
    ResetObserverCounts();
  }

  void ExpectOneTokenAvailableNotification() {
    EXPECT_EQ(1, token_available_count_);
    EXPECT_EQ(0, token_revoked_count_);
    EXPECT_EQ(0, tokens_loaded_count_);
    ResetObserverCounts();
  }

  void ExpectOneTokenRevokedNotification() {
    EXPECT_EQ(0, token_available_count_);
    EXPECT_EQ(1, token_revoked_count_);
    EXPECT_EQ(0, tokens_loaded_count_);
    ResetObserverCounts();
  }

  void ExpectOneTokensLoadedNotification() {
    EXPECT_EQ(0, token_available_count_);
    EXPECT_EQ(0, token_revoked_count_);
    EXPECT_EQ(1, tokens_loaded_count_);
    ResetObserverCounts();
  }

 protected:
  net::TestURLFetcherFactory factory_;
  ProfileOAuth2TokenService* oauth2_service_;
  TestingOAuth2TokenServiceConsumer consumer_;
  int token_available_count_;
  int token_revoked_count_;
  int tokens_loaded_count_;
};

TEST_F(ProfileOAuth2TokenServiceTest, Notifications) {
  EXPECT_EQ(0, oauth2_service_->cache_size_for_testing());
  CreateSigninManager(kEmail);
  service()->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                   "refreshToken");
  ExpectOneTokenAvailableNotification();

  service()->EraseTokensFromDB();
  service()->ResetCredentialsInMemory();
}

TEST_F(ProfileOAuth2TokenServiceTest, PersistenceDBUpgrade) {
  CreateSigninManager(kEmail);

  std::string main_account_id(kEmail);
  std::string main_refresh_token("old_refresh_token");

  // Populate DB with legacy tokens.
  service()->OnIssueAuthTokenSuccess(GaiaConstants::kSyncService,
                                     "syncServiceToken");
  service()->OnIssueAuthTokenSuccess(kLSOService, "lsoToken");
  service()->OnIssueAuthTokenSuccess(
      GaiaConstants::kGaiaOAuth2LoginRefreshToken,
      main_refresh_token);
  // Add a token using the new API.
  ResetObserverCounts();

  // Force LoadCredentials.
  oauth2_service_->LoadCredentials();
  base::RunLoop().RunUntilIdle();

  // Legacy tokens get discarded, but the old refresh token is kept.
  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(1, token_available_count_);
  EXPECT_TRUE(oauth2_service_->RefreshTokenIsAvailable(main_account_id));
  EXPECT_EQ(1U, oauth2_service_->refresh_tokens_.size());
  EXPECT_EQ(main_refresh_token,
            oauth2_service_->refresh_tokens_[main_account_id]->refresh_token());

  // Add an old legacy token to the DB, to ensure it will not overwrite existing
  // credentials for main account.
  service()->OnIssueAuthTokenSuccess(
      GaiaConstants::kGaiaOAuth2LoginRefreshToken,
      "secondOldRefreshToken");
  // Add some other legacy token. (Expected to get discarded).
  service()->OnIssueAuthTokenSuccess(kLSOService, "lsoToken");
  // Also add a token using PO2TS.UpdateCredentials and make sure upgrade does
  // not wipe it.
  std::string other_account_id("other_account_id");
  std::string other_refresh_token("other_refresh_token");
  oauth2_service_->UpdateCredentials(other_account_id, other_refresh_token);
  ResetObserverCounts();

  // Force LoadCredentials.
  oauth2_service_->LoadCredentials();
  base::RunLoop().RunUntilIdle();

  // Again legacy tokens get discarded, but since the main porfile account
  // token is present it is not overwritten.
  EXPECT_EQ(2, token_available_count_);
  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_TRUE(oauth2_service_->RefreshTokenIsAvailable(main_account_id));
  // TODO(fgorski): cover both using RefreshTokenIsAvailable() and then get the
  // tokens using GetRefreshToken()
  EXPECT_EQ(2U, oauth2_service_->refresh_tokens_.size());
  EXPECT_EQ(main_refresh_token,
            oauth2_service_->refresh_tokens_[main_account_id]->refresh_token());
  EXPECT_EQ(
      other_refresh_token,
      oauth2_service_->refresh_tokens_[other_account_id]->refresh_token());

  oauth2_service_->RevokeAllCredentials();
}

TEST_F(ProfileOAuth2TokenServiceTest, PersistenceRevokeCredentials) {
  std::string account_id_1 = "account_id_1";
  std::string refresh_token_1 = "refresh_token_1";
  std::string account_id_2 = "account_id_2";
  std::string refresh_token_2 = "refresh_token_2";

  // TODO(fgorski): Enable below when implemented:
  // EXPECT_FALSE(oauth2_servive_->RefreshTokenIsAvailable(account_id_1));
  // EXPECT_FALSE(oauth2_servive_->RefreshTokenIsAvailable(account_id_2));

  oauth2_service_->UpdateCredentials(account_id_1, refresh_token_1);
  oauth2_service_->UpdateCredentials(account_id_2, refresh_token_2);

  // TODO(fgorski): Enable below when implemented:
  // EXPECT_TRUE(oauth2_servive_->RefreshTokenIsAvailable(account_id_1));
  // EXPECT_TRUE(oauth2_service_->RefreshTokenIsAvailable(account_id_2));

  ResetObserverCounts();
  oauth2_service_->RevokeCredentials(account_id_1);
  ExpectOneTokenRevokedNotification();

  // TODO(fgorski): Enable below when implemented:
  // EXPECT_FALSE(oauth2_servive_->RefreshTokenIsAvailable(account_id_1));
  // EXPECT_TRUE(oauth2_service_->RefreshTokenIsAvailable(account_id_2));

  oauth2_service_->RevokeAllCredentials();
  EXPECT_EQ(0, token_available_count_);
  EXPECT_EQ(1, token_revoked_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  ResetObserverCounts();
}

TEST_F(ProfileOAuth2TokenServiceTest, PersistenceLoadCredentials) {
  // Ensure DB is clean.
  oauth2_service_->RevokeAllCredentials();
  ResetObserverCounts();
  // Perform a load from an empty DB.
  oauth2_service_->LoadCredentials();
  base::RunLoop().RunUntilIdle();
  ExpectOneTokensLoadedNotification();
  EXPECT_EQ(0U, oauth2_service_->refresh_tokens_.size());
  // Setup a DB with tokens that don't require upgrade and clear memory.
  oauth2_service_->UpdateCredentials("account_id", "refresh_token");
  oauth2_service_->UpdateCredentials("account_id2", "refresh_token2");
  oauth2_service_->refresh_tokens_.clear();
  ResetObserverCounts();

  oauth2_service_->LoadCredentials();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, token_available_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(1, tokens_loaded_count_);
  ResetObserverCounts();

  // TODO(fgorski): Enable below when implemented:
  // EXPECT_TRUE(oauth2_servive_->RefreshTokenIsAvailable("account_id"));
  // EXPECT_TRUE(oauth2_service_->RefreshTokenIsAvailable("account_id2"));

  oauth2_service_->RevokeAllCredentials();
  EXPECT_EQ(0, token_available_count_);
  EXPECT_EQ(2, token_revoked_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  ResetObserverCounts();
}

TEST_F(ProfileOAuth2TokenServiceTest, PersistanceNotifications) {
  EXPECT_EQ(0, oauth2_service_->cache_size_for_testing());
  oauth2_service_->UpdateCredentials("account_id", "refresh_token");
  ExpectOneTokenAvailableNotification();

  oauth2_service_->UpdateCredentials("account_id", "refresh_token");
  ExpectNoNotifications();

  oauth2_service_->UpdateCredentials("account_id", "refresh_token2");
  ExpectOneTokenAvailableNotification();

  oauth2_service_->RevokeCredentials("account_id");
  ExpectOneTokenRevokedNotification();

  oauth2_service_->UpdateCredentials("account_id", "refresh_token2");
  ExpectOneTokenAvailableNotification();

  oauth2_service_->RevokeAllCredentials();
  ResetObserverCounts();
}

TEST_F(ProfileOAuth2TokenServiceTest, GetAccounts) {
  EXPECT_TRUE(oauth2_service_->GetAccounts().empty());
  oauth2_service_->UpdateCredentials("account_id1", "refresh_token1");
  oauth2_service_->UpdateCredentials("account_id2", "refresh_token2");
  std::vector<std::string> accounts = oauth2_service_->GetAccounts();
  EXPECT_EQ(2u, accounts.size());
  EXPECT_EQ(1, count(accounts.begin(), accounts.end(), "account_id1"));
  EXPECT_EQ(1, count(accounts.begin(), accounts.end(), "account_id2"));
  oauth2_service_->RevokeCredentials("account_id2");
  accounts = oauth2_service_->GetAccounts();
  EXPECT_EQ(1u, oauth2_service_->GetAccounts().size());
  EXPECT_EQ(1, count(accounts.begin(), accounts.end(), "account_id1"));
}

// Until the TokenService class is removed, finish token loading in TokenService
// should translate to finish token loading in ProfileOAuth2TokenService.
TEST_F(ProfileOAuth2TokenServiceTest, TokensLoaded) {
  EXPECT_EQ(0, oauth2_service_->cache_size_for_testing());
  service()->LoadTokensFromDB();
  base::RunLoop().RunUntilIdle();
  ExpectOneTokensLoadedNotification();
}

TEST_F(ProfileOAuth2TokenServiceTest, UnknownNotificationsAreNoops) {
  EXPECT_EQ(0, oauth2_service_->cache_size_for_testing());
  CreateSigninManager(kEmail);
  service()->IssueAuthTokenForTest("foo", "toto");
  ExpectNoNotifications();

  // Get a valid token.
  service()->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                   "refreshToken");
  ExpectOneTokenAvailableNotification();

  service()->IssueAuthTokenForTest("bar", "baz");
  ExpectNoNotifications();
}

TEST_F(ProfileOAuth2TokenServiceTest, TokenServiceUpdateClearsCache) {
  EXPECT_EQ(0, oauth2_service_->cache_size_for_testing());
  CreateSigninManager(kEmail);
  std::set<std::string> scope_list;
  scope_list.insert("scope");
  oauth2_service_->UpdateCredentials(oauth2_service_->GetPrimaryAccountId(),
                                    "refreshToken");
  ExpectOneTokenAvailableNotification();

  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      oauth2_service_->GetPrimaryAccountId(), scope_list, &consumer_));
  base::RunLoop().RunUntilIdle();
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);
  EXPECT_EQ(1, oauth2_service_->cache_size_for_testing());

  // Signs out and signs in
  oauth2_service_->RevokeCredentials(oauth2_service_->GetPrimaryAccountId());
  ExpectOneTokenRevokedNotification();

  EXPECT_EQ(0, oauth2_service_->cache_size_for_testing());
  oauth2_service_->UpdateCredentials(oauth2_service_->GetPrimaryAccountId(),
                                     "refreshToken");
  ExpectOneTokenAvailableNotification();

  request = oauth2_service_->StartRequest(
      oauth2_service_->GetPrimaryAccountId(), scope_list, &consumer_);
  base::RunLoop().RunUntilIdle();
  fetcher = factory_.GetFetcherByID(0);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("another token", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(2, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("another token", consumer_.last_token_);
  EXPECT_EQ(1, oauth2_service_->cache_size_for_testing());
}

TEST_F(ProfileOAuth2TokenServiceTest, FetchTransientError) {
  EXPECT_EQ(0, oauth2_service_->cache_size_for_testing());
  CreateSigninManager(kEmail);
  std::set<std::string> scope_list;
  scope_list.insert("scope");
  oauth2_service_->set_max_authorization_token_fetch_retries_for_testing(0);
  oauth2_service_->UpdateCredentials(oauth2_service_->GetPrimaryAccountId(),
                                     "refreshToken");
  ExpectOneTokenAvailableNotification();

  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      oauth2_service_->GetPrimaryAccountId(), scope_list, &consumer_));
  base::RunLoop().RunUntilIdle();
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  fetcher->set_response_code(net::HTTP_FORBIDDEN);
  fetcher->SetResponseString("");
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
      oauth2_service_->signin_global_error()->GetLastAuthError());
}

