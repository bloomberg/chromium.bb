// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/signin/oauth2_token_service.h"
#include "chrome/browser/signin/oauth2_token_service_test_util.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/signin/token_service_unittest.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

// A testing consumer that retries on error.
class RetryingTestingOAuth2TokenServiceConsumer
    : public TestingOAuth2TokenServiceConsumer {
 public:
  RetryingTestingOAuth2TokenServiceConsumer(
      OAuth2TokenService* oauth2_service)
      : oauth2_service_(oauth2_service) {}
  virtual ~RetryingTestingOAuth2TokenServiceConsumer() {}

  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE {
    TestingOAuth2TokenServiceConsumer::OnGetTokenFailure(request, error);
    request_.reset(oauth2_service_->StartRequest(
        std::set<std::string>(), this).release());
  }

  OAuth2TokenService* oauth2_service_;
  scoped_ptr<OAuth2TokenService::Request> request_;
};

class TestOAuth2TokenService : public OAuth2TokenService {
 public:
  explicit TestOAuth2TokenService(net::TestURLRequestContextGetter* getter)
      : request_context_getter_(getter) {
  }

  void CancelAllRequestsForTest() { CancelAllRequests(); }

  void CancelRequestsForTokenForTest(const std::string& refresh_token) {
    CancelRequestsForToken(refresh_token);
  }

  // For testing: set the refresh token to be used.
  void set_refresh_token(const std::string& refresh_token) {
    refresh_token_ = refresh_token;
  }

 protected:
  virtual std::string GetRefreshToken() OVERRIDE { return refresh_token_; }

 private:
  // OAuth2TokenService implementation.
  virtual net::URLRequestContextGetter* GetRequestContext() OVERRIDE {
    return request_context_getter_.get();
  }

  std::string refresh_token_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;
};

class OAuth2TokenServiceTest : public TokenServiceTestHarness {
 public:
  virtual void SetUp() OVERRIDE {
    TokenServiceTestHarness::SetUp();
    oauth2_service_.reset(
        new TestOAuth2TokenService(new net::TestURLRequestContextGetter(
            BrowserThread::GetMessageLoopProxyForThread(
                BrowserThread::IO))));
  }

 protected:
  net::TestURLFetcherFactory factory_;
  scoped_ptr<TestOAuth2TokenService> oauth2_service_;
  TestingOAuth2TokenServiceConsumer consumer_;
};

TEST_F(OAuth2TokenServiceTest, NoOAuth2RefreshToken) {
  scoped_ptr<OAuth2TokenService::Request> request(
      oauth2_service_->StartRequest(std::set<std::string>(), &consumer_));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(1, consumer_.number_of_errors_);
}

TEST_F(OAuth2TokenServiceTest, FailureShouldNotRetry) {
  oauth2_service_->set_refresh_token("refreshToken");
  scoped_ptr<OAuth2TokenService::Request> request(
      oauth2_service_->StartRequest(std::set<std::string>(), &consumer_));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  EXPECT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_UNAUTHORIZED);
  fetcher->SetResponseString(std::string());
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(1, consumer_.number_of_errors_);
  EXPECT_EQ(fetcher, factory_.GetFetcherByID(0));
}

TEST_F(OAuth2TokenServiceTest, SuccessWithoutCaching) {
  oauth2_service_->set_refresh_token("refreshToken");
  scoped_ptr<OAuth2TokenService::Request> request(
      oauth2_service_->StartRequest(std::set<std::string>(), &consumer_));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  EXPECT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
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

  oauth2_service_->set_refresh_token("refreshToken");

  // First request.
  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      scopes1, &consumer_));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  EXPECT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  // Second request to the same set of scopes, should return the same token
  // without needing a network request.
  scoped_ptr<OAuth2TokenService::Request> request2(
      oauth2_service_->StartRequest(scopes1_same, &consumer_));
  base::RunLoop().RunUntilIdle();

  // No new network fetcher.
  EXPECT_EQ(fetcher, factory_.GetFetcherByID(0));
  EXPECT_EQ(2, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  // Third request to a new set of scopes, should return another token.
  scoped_ptr<OAuth2TokenService::Request> request3(
      oauth2_service_->StartRequest(scopes2, &consumer_));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  fetcher = factory_.GetFetcherByID(0);
  EXPECT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token2", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(3, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token2", consumer_.last_token_);
}

