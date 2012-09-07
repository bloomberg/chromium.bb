// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/ubertoken_fetcher.h"

#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_unittest.h"
#include "chrome/common/chrome_notification_types.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

class MockUbertokenConsumer : public UbertokenConsumer {
 public:
  MockUbertokenConsumer()
      : nb_correct_token_(0),
        last_error_(GoogleServiceAuthError::None()),
        nb_error_(0) {
  }
  virtual ~MockUbertokenConsumer() {}

  virtual void OnUbertokenSuccess(const std::string& token) OVERRIDE {
    last_token_ = token;
    ++ nb_correct_token_;
  }

  virtual void OnUbertokenFailure(const GoogleServiceAuthError& error)
      OVERRIDE {
    last_error_ = error;
    ++nb_error_;
  }

  std::string last_token_;
  int nb_correct_token_;
  GoogleServiceAuthError last_error_;
  int nb_error_;
};

class UbertokenFetcherTest : public TokenServiceTestHarness {
 public:
  virtual void SetUp() OVERRIDE {
    TokenServiceTestHarness::SetUp();
    service_->UpdateCredentials(credentials_);
    fetcher_.reset(new UbertokenFetcher(profile_.get(), &consumer_));
  }

  virtual void TearDown() OVERRIDE {
    TokenServiceTestHarness::TearDown();
  }

 protected:
  net::TestURLFetcherFactory factory_;
  MockUbertokenConsumer consumer_;
  scoped_ptr<UbertokenFetcher> fetcher_;
};

TEST_F(UbertokenFetcherTest, TestSuccessWithoutRefreshToken) {
  fetcher_->StartFetchingToken();
  TokenService::TokenAvailableDetails
      details(GaiaConstants::kGaiaOAuth2LoginRefreshToken, "refreshToken");
  service_->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                 "refreshToken");
  fetcher_->Observe(chrome::NOTIFICATION_TOKEN_AVAILABLE,
                    content::Source<TokenService>(service_),
                    content::Details<const TokenService::TokenAvailableDetails>(
                        &details));
  fetcher_->OnRefreshTokenResponse("accessToken", 3600);
  fetcher_->OnUberAuthTokenSuccess("uberToken");
  EXPECT_EQ(0, consumer_.nb_error_);
  EXPECT_EQ(1, consumer_.nb_correct_token_);
  EXPECT_EQ("uberToken", consumer_.last_token_);
}

TEST_F(UbertokenFetcherTest, TestSuccessWithRefreshToken) {
  service_->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                 "refreshToken");
  fetcher_->StartFetchingToken();
  fetcher_->OnRefreshTokenResponse("accessToken", 3600);
  fetcher_->OnUberAuthTokenSuccess("uberToken");
  EXPECT_EQ(0, consumer_.nb_error_);
  EXPECT_EQ(1, consumer_.nb_correct_token_);
  EXPECT_EQ("uberToken", consumer_.last_token_);
}


TEST_F(UbertokenFetcherTest, TestFailures) {
  GoogleServiceAuthError error(GoogleServiceAuthError::USER_NOT_SIGNED_UP);
  EXPECT_EQ(0, consumer_.nb_error_);
  TokenService::TokenRequestFailedDetails
      details(GaiaConstants::kGaiaOAuth2LoginRefreshToken, error);
  fetcher_->Observe(
      chrome::NOTIFICATION_TOKEN_REQUEST_FAILED,
      content::Source<TokenService>(service_),
      content::Details<const TokenService::TokenRequestFailedDetails>(
          &details));
  EXPECT_EQ(1, consumer_.nb_error_);
  fetcher_->OnOAuthError();
  EXPECT_EQ(2, consumer_.nb_error_);
  fetcher_->OnNetworkError(401);
  EXPECT_EQ(3, consumer_.nb_error_);
  fetcher_->OnUberAuthTokenFailure(error);
  EXPECT_EQ(4, consumer_.nb_error_);
}
