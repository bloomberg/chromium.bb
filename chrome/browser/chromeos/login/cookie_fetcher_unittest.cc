// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>

#include <string>

#include "base/message_loop.h"
#include "chrome/browser/chromeos/login/client_login_response_handler.h"
#include "chrome/browser/chromeos/login/cookie_fetcher.h"
#include "chrome/browser/chromeos/login/issue_response_handler.h"
#include "chrome/browser/chromeos/login/mock_auth_response_handler.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/common/url_fetcher.h"
#include "content/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace chromeos {
using ::testing::Return;
using ::testing::Invoke;
using ::testing::Unused;
using ::testing::_;

class CookieFetcherTest : public testing::Test {
 public:
  CookieFetcherTest()
      : iat_url_(GaiaUrls::GetInstance()->issue_auth_token_url()),
        ta_url_(GaiaUrls::GetInstance()->token_auth_url()),
        client_login_data_("SID n' LSID"),
        token_("auth token"),
        ui_thread_(BrowserThread::UI, &message_loop_) {
  }

  const GURL iat_url_;
  const GURL ta_url_;
  const std::string client_login_data_;
  const std::string token_;
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  TestingProfile profile_;
};

// Check that successful HTTP responses from both end points results in
// the browser window getting put up.
TEST_F(CookieFetcherTest, SuccessfulFetchTest) {
  net::URLRequestStatus status(net::URLRequestStatus::SUCCESS, 0);

  MockAuthResponseHandler* cl_handler =
      new MockAuthResponseHandler(iat_url_, status, kHttpSuccess, token_);
  MockAuthResponseHandler* i_handler =
      new MockAuthResponseHandler(ta_url_, status, kHttpSuccess, std::string());

  CookieFetcher* cf = new CookieFetcher(NULL, cl_handler, i_handler);

  EXPECT_CALL(*cl_handler, Handle(client_login_data_, cf))
      .Times(1);

  EXPECT_CALL(*i_handler, CanHandle(iat_url_))
      .WillOnce(Return(true));
  EXPECT_CALL(*i_handler, CanHandle(ta_url_))
      .WillOnce(Return(false));
  EXPECT_CALL(*i_handler, Handle(token_, cf))
      .Times(1);

  cf->AttemptFetch(client_login_data_);
  message_loop_.RunAllPending();
}

// Check that a network failure when trying IssueAuthToken results in us bailing
// and putting up the browser window.
TEST_F(CookieFetcherTest, IssueAuthTokenNetworkFailureTest) {
  net::URLRequestStatus failed(net::URLRequestStatus::FAILED, ECONNRESET);

  MockAuthResponseHandler* cl_handler =
      new MockAuthResponseHandler(iat_url_, failed, kHttpSuccess, token_);
  // I expect nothing in i_handler to get called anyway
  MockAuthResponseHandler* i_handler =
      new MockAuthResponseHandler(ta_url_, failed, kHttpSuccess, std::string());

  CookieFetcher* cf = new CookieFetcher(&profile_,
                                        cl_handler,
                                        i_handler);

  EXPECT_CALL(*cl_handler, Handle(client_login_data_, cf))
      .Times(1);

  cf->AttemptFetch(client_login_data_);
  message_loop_.RunAllPending();
}

// Check that a network failure when trying TokenAuth results in us bailing
// and putting up the browser window.
TEST_F(CookieFetcherTest, TokenAuthNetworkFailureTest) {
  net::URLRequestStatus success;
  net::URLRequestStatus failed(net::URLRequestStatus::FAILED, ECONNRESET);

  MockAuthResponseHandler* cl_handler =
      new MockAuthResponseHandler(iat_url_, success, kHttpSuccess, token_);
  MockAuthResponseHandler* i_handler =
      new MockAuthResponseHandler(ta_url_, failed, 0, std::string());

  CookieFetcher* cf = new CookieFetcher(&profile_,
                                        cl_handler,
                                        i_handler);

  EXPECT_CALL(*cl_handler, Handle(client_login_data_, cf))
      .Times(1);

  EXPECT_CALL(*i_handler, CanHandle(iat_url_))
      .WillOnce(Return(true));
  EXPECT_CALL(*i_handler, Handle(token_, cf))
      .Times(1);

  cf->AttemptFetch(client_login_data_);
  message_loop_.RunAllPending();
}

// Check that an unsuccessful HTTP response when trying IssueAuthToken results
// in us bailing and putting up the browser window.
TEST_F(CookieFetcherTest, IssueAuthTokenDeniedTest) {
  net::URLRequestStatus success;

  MockAuthResponseHandler* cl_handler =
      new MockAuthResponseHandler(iat_url_, success, 403, std::string());
  // I expect nothing in i_handler to get called anyway.
  MockAuthResponseHandler* i_handler =
      new MockAuthResponseHandler(ta_url_, success, 0, std::string());

  CookieFetcher* cf = new CookieFetcher(&profile_,
                                        cl_handler,
                                        i_handler);

  EXPECT_CALL(*cl_handler, Handle(client_login_data_, cf))
      .Times(1);

  cf->AttemptFetch(client_login_data_);
  message_loop_.RunAllPending();
}

// Check that an unsuccessful HTTP response when trying TokenAuth results
// in us bailing and putting up the browser window.
TEST_F(CookieFetcherTest, TokenAuthDeniedTest) {
  net::URLRequestStatus success;

  MockAuthResponseHandler* cl_handler =
      new MockAuthResponseHandler(iat_url_,
                                  success,
                                  kHttpSuccess,
                                  token_);
  MockAuthResponseHandler* i_handler =
      new MockAuthResponseHandler(ta_url_, success, 403, std::string());

  CookieFetcher* cf = new CookieFetcher(&profile_,
                                        cl_handler,
                                        i_handler);

  EXPECT_CALL(*cl_handler, Handle(client_login_data_, cf))
      .Times(1);

  EXPECT_CALL(*i_handler, CanHandle(iat_url_))
      .WillOnce(Return(true));
  EXPECT_CALL(*i_handler, Handle(token_, cf))
      .Times(1);

  cf->AttemptFetch(client_login_data_);
  message_loop_.RunAllPending();
}

TEST_F(CookieFetcherTest, ClientLoginResponseHandlerTest) {
  ClientLoginResponseHandler handler(NULL);
  std::string input("a\nb\n");
  std::string expected("a&b&");
  expected.append(ClientLoginResponseHandler::kService);

  scoped_ptr<content::URLFetcher> fetcher(handler.Handle(input, NULL));
  EXPECT_EQ(expected, handler.payload());
}

TEST_F(CookieFetcherTest, IssueResponseHandlerTest) {
  IssueResponseHandler handler(NULL);
  std::string expected(IssueResponseHandler::BuildTokenAuthUrlWithToken(
      std::string("a\n")));

  scoped_ptr<content::URLFetcher> fetcher(
      handler.Handle(std::string("a\n"), NULL));
  EXPECT_EQ(expected, handler.token_url());
}

}  // namespace chromeos
