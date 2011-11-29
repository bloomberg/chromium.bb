// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for OAuth2AccessTokenFetcher.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/net/http_return.h"
#include "chrome/common/net/gaia/oauth2_access_token_consumer.h"
#include "chrome/common/net/gaia/oauth2_access_token_fetcher.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/common/url_fetcher.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "content/public/common/url_fetcher_factory.h"
#include "content/test/test_browser_thread.h"
#include "content/test/test_url_fetcher_factory.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using content::URLFetcher;
using content::URLFetcherDelegate;
using content::URLFetcherFactory;
using net::ResponseCookies;
using net::URLRequestStatus;
using testing::_;
using testing::Return;

namespace {
static const char kValidTokenResponse[] =
    "{"
    "  \"access_token\": \"at1\","
    "  \"expires_in\": 3600,"
    "  \"token_type\": \"Bearer\""
    "}";
static const char kTokenResponseNoAccessToken[] =
    "{"
    "  \"expires_in\": 3600,"
    "  \"token_type\": \"Bearer\""
    "}";
}

class MockUrlFetcherFactory : public ScopedURLFetcherFactory,
                              public URLFetcherFactory {
public:
  MockUrlFetcherFactory()
      : ScopedURLFetcherFactory(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  }
  virtual ~MockUrlFetcherFactory() {}

  MOCK_METHOD4(
      CreateURLFetcher,
      URLFetcher* (int id,
                   const GURL& url,
                   URLFetcher::RequestType request_type,
                   URLFetcherDelegate* d));
};

class MockOAuth2AccessTokenConsumer : public OAuth2AccessTokenConsumer {
 public:
  MockOAuth2AccessTokenConsumer() {}
  ~MockOAuth2AccessTokenConsumer() {}

  MOCK_METHOD1(OnGetTokenSuccess, void(const std::string& access_token));
  MOCK_METHOD1(OnGetTokenFailure,
               void(const GoogleServiceAuthError& error));
};

class OAuth2AccessTokenFetcherTest : public testing::Test {
 public:
  OAuth2AccessTokenFetcherTest()
    : ui_thread_(BrowserThread::UI, &message_loop_),
      fetcher_(&consumer_, profile_.GetRequestContext(), "test") {
  }

  virtual ~OAuth2AccessTokenFetcherTest() { }

  virtual TestURLFetcher* SetupGetAccessToken(
      bool fetch_succeeds, int response_code, const std::string& body) {
    GURL url(GaiaUrls::GetInstance()->oauth2_token_url());
    TestURLFetcher* url_fetcher = new TestURLFetcher(0, url, &fetcher_);
    URLRequestStatus::Status status =
        fetch_succeeds ? URLRequestStatus::SUCCESS : URLRequestStatus::FAILED;
    url_fetcher->set_status(URLRequestStatus(status, 0));

    if (response_code != 0)
      url_fetcher->set_response_code(response_code);

    if (!body.empty())
      url_fetcher->SetResponseString(body);

    EXPECT_CALL(factory_, CreateURLFetcher(_, url, _, _))
        .WillOnce(Return(url_fetcher));
    return url_fetcher;
  }

 protected:
  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  MockUrlFetcherFactory factory_;
  MockOAuth2AccessTokenConsumer consumer_;
  TestingProfile profile_;
  OAuth2AccessTokenFetcher fetcher_;
};

TEST_F(OAuth2AccessTokenFetcherTest, GetAccessTokenRequestFailure) {
  TestURLFetcher* url_fetcher = SetupGetAccessToken(false, 0, "");
  EXPECT_CALL(consumer_, OnGetTokenFailure(_)).Times(1);
  fetcher_.Start("client_id", "client_secret", "refresh_token");
  fetcher_.OnURLFetchComplete(url_fetcher);
}

TEST_F(OAuth2AccessTokenFetcherTest, GetAccessTokenResponseCodeFailure) {
  TestURLFetcher* url_fetcher = SetupGetAccessToken(true, RC_FORBIDDEN, "");
  EXPECT_CALL(consumer_, OnGetTokenFailure(_)).Times(1);
  fetcher_.Start("client_id", "client_secret", "refresh_token");
  fetcher_.OnURLFetchComplete(url_fetcher);
}

TEST_F(OAuth2AccessTokenFetcherTest, Success) {
  TestURLFetcher* url_fetcher = SetupGetAccessToken(
      true, RC_REQUEST_OK, kValidTokenResponse);
  EXPECT_CALL(consumer_, OnGetTokenSuccess("at1")).Times(1);
  fetcher_.Start("client_id", "client_secret", "refresh_token");
  fetcher_.OnURLFetchComplete(url_fetcher);
}

TEST_F(OAuth2AccessTokenFetcherTest, ParseGetAccessTokenResponse) {
  {  // No body.
    TestURLFetcher url_fetcher(0, GURL("www.google.com"), NULL);

    std::string at;
    EXPECT_FALSE(OAuth2AccessTokenFetcher::ParseGetAccessTokenResponse(
        &url_fetcher, &at));
    EXPECT_TRUE(at.empty());
  }
  {  // Bad json.
    TestURLFetcher url_fetcher(0, GURL("www.google.com"), NULL);
    url_fetcher.SetResponseString("foo");

    std::string at;
    EXPECT_FALSE(OAuth2AccessTokenFetcher::ParseGetAccessTokenResponse(
        &url_fetcher, &at));
    EXPECT_TRUE(at.empty());
  }
  {  // Valid json: access token missing.
    TestURLFetcher url_fetcher(0, GURL("www.google.com"), NULL);
    url_fetcher.SetResponseString(kTokenResponseNoAccessToken);

    std::string at;
    EXPECT_FALSE(OAuth2AccessTokenFetcher::ParseGetAccessTokenResponse(
        &url_fetcher, &at));
    EXPECT_TRUE(at.empty());
  }
  {  // Valid json: all good.
    TestURLFetcher url_fetcher(0, GURL("www.google.com"), NULL);
    url_fetcher.SetResponseString(kValidTokenResponse);

    std::string at;
    EXPECT_TRUE(OAuth2AccessTokenFetcher::ParseGetAccessTokenResponse(
        &url_fetcher, &at));
    EXPECT_EQ("at1", at);
  }
}