TEST_F(OAuth2TokenServiceTest, SuccessAndExpirationAndFailure) {
  oauth2_service_->set_refresh_token("refreshToken");

  // First request.
  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      std::set<std::string>(), &consumer_));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  EXPECT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token", 0));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  // Second request must try to access the network as the token has expired.
  scoped_ptr<OAuth2TokenService::Request> request2(
      oauth2_service_->StartRequest(std::set<std::string>(), &consumer_));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);

  // Network failure.
  fetcher = factory_.GetFetcherByID(0);
  EXPECT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_UNAUTHORIZED);
  fetcher->SetResponseString(std::string());
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(1, consumer_.number_of_errors_);
}

TEST_F(OAuth2TokenServiceTest, SuccessAndExpirationAndSuccess) {
  oauth2_service_->set_refresh_token("refreshToken");

  // First request.
  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      std::set<std::string>(), &consumer_));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  EXPECT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token", 0));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  // Second request must try to access the network as the token has expired.
  scoped_ptr<OAuth2TokenService::Request> request2(
      oauth2_service_->StartRequest(std::set<std::string>(), &consumer_));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);

  fetcher = factory_.GetFetcherByID(0);
  EXPECT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("another token", 0));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(2, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("another token", consumer_.last_token_);
}

TEST_F(OAuth2TokenServiceTest, RequestDeletedBeforeCompletion) {
  oauth2_service_->set_refresh_token("refreshToken");

  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      std::set<std::string>(), &consumer_));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  EXPECT_TRUE(fetcher);

  request.reset();

  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
}

TEST_F(OAuth2TokenServiceTest, RequestDeletedAfterCompletion) {
  oauth2_service_->set_refresh_token("refreshToken");

  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      std::set<std::string>(), &consumer_));
  base::RunLoop().RunUntilIdle();
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  request.reset();

  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);
}

TEST_F(OAuth2TokenServiceTest, MultipleRequestsForTheSameScopesWithOneDeleted) {
  oauth2_service_->set_refresh_token("refreshToken");

  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      std::set<std::string>(), &consumer_));
  base::RunLoop().RunUntilIdle();
  scoped_ptr<OAuth2TokenService::Request> request2(
      oauth2_service_->StartRequest(std::set<std::string>(), &consumer_));
  base::RunLoop().RunUntilIdle();

  request.reset();

  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
}

TEST_F(OAuth2TokenServiceTest, ClearedRefreshTokenFailsSubsequentRequests) {
  // We have a valid refresh token; the first request is successful.
  oauth2_service_->set_refresh_token("refreshToken");
  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      std::set<std::string>(), &consumer_));
  base::RunLoop().RunUntilIdle();
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  // The refresh token is no longer available; subsequent requests fail.
  oauth2_service_->set_refresh_token("");
  request = oauth2_service_->StartRequest(std::set<std::string>(), &consumer_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(fetcher, factory_.GetFetcherByID(0));
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(1, consumer_.number_of_errors_);
}

TEST_F(OAuth2TokenServiceTest,
       ChangedRefreshTokenDoesNotAffectInFlightRequests) {
  oauth2_service_->set_refresh_token("first refreshToken");
  std::set<std::string> scopes;
  scopes.insert("s1");
  scopes.insert("s2");

  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      scopes, &consumer_));
  base::RunLoop().RunUntilIdle();
  net::TestURLFetcher* fetcher1 = factory_.GetFetcherByID(0);

  // Note |request| is still pending when the refresh token changes.
  oauth2_service_->set_refresh_token("second refreshToken");

  // A 2nd request (using the new refresh token) that occurs and completes
  // while the 1st request is in flight is successful.
  TestingOAuth2TokenServiceConsumer consumer2;
  scoped_ptr<OAuth2TokenService::Request> request2(
      oauth2_service_->StartRequest(scopes, &consumer2));
  base::RunLoop().RunUntilIdle();

  net::TestURLFetcher* fetcher2 = factory_.GetFetcherByID(0);
  fetcher2->set_response_code(net::HTTP_OK);
  fetcher2->SetResponseString(GetValidTokenResponse("second token", 3600));
  fetcher2->delegate()->OnURLFetchComplete(fetcher2);
  EXPECT_EQ(1, consumer2.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer2.number_of_errors_);
  EXPECT_EQ("second token", consumer2.last_token_);

  fetcher1->set_response_code(net::HTTP_OK);
  fetcher1->SetResponseString(GetValidTokenResponse("first token", 3600));
  fetcher1->delegate()->OnURLFetchComplete(fetcher1);
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("first token", consumer_.last_token_);
}

