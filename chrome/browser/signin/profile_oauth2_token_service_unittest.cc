// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/signin/oauth2_token_service.h"
#include "chrome/browser/signin/oauth2_token_service_test_util.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/token_service_unittest.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

class ProfileOAuth2TokenServiceTest : public TokenServiceTestHarness,
                                      public OAuth2TokenService::Observer {
 public:
  ProfileOAuth2TokenServiceTest()
      : token_available_count_(0),
        token_revoked_count_(0),
        tokens_loaded_count_(0),
        tokens_cleared_count_(0) {
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
  virtual void OnRefreshTokenRevoked(
      const std::string& account_id,
      const GoogleServiceAuthError& error) OVERRIDE {
    ++token_revoked_count_;
  }
  virtual void OnRefreshTokensLoaded() OVERRIDE {
    ++tokens_loaded_count_;
  }
  virtual void OnRefreshTokensCleared() OVERRIDE {
    ++tokens_cleared_count_;
  }

  void ResetObserverCounts() {
    token_available_count_ = 0;
    token_revoked_count_ = 0;
    tokens_loaded_count_ = 0;
    tokens_cleared_count_ = 0;
  }

  void ExpectNoNotifications() {
    EXPECT_EQ(0, token_available_count_);
    EXPECT_EQ(0, token_revoked_count_);
    EXPECT_EQ(0, tokens_loaded_count_);
    EXPECT_EQ(0, tokens_cleared_count_);
    ResetObserverCounts();
  }

  void ExpectOneTokenAvailableNotification() {
    EXPECT_EQ(1, token_available_count_);
    EXPECT_EQ(0, token_revoked_count_);
    EXPECT_EQ(0, tokens_loaded_count_);
    EXPECT_EQ(0, tokens_cleared_count_);
    ResetObserverCounts();
  }

  void ExpectOneTokenRevokedNotification() {
    EXPECT_EQ(0, token_available_count_);
    EXPECT_EQ(1, token_revoked_count_);
    EXPECT_EQ(0, tokens_loaded_count_);
    EXPECT_EQ(0, tokens_cleared_count_);
    ResetObserverCounts();
  }

  void ExpectOneTokensLoadedNotification() {
    EXPECT_EQ(0, token_available_count_);
    EXPECT_EQ(0, token_revoked_count_);
    EXPECT_EQ(1, tokens_loaded_count_);
    EXPECT_EQ(0, tokens_cleared_count_);
    ResetObserverCounts();
  }

  void ExpectOneTokensClearedNotification() {
    EXPECT_EQ(0, token_available_count_);
    EXPECT_EQ(0, token_revoked_count_);
    EXPECT_EQ(0, tokens_loaded_count_);
    EXPECT_EQ(1, tokens_cleared_count_);
    ResetObserverCounts();
  }

 protected:
  net::TestURLFetcherFactory factory_;
  ProfileOAuth2TokenService* oauth2_service_;
  TestingOAuth2TokenServiceConsumer consumer_;
  int token_available_count_;
  int token_revoked_count_;
  int tokens_loaded_count_;
  int tokens_cleared_count_;
};

TEST_F(ProfileOAuth2TokenServiceTest, Notifications) {
  EXPECT_EQ(0, oauth2_service_->cache_size_for_testing());
  service()->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                   "refreshToken");
  ExpectOneTokenAvailableNotification();

  service()->EraseTokensFromDB();
  service()->ResetCredentialsInMemory();
  ExpectOneTokensClearedNotification();
}

// Until the TokenService class is removed, problems fetching the LSO token
// should translate to problems fetching the oauth2 refresh token.
TEST_F(ProfileOAuth2TokenServiceTest, LsoNotification) {
  EXPECT_EQ(0, oauth2_service_->cache_size_for_testing());

  // Get a valid token.
  service()->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                   "refreshToken");
  ExpectOneTokenAvailableNotification();

  service()->OnIssueAuthTokenFailure(
      GaiaConstants::kLSOService,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  ExpectOneTokenRevokedNotification();
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
  std::set<std::string> scope_list;
  scope_list.insert("scope");
  service()->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                   "refreshToken");
  ExpectOneTokenAvailableNotification();
  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      scope_list, &consumer_));
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
  service()->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                  "");
  ExpectOneTokenAvailableNotification();
  service()->EraseTokensFromDB();
  ExpectOneTokensClearedNotification();

  EXPECT_EQ(0, oauth2_service_->cache_size_for_testing());
  service()->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                  "refreshToken");
  ExpectOneTokenAvailableNotification();

  request = oauth2_service_->StartRequest(scope_list, &consumer_);
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

// Android doesn't use the current profile's TokenService login refresh token.
#if !defined(OS_ANDROID)
TEST_F(ProfileOAuth2TokenServiceTest, StaleRefreshTokensNotCached) {
  EXPECT_FALSE(service()->HasOAuthLoginToken());
  EXPECT_FALSE(oauth2_service_->ShouldCacheForRefreshToken(service(), "T1"));

  service()->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                  "T1");
  ExpectOneTokenAvailableNotification();
  EXPECT_TRUE(oauth2_service_->ShouldCacheForRefreshToken(service(), "T1"));
  EXPECT_FALSE(oauth2_service_->ShouldCacheForRefreshToken(service(), "T2"));

  service()->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                  "T2");
  ExpectOneTokenAvailableNotification();
  EXPECT_TRUE(oauth2_service_->ShouldCacheForRefreshToken(service(), "T2"));
  EXPECT_FALSE(oauth2_service_->ShouldCacheForRefreshToken(NULL, "T2"));
}
#endif
