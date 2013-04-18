// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

class ProfileOAuth2TokenServiceTest : public TokenServiceTestHarness {
 public:
  ProfileOAuth2TokenServiceTest() {}

  virtual void SetUp() OVERRIDE {
    TokenServiceTestHarness::SetUp();
    io_thread_.reset(new content::TestBrowserThread(content::BrowserThread::IO,
                                                    &message_loop_));
    service_->UpdateCredentials(credentials_);
    profile_->CreateRequestContext();
    oauth2_service_ = ProfileOAuth2TokenServiceFactory::GetForProfile(
        profile_.get());
  }

  virtual void TearDown() OVERRIDE {
    TokenServiceTestHarness::TearDown();
  }

 protected:
  scoped_ptr<content::TestBrowserThread> io_thread_;
  net::TestURLFetcherFactory factory_;
  ProfileOAuth2TokenService* oauth2_service_;
  TestingOAuth2TokenServiceConsumer consumer_;
};

TEST_F(ProfileOAuth2TokenServiceTest, TokenServiceUpdateClearsCache) {
  EXPECT_EQ(0, oauth2_service_->cache_size_for_testing());
  std::set<std::string> scope_list;
  scope_list.insert("scope");
  service_->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                  "refreshToken");
  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      scope_list, &consumer_));
  message_loop_.RunUntilIdle();
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);
  EXPECT_EQ(1, oauth2_service_->cache_size_for_testing());

  // Signs out and signs in
  service_->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                  "");
  service_->EraseTokensFromDB();
  EXPECT_EQ(0, oauth2_service_->cache_size_for_testing());
  service_->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                  "refreshToken");

  request = oauth2_service_->StartRequest(scope_list, &consumer_);
  message_loop_.RunUntilIdle();
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
  EXPECT_FALSE(service_->HasOAuthLoginToken());
  EXPECT_FALSE(oauth2_service_->ShouldCacheForRefreshToken(service_, "T1"));

  service_->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                  "T1");
  EXPECT_TRUE(oauth2_service_->ShouldCacheForRefreshToken(service_, "T1"));
  EXPECT_FALSE(oauth2_service_->ShouldCacheForRefreshToken(service_, "T2"));

  service_->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                  "T2");
  EXPECT_TRUE(oauth2_service_->ShouldCacheForRefreshToken(service_, "T2"));
  EXPECT_FALSE(oauth2_service_->ShouldCacheForRefreshToken(NULL, "T2"));
}
#endif
