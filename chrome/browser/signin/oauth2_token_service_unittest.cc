// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "chrome/browser/signin/oauth2_token_service.h"
#include "chrome/browser/signin/oauth2_token_service_factory.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/signin/token_service_unittest.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

static const char kValidTokenResponse[] =
    "{"
    "  \"access_token\": \"%s\","
    "  \"expires_in\": %d,"
    "  \"token_type\": \"Bearer\""
    "}";

std::string GetValidTokenResponse(std::string token, int expiration) {
  return base::StringPrintf(kValidTokenResponse, token.c_str(), expiration);
}

// A simple testing consumer.
class TestingOAuth2TokenServiceConsumer : public OAuth2TokenService::Consumer {
 public:
  TestingOAuth2TokenServiceConsumer()
      : number_of_correct_tokens_(0),
        last_error_(GoogleServiceAuthError::AuthErrorNone()),
        number_of_errors_(0) {}
  virtual ~TestingOAuth2TokenServiceConsumer() {}

  virtual void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                 const std::string& token,
                                 const base::Time& expiration_date) OVERRIDE {
    last_token_ = token;
    ++number_of_correct_tokens_;
  }

  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE {
    last_error_ = error;
    ++number_of_errors_;
  }

  std::string last_token_;
  int number_of_correct_tokens_;
  GoogleServiceAuthError last_error_;
  int number_of_errors_;
};

// A testing consumer that retries on error.
class RetryingTestingOAuth2TokenServiceConsumer
    : public OAuth2TokenService::Consumer {
 public:
  RetryingTestingOAuth2TokenServiceConsumer(
      OAuth2TokenService* oauth2_service)
      : oauth2_service_(oauth2_service),
        number_of_correct_tokens_(0),
        last_error_(GoogleServiceAuthError::AuthErrorNone()),
        number_of_errors_(0) {}
  virtual ~RetryingTestingOAuth2TokenServiceConsumer() {}

  virtual void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                 const std::string& token,
                                 const base::Time& expiration_date) OVERRIDE {
    last_token_ = token;
    ++number_of_correct_tokens_;
  }

  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE {
    last_error_ = error;
    ++number_of_errors_;
    request_.reset(oauth2_service_->StartRequest(
        std::set<std::string>(), this).release());
  }

  OAuth2TokenService* oauth2_service_;
  scoped_ptr<OAuth2TokenService::Request> request_;
  std::string last_token_;
  int number_of_correct_tokens_;
  GoogleServiceAuthError last_error_;
  int number_of_errors_;
};

class OAuth2TokenServiceTest : public TokenServiceTestHarness {
 public:
  OAuth2TokenServiceTest() {}

  virtual void SetUp() OVERRIDE {
    TokenServiceTestHarness::SetUp();
    io_thread_.reset(new content::TestBrowserThread(content::BrowserThread::IO,
                                                    &message_loop_));
    service_->UpdateCredentials(credentials_);
    profile_->CreateRequestContext();
    oauth2_service_ = OAuth2TokenServiceFactory::GetForProfile(profile_.get());
  }

  virtual void TearDown() OVERRIDE {
    TokenServiceTestHarness::TearDown();
  }

 protected:
  scoped_ptr<content::TestBrowserThread> io_thread_;
  net::TestURLFetcherFactory factory_;
  OAuth2TokenService* oauth2_service_;
  TestingOAuth2TokenServiceConsumer consumer_;
};

#if defined(OS_ANDROID)
#define MAYBE_NoOAuth2RefreshToken DISABLED_NoOAuth2RefreshToken
#define MAYBE_FailureShouldNotRetry DISABLED_FailureShouldNotRetry
#else
#define MAYBE_NoOAuth2RefreshToken NoOAuth2RefreshToken
#define MAYBE_FailureShouldNotRetry FailureShouldNotRetry
#endif  // defined(OS_ANDROID)

TEST_F(OAuth2TokenServiceTest, MAYBE_NoOAuth2RefreshToken) {
  scoped_ptr<OAuth2TokenService::Request> request(
      oauth2_service_->StartRequest(std::set<std::string>(), &consumer_));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(0, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(1, consumer_.number_of_errors_);
}

TEST_F(OAuth2TokenServiceTest, MAYBE_FailureShouldNotRetry) {
  service_->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                  "refreshToken");
  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      std::set<std::string>(), &consumer_));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(0, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  EXPECT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_UNAUTHORIZED);
  fetcher->SetResponseString("");
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(0, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(1, consumer_.number_of_errors_);
  EXPECT_EQ(fetcher, factory_.GetFetcherByID(0));
}

