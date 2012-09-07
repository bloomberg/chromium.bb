// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for OAuth2RevocationFetcher.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_revocation_consumer.h"
#include "google_apis/gaia/oauth2_revocation_fetcher.h"
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

class MockOAuth2RevocationConsumer : public OAuth2RevocationConsumer {
 public:
  MockOAuth2RevocationConsumer() {}
  ~MockOAuth2RevocationConsumer() {}

  MOCK_METHOD0(OnRevocationSuccess, void());
  MOCK_METHOD1(OnRevocationFailure,
               void(const GoogleServiceAuthError& error));
};

}  // namespace

class OAuth2RevocationFetcherTest : public testing::Test {
 public:
  OAuth2RevocationFetcherTest()
    : ui_thread_(BrowserThread::UI, &message_loop_),
      fetcher_(&consumer_, profile_.GetRequestContext()) {
  }

  virtual ~OAuth2RevocationFetcherTest() { }

  virtual TestURLFetcher* SetupRevocation(
      bool fetch_succeeds, int response_code) {
    GURL url = OAuth2RevocationFetcher::MakeRevocationUrl();
    TestURLFetcher* url_fetcher = new TestURLFetcher(0, url, &fetcher_);
    URLRequestStatus::Status status =
        fetch_succeeds ? URLRequestStatus::SUCCESS : URLRequestStatus::FAILED;
    url_fetcher->set_status(URLRequestStatus(status, 0));

    if (response_code != 0)
      url_fetcher->set_response_code(response_code);

    EXPECT_CALL(factory_, CreateURLFetcher(_, url, _, _))
        .WillOnce(Return(url_fetcher));
    return url_fetcher;
  }

 protected:
  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  MockUrlFetcherFactory factory_;
  MockOAuth2RevocationConsumer consumer_;
  TestingProfile profile_;
  OAuth2RevocationFetcher fetcher_;
};

TEST_F(OAuth2RevocationFetcherTest, RequestFailure) {
  TestURLFetcher* url_fetcher = SetupRevocation(false, 0);
  EXPECT_CALL(consumer_, OnRevocationFailure(_)).Times(1);
  fetcher_.Start("access_token", "client_id", "origin");
  fetcher_.OnURLFetchComplete(url_fetcher);
}

TEST_F(OAuth2RevocationFetcherTest, ResponseCodeFailure) {
  TestURLFetcher* url_fetcher = SetupRevocation(true, net::HTTP_FORBIDDEN);
  EXPECT_CALL(consumer_, OnRevocationFailure(_)).Times(1);
  fetcher_.Start("access_token", "client_id", "origin");
  fetcher_.OnURLFetchComplete(url_fetcher);
}

TEST_F(OAuth2RevocationFetcherTest, Success) {
  TestURLFetcher* url_fetcher = SetupRevocation(true, net::HTTP_NO_CONTENT);
  EXPECT_CALL(consumer_, OnRevocationSuccess()).Times(1);
  fetcher_.Start("access_token", "client_id", "origin");
  fetcher_.OnURLFetchComplete(url_fetcher);
}
