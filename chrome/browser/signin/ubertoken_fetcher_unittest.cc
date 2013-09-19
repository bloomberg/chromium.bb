// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/ubertoken_fetcher.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/token_service_unittest.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockUbertokenConsumer : public UbertokenConsumer {
 public:
  MockUbertokenConsumer()
      : nb_correct_token_(0),
        last_error_(GoogleServiceAuthError::AuthErrorNone()),
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

class MockOAuth2TokenService : public ProfileOAuth2TokenService {
  // OAuth2TokenService overrides:
  virtual scoped_ptr<OAuth2TokenService::Request>
      StartRequest(const OAuth2TokenService::ScopeSet& scopes,
                   OAuth2TokenService::Consumer* consumer) OVERRIDE {
    // Don't actually make a request.
    scoped_ptr<OAuth2TokenService::Request> request;
    return request.Pass();
  }
};


BrowserContextKeyedService* Build(content::BrowserContext* profile) {
  MockOAuth2TokenService* token_service = new MockOAuth2TokenService();
  token_service->Initialize(static_cast<Profile*>(profile));
  return token_service;
}

}  // namespace

class UbertokenFetcherTest : public TokenServiceTestHarness {
 public:
  virtual void SetUp() OVERRIDE {
    TokenServiceTestHarness::SetUp();
    UpdateCredentialsOnService();
    fetcher_.reset(new UbertokenFetcher(profile(), &consumer_));
  }

  virtual scoped_ptr<TestingProfile> CreateProfile() OVERRIDE {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(ProfileOAuth2TokenServiceFactory::GetInstance(),
                              Build);
    return builder.Build().Pass();
  }

  virtual void TearDown() OVERRIDE {
    fetcher_.reset();
    TokenServiceTestHarness::TearDown();
  }

 protected:
  net::TestURLFetcherFactory factory_;
  MockUbertokenConsumer consumer_;
  scoped_ptr<UbertokenFetcher> fetcher_;
};

TEST_F(UbertokenFetcherTest, Basic) {
}

TEST_F(UbertokenFetcherTest, Success) {
  service()->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                   "refreshToken");
  fetcher_->StartFetchingToken();
  fetcher_->OnGetTokenSuccess(NULL, "accessToken", base::Time());
  fetcher_->OnUberAuthTokenSuccess("uberToken");
  EXPECT_EQ(0, consumer_.nb_error_);
  EXPECT_EQ(1, consumer_.nb_correct_token_);
  EXPECT_EQ("uberToken", consumer_.last_token_);
}

TEST_F(UbertokenFetcherTest, NoRefreshToken) {
  fetcher_->StartFetchingToken();
  GoogleServiceAuthError error(GoogleServiceAuthError::USER_NOT_SIGNED_UP);
  fetcher_->OnGetTokenFailure(NULL, error);
  EXPECT_EQ(1, consumer_.nb_error_);
  EXPECT_EQ(0, consumer_.nb_correct_token_);
}

TEST_F(UbertokenFetcherTest, FailureToGetAccessToken) {
  GoogleServiceAuthError error(GoogleServiceAuthError::USER_NOT_SIGNED_UP);

  service()->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                   "refreshToken");
  fetcher_->StartFetchingToken();
  fetcher_->OnGetTokenFailure(NULL, error);

  EXPECT_EQ(1, consumer_.nb_error_);
  EXPECT_EQ(0, consumer_.nb_correct_token_);
  EXPECT_EQ("", consumer_.last_token_);
}

TEST_F(UbertokenFetcherTest, FailureToGetUberToken) {
  GoogleServiceAuthError error(GoogleServiceAuthError::USER_NOT_SIGNED_UP);

  service()->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                   "refreshToken");
  fetcher_->StartFetchingToken();
  fetcher_->OnGetTokenSuccess(NULL, "accessToken", base::Time());
  fetcher_->OnUberAuthTokenFailure(error);

  EXPECT_EQ(1, consumer_.nb_error_);
  EXPECT_EQ(0, consumer_.nb_correct_token_);
  EXPECT_EQ("", consumer_.last_token_);
}
