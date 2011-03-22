// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for GaiaOAuthClient.

#include <string>

#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/common/net/gaia/gaia_oauth_client.h"
#include "chrome/common/net/http_return.h"
#include "chrome/common/net/test_url_fetcher_factory.h"
#include "chrome/common/net/url_fetcher.h"
#include "chrome/test/testing_profile.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace {
// Responds as though OAuth returned from the server.
class MockOAuthFetcher : public URLFetcher {
 public:
  MockOAuthFetcher(int response_code,
                   int max_failure_count,
                   const GURL& url,
                   const std::string& results,
                   URLFetcher::RequestType request_type,
                   URLFetcher::Delegate* d)
    : URLFetcher(url, request_type, d),
      response_code_(response_code),
      max_failure_count_(max_failure_count),
      current_failure_count_(0),
      url_(url),
      results_(results) { }
  virtual ~MockOAuthFetcher() { }

  virtual void Start() {
    if ((response_code_ != RC_REQUEST_OK) && (max_failure_count_ != -1) &&
        (current_failure_count_ == max_failure_count_)) {
      response_code_ = RC_REQUEST_OK;
    }

    net::URLRequestStatus::Status code = net::URLRequestStatus::SUCCESS;
    if (response_code_ != RC_REQUEST_OK) {
      code = net::URLRequestStatus::FAILED;
      current_failure_count_++;
    }
    net::URLRequestStatus status(code, 0);
    delegate()->OnURLFetchComplete(this,
                                   url_,
                                   status,
                                   response_code_,
                                   ResponseCookies(),
                                   results_);
  }

 private:
  int response_code_;
  int max_failure_count_;
  int current_failure_count_;
  GURL url_;
  std::string results_;
  DISALLOW_COPY_AND_ASSIGN(MockOAuthFetcher);
};

class MockOAuthFetcherFactory : public URLFetcher::Factory {
 public:
  MockOAuthFetcherFactory()
      : response_code_(RC_REQUEST_OK) {}
  ~MockOAuthFetcherFactory() {}
  virtual URLFetcher* CreateURLFetcher(
      int id,
      const GURL& url,
      URLFetcher::RequestType request_type,
      URLFetcher::Delegate* d) {
    return new MockOAuthFetcher(
        response_code_,
        max_failure_count_,
        url,
        results_,
        request_type,
        d);
  }
  void set_response_code(int response_code) {
    response_code_ = response_code;
  }
  void set_max_failure_count(int count) {
    max_failure_count_ = count;
  }
  void set_results(const std::string& results) {
    results_ = results;
  }
 private:
  int response_code_;
  int max_failure_count_;
  std::string results_;
  DISALLOW_COPY_AND_ASSIGN(MockOAuthFetcherFactory);
};

const std::string kTestAccessToken = "1/fFAGRNJru1FTz70BzhT3Zg";
const std::string kTestRefreshToken =
    "1/6BMfW9j53gdGImsixUH6kU5RsR4zwI9lUVX-tqf8JXQ";
const int kTestExpiresIn = 3920;

const std::string kDummyGetTokensResult =
  "{\"access_token\":\"" + kTestAccessToken + "\","
  "\"expires_in\":" + base::IntToString(kTestExpiresIn) + ","
  "\"refresh_token\":\"" + kTestRefreshToken + "\"}";

const std::string kDummyRefreshTokenResult =
  "{\"access_token\":\"" + kTestAccessToken + "\","
  "\"expires_in\":" + base::IntToString(kTestExpiresIn) + "}";
}

namespace gaia {

class GaiaOAuthClientTest : public testing::Test {
 public:
  GaiaOAuthClientTest() {}

  TestingProfile profile_;
 protected:
  MessageLoop message_loop_;
};

class MockGaiaOAuthClientDelegate : public gaia::GaiaOAuthClient::Delegate {
 public:
  MockGaiaOAuthClientDelegate() {}
  ~MockGaiaOAuthClientDelegate() {}