TEST_F(OAuth2TokenServiceTest, SuccessWithoutCaching) {
  service_->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                  "refreshToken");
  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      std::set<std::string>(), &consumer_));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(0, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  EXPECT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);
}

TEST_F(OAuth2TokenServiceTest, SuccessWithCaching) {
  std::set<std::string> scopes1;
  scopes1.insert("s1");
  scopes1.insert("s2");
  std::set<std::string> scopes1_same;
  scopes1_same.insert("s2");
  scopes1_same.insert("s1");
  std::set<std::string> scopes2;
  scopes2.insert("s3");

  service_->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                  "refreshToken");

  // First request.
  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      scopes1, &consumer_));
  message_loop_.RunUntilIdle();

  EXPECT_EQ(0, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  EXPECT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  // Second request to the same set of scopes, should return the same token
  // without needing a network request.
  scoped_ptr<OAuth2TokenService::Request> request2(
      oauth2_service_->StartRequest(scopes1_same, &consumer_));
  message_loop_.RunUntilIdle();

  // No new network fetcher.
  EXPECT_EQ(fetcher, factory_.GetFetcherByID(0));
  EXPECT_EQ(2, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  // Third request to a new set of scopes, should return another token.
  scoped_ptr<OAuth2TokenService::Request> request3(
      oauth2_service_->StartRequest(scopes2, &consumer_));
  message_loop_.RunUntilIdle();
  EXPECT_EQ(2, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  fetcher = factory_.GetFetcherByID(0);
  EXPECT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token2", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(3, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token2", consumer_.last_token_);
}

TEST_F(OAuth2TokenServiceTest, SuccessAndExpirationAndFailure) {
  service_->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                  "refreshToken");

  // First request.
  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      std::set<std::string>(), &consumer_));
  message_loop_.RunUntilIdle();
  EXPECT_EQ(0, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  EXPECT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token", 0));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  // Second request must try to access the network as the token has expired.
  scoped_ptr<OAuth2TokenService::Request> request2(
      oauth2_service_->StartRequest(std::set<std::string>(), &consumer_));
  message_loop_.RunUntilIdle();
  EXPECT_EQ(1, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);

  // Network failure.
  fetcher = factory_.GetFetcherByID(0);
  EXPECT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_UNAUTHORIZED);
  fetcher->SetResponseString("");
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(1, consumer_.number_of_errors_);
}

TEST_F(OAuth2TokenServiceTest, SuccessAndExpirationAndSuccess) {
  service_->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                  "refreshToken");

  // First request.
  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      std::set<std::string>(), &consumer_));
  message_loop_.RunUntilIdle();
  EXPECT_EQ(0, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  EXPECT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token", 0));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  // Second request must try to access the network as the token has expired.
  scoped_ptr<OAuth2TokenService::Request> request2(
      oauth2_service_->StartRequest(std::set<std::string>(), &consumer_));
  message_loop_.RunUntilIdle();
  EXPECT_EQ(1, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);

  fetcher = factory_.GetFetcherByID(0);
  EXPECT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("another token", 0));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(2, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("another token", consumer_.last_token_);
}

TEST_F(OAuth2TokenServiceTest, RequestDeletedBeforeCompletion) {
  service_->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                  "refreshToken");

  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      std::set<std::string>(), &consumer_));
  message_loop_.RunUntilIdle();
  EXPECT_EQ(0, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  EXPECT_TRUE(fetcher);

  request.reset();

  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(0, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
}

TEST_F(OAuth2TokenServiceTest, RequestDeletedAfterCompletion) {
  service_->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                  "refreshToken");

  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      std::set<std::string>(), &consumer_));
  message_loop_.RunUntilIdle();
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  request.reset();

  EXPECT_EQ(1, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);
}

TEST_F(OAuth2TokenServiceTest, MultipleRequestsForTheSameScopesWithOneDeleted) {
  service_->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                  "refreshToken");

  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      std::set<std::string>(), &consumer_));
  message_loop_.RunUntilIdle();
  scoped_ptr<OAuth2TokenService::Request> request2(
      oauth2_service_->StartRequest(std::set<std::string>(), &consumer_));
  message_loop_.RunUntilIdle();

  request.reset();

  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
}

