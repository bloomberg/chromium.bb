// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for GaiaOAuthClient.

#include <string>

#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/test/base/testing_profile.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace {
// Responds as though OAuth returned from the server.
class MockOAuthFetcher : public net::TestURLFetcher {
 public:
  MockOAuthFetcher(int response_code,
                   int max_failure_count,
                   const GURL& url,
                   const std::string& results,
                   net::URLFetcher::RequestType request_type,
                   net::URLFetcherDelegate* d)
      : net::TestURLFetcher(0, url, d),
        max_failure_count_(max_failure_count),
        current_failure_count_(0) {
    set_url(url);
    set_response_code(response_code);
    SetResponseString(results);
  }

  virtual ~MockOAuthFetcher() { }

  virtual void Start() {
    if ((GetResponseCode() != net::HTTP_OK) && (max_failure_count_ != -1) &&
        (current_failure_count_ == max_failure_count_)) {
      set_response_code(net::HTTP_OK);
    }

    net::URLRequestStatus::Status code = net::URLRequestStatus::SUCCESS;
    if (GetResponseCode() != net::HTTP_OK) {
      code = net::URLRequestStatus::FAILED;
      current_failure_count_++;
    }
    set_status(net::URLRequestStatus(code, 0));

    delegate()->OnURLFetchComplete(this);
  }

 private:
  int max_failure_count_;
  int current_failure_count_;
  DISALLOW_COPY_AND_ASSIGN(MockOAuthFetcher);
};

class MockOAuthFetcherFactory : public net::URLFetcherFactory,
                                public net::ScopedURLFetcherFactory {
 public:
  MockOAuthFetcherFactory()
      : net::ScopedURLFetcherFactory(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
        response_code_(net::HTTP_OK) {
  }
  ~MockOAuthFetcherFactory() {}
  virtual net::URLFetcher* CreateURLFetcher(
      int id,
      const GURL& url,
      net::URLFetcher::RequestType request_type,
      net::URLFetcherDelegate* d) {
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
  int response_code = net::HTTP_INTERNAL_SERVER_ERROR;

  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnNetworkError(response_code))
      .Times(1);

  TestingProfile profile;

  MockOAuthFetcherFactory factory;
  factory.set_response_code(response_code);
  factory.set_max_failure_count(4);

  OAuthClientInfo client_info;
  client_info.client_id = "test_client_id";
  client_info.client_secret = "test_client_secret";
  GaiaOAuthClient auth(kGaiaOAuth2Url,
                       profile_.GetRequestContext());
  auth.GetTokensFromAuthCode(client_info, "auth_code", 2, &delegate);
}

TEST_F(GaiaOAuthClientTest, NetworkFailureRecover) {
  int response_code = net::HTTP_INTERNAL_SERVER_ERROR;

  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnGetTokensResponse(kTestRefreshToken, kTestAccessToken,
      kTestExpiresIn)).Times(1);

  TestingProfile profile;

  MockOAuthFetcherFactory factory;
  factory.set_response_code(response_code);
  factory.set_max_failure_count(4);
  factory.set_results(kDummyGetTokensResult);

  OAuthClientInfo client_info;
  client_info.client_id = "test_client_id";
  client_info.client_secret = "test_client_secret";
  GaiaOAuthClient auth(kGaiaOAuth2Url,
                       profile_.GetRequestContext());
  auth.GetTokensFromAuthCode(client_info, "auth_code", -1, &delegate);
}

TEST_F(GaiaOAuthClientTest, OAuthFailure) {
  int response_code = net::HTTP_BAD_REQUEST;

  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnOAuthError()).Times(1);

  TestingProfile profile;

  MockOAuthFetcherFactory factory;
  factory.set_response_code(response_code);
  factory.set_max_failure_count(-1);
  factory.set_results(kDummyGetTokensResult);

  OAuthClientInfo client_info;
  client_info.client_id = "test_client_id";
  client_info.client_secret = "test_client_secret";
  GaiaOAuthClient auth(kGaiaOAuth2Url,
                       profile_.GetRequestContext());
  auth.GetTokensFromAuthCode(client_info, "auth_code", -1, &delegate);
}


TEST_F(GaiaOAuthClientTest, GetTokensSuccess) {
  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnGetTokensResponse(kTestRefreshToken, kTestAccessToken,
      kTestExpiresIn)).Times(1);

  TestingProfile profile;

  MockOAuthFetcherFactory factory;
  factory.set_results(kDummyGetTokensResult);

  OAuthClientInfo client_info;
  client_info.client_id = "test_client_id";
  client_info.client_secret = "test_client_secret";
  GaiaOAuthClient auth(kGaiaOAuth2Url,
                       profile_.GetRequestContext());
  auth.GetTokensFromAuthCode(client_info, "auth_code", -1, &delegate);
}

TEST_F(GaiaOAuthClientTest, RefreshTokenSuccess) {
  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnRefreshTokenResponse(kTestAccessToken,
      kTestExpiresIn)).Times(1);

  TestingProfile profile;

  MockOAuthFetcherFactory factory;
  factory.set_results(kDummyRefreshTokenResult);

  OAuthClientInfo client_info;
  client_info.client_id = "test_client_id";
  client_info.client_secret = "test_client_secret";
  GaiaOAuthClient auth(kGaiaOAuth2Url,
                       profile_.GetRequestContext());
  auth.GetTokensFromAuthCode(client_info, "auth_code", -1, &delegate);
}
}  // namespace gaia
