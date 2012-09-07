// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for OAuth2MintTokenFetcher.

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_mint_token_consumer.h"
#include "google_apis/gaia/oauth2_mint_token_fetcher.h"
#include "googleurl/src/gurl.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_fetcher_factory.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using net::ResponseCookies;
using net::ScopedURLFetcherFactory;
using net::TestURLFetcher;
using net::URLFetcher;
using net::URLFetcherDelegate;
using net::URLFetcherFactory;
using net::URLRequestStatus;
using testing::_;
using testing::Return;

namespace {

static const char kValidTokenResponse[] =
    "{"
    "  \"token\": \"at1\","
    "  \"issueAdvice\": \"Auto\""
    "}";
static const char kTokenResponseNoAccessToken[] =
    "{"
    "  \"issueAdvice\": \"Auto\""
    "}";

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

class MockOAuth2MintTokenConsumer : public OAuth2MintTokenConsumer {
 public:
  MockOAuth2MintTokenConsumer() {}
  ~MockOAuth2MintTokenConsumer() {}

  MOCK_METHOD1(OnMintTokenSuccess, void(const std::string& access_token));
  MOCK_METHOD1(OnMintTokenFailure,
               void(const GoogleServiceAuthError& error));
};

}  // namespace

class OAuth2MintTokenFetcherTest : public testing::Test {
 public:
  OAuth2MintTokenFetcherTest()
    : ui_thread_(BrowserThread::UI, &message_loop_),
      fetcher_(&consumer_, profile_.GetRequestContext(), "test") {
    test_scopes_.push_back("scope1");
    test_scopes_.push_back("scope1");
  }

  virtual ~OAuth2MintTokenFetcherTest() { }

  virtual TestURLFetcher* SetupIssueToken(
      bool fetch_succeeds, int response_code, const std::string& body) {
    GURL url(GaiaUrls::GetInstance()->oauth2_issue_token_url());
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
  MockOAuth2MintTokenConsumer consumer_;
  TestingProfile profile_;
  OAuth2MintTokenFetcher fetcher_;
  std::vector<std::string> test_scopes_;
};

TEST_F(OAuth2MintTokenFetcherTest, GetAccessTokenRequestFailure) {
  TestURLFetcher* url_fetcher = SetupIssueToken(false, 0, "");
  EXPECT_CALL(consumer_, OnMintTokenFailure(_)).Times(1);
  fetcher_.Start("access_token1", "client1", test_scopes_, "extension1");
  fetcher_.OnURLFetchComplete(url_fetcher);
}

TEST_F(OAuth2MintTokenFetcherTest, GetAccessTokenResponseCodeFailure) {
  TestURLFetcher* url_fetcher = SetupIssueToken(
      false, net::HTTP_FORBIDDEN, "");
  EXPECT_CALL(consumer_, OnMintTokenFailure(_)).Times(1);
  fetcher_.Start("access_token1", "client1", test_scopes_, "extension1");
  fetcher_.OnURLFetchComplete(url_fetcher);
}

TEST_F(OAuth2MintTokenFetcherTest, Success) {
  TestURLFetcher* url_fetcher = SetupIssueToken(
      true, net::HTTP_OK, kValidTokenResponse);
  EXPECT_CALL(consumer_, OnMintTokenSuccess("at1")).Times(1);
  fetcher_.Start("access_token1", "client1", test_scopes_, "extension1");
  fetcher_.OnURLFetchComplete(url_fetcher);
}

TEST_F(OAuth2MintTokenFetcherTest, ParseMintTokenResponse) {
  {  // No body.
    TestURLFetcher url_fetcher(0, GURL("www.google.com"), NULL);

    std::string at;
    EXPECT_FALSE(OAuth2MintTokenFetcher::ParseMintTokenResponse(
        &url_fetcher, &at));
    EXPECT_TRUE(at.empty());
  }
  {  // Bad json.
    TestURLFetcher url_fetcher(0, GURL("www.google.com"), NULL);
    url_fetcher.SetResponseString("foo");

    std::string at;
    EXPECT_FALSE(OAuth2MintTokenFetcher::ParseMintTokenResponse(
        &url_fetcher, &at));
    EXPECT_TRUE(at.empty());
  }
  {  // Valid json: access token missing.
    TestURLFetcher url_fetcher(0, GURL("www.google.com"), NULL);
    url_fetcher.SetResponseString(kTokenResponseNoAccessToken);

    std::string at;
    EXPECT_FALSE(OAuth2MintTokenFetcher::ParseMintTokenResponse(
        &url_fetcher, &at));
    EXPECT_TRUE(at.empty());
  }
  {  // Valid json: all good.
    TestURLFetcher url_fetcher(0, GURL("www.google.com"), NULL);
    url_fetcher.SetResponseString(kValidTokenResponse);

    std::string at;
    EXPECT_TRUE(OAuth2MintTokenFetcher::ParseMintTokenResponse(
        &url_fetcher, &at));
    EXPECT_EQ("at1", at);
  }
}