  MOCK_METHOD3(OnGetTokensResponse, void(const std::string& refresh_token,
      const std::string& access_token, int expires_in_seconds));
  MOCK_METHOD2(OnRefreshTokenResponse, void(const std::string& access_token,
      int expires_in_seconds));
  MOCK_METHOD0(OnOAuthError, void());
  MOCK_METHOD1(OnNetworkError, void(int response_code));
};

TEST_F(GaiaOAuthClientTest, NetworkFailure) {
  int response_code = RC_INTERNAL_SERVER_ERROR;

  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnNetworkError(response_code))
      .Times(1);

  TestingProfile profile;

  MockOAuthFetcherFactory factory;
  URLFetcher::set_factory(&factory);
  factory.set_response_code(response_code);
  factory.set_max_failure_count(4);

  OAuthClientInfo client_info;
  client_info.client_id = "test_client_id";
  client_info.client_secret = "test_client_secret";
  GaiaOAuthClient auth(kGaiaOAuth2Url,
                       profile_.GetRequestContext());
  auth.GetTokensFromAuthCode(client_info, "auth_code", 2, &delegate);
  URLFetcher::set_factory(NULL);
}

TEST_F(GaiaOAuthClientTest, NetworkFailureRecover) {
  int response_code = RC_INTERNAL_SERVER_ERROR;

  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnGetTokensResponse(kTestRefreshToken, kTestAccessToken,
      kTestExpiresIn)).Times(1);

  TestingProfile profile;

  MockOAuthFetcherFactory factory;
  URLFetcher::set_factory(&factory);
  factory.set_response_code(response_code);
  factory.set_max_failure_count(4);
  factory.set_results(kDummyGetTokensResult);

  OAuthClientInfo client_info;
  client_info.client_id = "test_client_id";
  client_info.client_secret = "test_client_secret";
  GaiaOAuthClient auth(kGaiaOAuth2Url,
                       profile_.GetRequestContext());
  auth.GetTokensFromAuthCode(client_info, "auth_code", -1, &delegate);
  URLFetcher::set_factory(NULL);
}

TEST_F(GaiaOAuthClientTest, OAuthFailure) {
  int response_code = RC_BAD_REQUEST;

  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnOAuthError()).Times(1);

  TestingProfile profile;

  MockOAuthFetcherFactory factory;
  URLFetcher::set_factory(&factory);
  factory.set_response_code(response_code);
  factory.set_max_failure_count(-1);
  factory.set_results(kDummyGetTokensResult);

  OAuthClientInfo client_info;
  client_info.client_id = "test_client_id";
  client_info.client_secret = "test_client_secret";
  GaiaOAuthClient auth(kGaiaOAuth2Url,
                       profile_.GetRequestContext());
  auth.GetTokensFromAuthCode(client_info, "auth_code", -1, &delegate);
  URLFetcher::set_factory(NULL);
}


TEST_F(GaiaOAuthClientTest, GetTokensSuccess) {
  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnGetTokensResponse(kTestRefreshToken, kTestAccessToken,
      kTestExpiresIn)).Times(1);

  TestingProfile profile;

  MockOAuthFetcherFactory factory;
  URLFetcher::set_factory(&factory);
  factory.set_results(kDummyGetTokensResult);

  OAuthClientInfo client_info;
  client_info.client_id = "test_client_id";
  client_info.client_secret = "test_client_secret";
  GaiaOAuthClient auth(kGaiaOAuth2Url,
                       profile_.GetRequestContext());
  auth.GetTokensFromAuthCode(client_info, "auth_code", -1, &delegate);
  URLFetcher::set_factory(NULL);
}

TEST_F(GaiaOAuthClientTest, RefreshTokenSuccess) {
  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnRefreshTokenResponse(kTestAccessToken,
      kTestExpiresIn)).Times(1);

  TestingProfile profile;

  MockOAuthFetcherFactory factory;
  URLFetcher::set_factory(&factory);
  factory.set_results(kDummyRefreshTokenResult);

  OAuthClientInfo client_info;
  client_info.client_id = "test_client_id";
  client_info.client_secret = "test_client_secret";
  GaiaOAuthClient auth(kGaiaOAuth2Url,
                       profile_.GetRequestContext());
  auth.GetTokensFromAuthCode(client_info, "auth_code", -1, &delegate);
  URLFetcher::set_factory(NULL);
}
}  // namespace gaia