TEST_F(OAuth2TokenServiceTest, ServiceShutDownBeforeFetchComplete) {
  oauth2_service_->set_refresh_token("refreshToken");
  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      std::set<std::string>(), &consumer_));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);

  // The destructor should cancel all in-flight fetchers.
  oauth2_service_.reset(NULL);

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(1, consumer_.number_of_errors_);
}

TEST_F(OAuth2TokenServiceTest, RetryingConsumer) {
  oauth2_service_->set_refresh_token("refreshToken");
  RetryingTestingOAuth2TokenServiceConsumer consumer(oauth2_service_.get());
  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      std::set<std::string>(), &consumer));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, consumer.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer.number_of_errors_);

  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_UNAUTHORIZED);
  fetcher->SetResponseString(std::string());
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(0, consumer.number_of_successful_tokens_);
  EXPECT_EQ(1, consumer.number_of_errors_);

  fetcher = factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_UNAUTHORIZED);
  fetcher->SetResponseString(std::string());
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(0, consumer.number_of_successful_tokens_);
  EXPECT_EQ(2, consumer.number_of_errors_);
}

TEST_F(OAuth2TokenServiceTest, InvalidateToken) {
  std::set<std::string> scopes;
  oauth2_service_->set_refresh_token("refreshToken");

  // First request.
  scoped_ptr<OAuth2TokenService::Request> request(
      oauth2_service_->StartRequest(scopes, &consumer_));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  EXPECT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  // Second request, should return the same token without needing a network
  // request.
  scoped_ptr<OAuth2TokenService::Request> request2(
      oauth2_service_->StartRequest(scopes, &consumer_));
  base::RunLoop().RunUntilIdle();

  // No new network fetcher.
  EXPECT_EQ(fetcher, factory_.GetFetcherByID(0));
  EXPECT_EQ(2, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  // Invalidating the token should return a new token on the next request.
  oauth2_service_->InvalidateToken(scopes, consumer_.last_token_);
  scoped_ptr<OAuth2TokenService::Request> request3(
      oauth2_service_->StartRequest(scopes, &consumer_));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  fetcher = factory_.GetFetcherByID(0);
  EXPECT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token2", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(3, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token2", consumer_.last_token_);
}

TEST_F(OAuth2TokenServiceTest, CancelAllRequests) {
  oauth2_service_->set_refresh_token("refreshToken");
  scoped_ptr<OAuth2TokenService::Request> request(
      oauth2_service_->StartRequest(std::set<std::string>(), &consumer_));

  oauth2_service_->set_refresh_token("refreshToken2");
  scoped_ptr<OAuth2TokenService::Request> request2(
      oauth2_service_->StartRequest(std::set<std::string>(), &consumer_));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);

  oauth2_service_->CancelAllRequestsForTest();

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(2, consumer_.number_of_errors_);
}

TEST_F(OAuth2TokenServiceTest, CancelRequestsForToken) {
  std::set<std::string> scope_set_1;
  scope_set_1.insert("scope1");
  scope_set_1.insert("scope2");
  std::set<std::string> scope_set_2(scope_set_1.begin(), scope_set_1.end());
  scope_set_2.insert("scope3");

  oauth2_service_->set_refresh_token("refreshToken");
  scoped_ptr<OAuth2TokenService::Request> request1(
      oauth2_service_->StartRequest(scope_set_1, &consumer_));
  scoped_ptr<OAuth2TokenService::Request> request2(
      oauth2_service_->StartRequest(scope_set_2, &consumer_));

  oauth2_service_->set_refresh_token("refreshToken2");
  scoped_ptr<OAuth2TokenService::Request> request3(
      oauth2_service_->StartRequest(scope_set_1, &consumer_));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);

  oauth2_service_->CancelRequestsForTokenForTest("refreshToken");

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(2, consumer_.number_of_errors_);

  oauth2_service_->CancelRequestsForTokenForTest("refreshToken2");

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(3, consumer_.number_of_errors_);
}