TEST_F(OAuth2TokenServiceTest, SuccessAndSignOutAndRequest) {
  service_->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                  "refreshToken");

  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      std::set<std::string>(), &consumer_));
  message_loop_.RunUntilIdle();
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  // Signs out
  service_->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                  "");
  service_->EraseTokensFromDB();

  request = oauth2_service_->StartRequest(std::set<std::string>(), &consumer_);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(fetcher, factory_.GetFetcherByID(0));
  EXPECT_EQ(1, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(1, consumer_.number_of_errors_);
}

TEST_F(OAuth2TokenServiceTest, SuccessAndSignOutAndSignInAndSuccess) {
  service_->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                  "refreshToken");
  std::set<std::string> scopes;
  scopes.insert("s1");
  scopes.insert("s2");

  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      scopes, &consumer_));
  message_loop_.RunUntilIdle();
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  // Signs out and signs in
  service_->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                  "");
  service_->EraseTokensFromDB();
  service_->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                  "refreshToken");

  request = oauth2_service_->StartRequest(scopes, &consumer_);
  message_loop_.RunUntilIdle();
  fetcher = factory_.GetFetcherByID(0);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("another token", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(2, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("another token", consumer_.last_token_);
}

TEST_F(OAuth2TokenServiceTest, PendingAndSignOutAndSignInAndSuccess) {
  service_->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                  "first refreshToken");
  std::set<std::string> scopes;
  scopes.insert("s1");
  scopes.insert("s2");

  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      scopes, &consumer_));
  message_loop_.RunUntilIdle();
  net::TestURLFetcher* fetcher1 = factory_.GetFetcherByID(0);

  // Note |request| is still pending.
  service_->EraseTokensFromDB();
  service_->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                  "second refreshToken");

  TestingOAuth2TokenServiceConsumer consumer2;
  scoped_ptr<OAuth2TokenService::Request> request2(
      oauth2_service_->StartRequest(scopes, &consumer2));
  message_loop_.RunUntilIdle();

  net::TestURLFetcher* fetcher2 = factory_.GetFetcherByID(0);
  fetcher2->set_response_code(net::HTTP_OK);
  fetcher2->SetResponseString(GetValidTokenResponse("second token", 3600));
  fetcher2->delegate()->OnURLFetchComplete(fetcher2);
  EXPECT_EQ(1, consumer2.number_of_correct_tokens_);
  EXPECT_EQ(0, consumer2.number_of_errors_);
  EXPECT_EQ("second token", consumer2.last_token_);

  fetcher1->set_response_code(net::HTTP_OK);
  fetcher1->SetResponseString(GetValidTokenResponse("first token", 3600));
  fetcher1->delegate()->OnURLFetchComplete(fetcher1);
  EXPECT_EQ(1, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("first token", consumer_.last_token_);
}

TEST_F(OAuth2TokenServiceTest, ServiceShutDownBeforeFetchComplete) {
  service_->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                  "refreshToken");
  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      std::set<std::string>(), &consumer_));
  message_loop_.RunUntilIdle();
  EXPECT_EQ(0, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);

  profile_.reset();

  EXPECT_EQ(0, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(1, consumer_.number_of_errors_);
}

TEST_F(OAuth2TokenServiceTest, RetryingConsumer) {
  service_->IssueAuthTokenForTest(GaiaConstants::kGaiaOAuth2LoginRefreshToken,
                                  "refreshToken");
  RetryingTestingOAuth2TokenServiceConsumer consumer(oauth2_service_);
  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      std::set<std::string>(), &consumer));
  message_loop_.RunUntilIdle();
  EXPECT_EQ(0, consumer.number_of_correct_tokens_);
  EXPECT_EQ(0, consumer.number_of_errors_);

  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  EXPECT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_UNAUTHORIZED);
  fetcher->SetResponseString("");
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(0, consumer.number_of_correct_tokens_);
  EXPECT_EQ(1, consumer.number_of_errors_);

  fetcher = factory_.GetFetcherByID(0);
  EXPECT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_UNAUTHORIZED);
  fetcher->SetResponseString("");
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(0, consumer.number_of_correct_tokens_);
  EXPECT_EQ(2, consumer.number_of_errors_);
}
