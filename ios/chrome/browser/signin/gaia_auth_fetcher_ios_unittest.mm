// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/gaia_auth_fetcher_ios.h"

#import "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/signin/gaia_auth_fetcher_ios_private.h"
#include "ios/web/public/test/test_browser_state.h"
#include "ios/web/public/test/web_test_util.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

class MockGaiaConsumer : public GaiaAuthConsumer {
 public:
  MockGaiaConsumer() {}
  ~MockGaiaConsumer() {}

  MOCK_METHOD1(OnMergeSessionSuccess, void(const std::string& data));
  MOCK_METHOD1(OnClientLoginFailure, void(const GoogleServiceAuthError& error));
  MOCK_METHOD1(OnLogOutFailure, void(const GoogleServiceAuthError& error));
  MOCK_METHOD1(OnGetCheckConnectionInfoSuccess, void(const std::string& data));
};

// Tests fixture for GaiaAuthFetcherIOS
class GaiaAuthFetcherIOSTest : public PlatformTest {
 protected:
  void SetUp() override {
    CR_TEST_REQUIRES_WK_WEB_VIEW();
    mock_web_view_.reset(
        [[OCMockObject niceMockForClass:[WKWebView class]] retain]);
  }

  GaiaAuthFetcherIOSBridge* GetBridge(GaiaAuthFetcherIOS* gaia_auth_fetcher) {
    return gaia_auth_fetcher->bridge_.get();
  }

  GaiaAuthFetcherIOS* GetGaiaAuthFetcher(GaiaAuthConsumer* consumer,
                                         bool use_bridge) {
    GaiaAuthFetcherIOS* gaia_auth_fetcher = new GaiaAuthFetcherIOS(
        consumer, std::string(), GetRequestContext(), &browser_state_);
    if (use_bridge) {
      gaia_auth_fetcher->bridge_.reset(
          new GaiaAuthFetcherIOSBridge(gaia_auth_fetcher, &browser_state_));
      gaia_auth_fetcher->bridge_->web_view_.reset([mock_web_view_ retain]);
      [gaia_auth_fetcher->bridge_->web_view_
          setNavigationDelegate:gaia_auth_fetcher->bridge_
                                    ->navigation_delegate_];
    }
    return gaia_auth_fetcher;
  }

  net::TestURLRequestContextGetter* GetRequestContext() {
    if (!request_context_getter_.get()) {
      request_context_getter_ =
          new net::TestURLRequestContextGetter(message_loop_.task_runner());
    }
    return request_context_getter_.get();
  }

  // BrowserState, required for WKWebView creation.
  web::TestBrowserState browser_state_;
  base::MessageLoop message_loop_;
  base::scoped_nsobject<id> mock_web_view_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;
};

// Tests that the cancel mechanism works properly by cancelling an OAuthLogin
// request and controlling that the consumer is properly called.
TEST_F(GaiaAuthFetcherIOSTest, StartOAuthLoginCancelled) {
  if (!experimental_flags::IsWKWebViewEnabled()) {
    return;
  }

  GoogleServiceAuthError expected_error =
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED);
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnClientLoginFailure(expected_error)).Times(1);

  scoped_ptr<GaiaAuthFetcherIOS> gaia_auth_fetcher(
      GetGaiaAuthFetcher(&consumer, true));
  [static_cast<WKWebView*>([mock_web_view_ expect]) loadRequest:[OCMArg any]];

  gaia_auth_fetcher->StartOAuthLogin("fake_token", "gaia");
  gaia_auth_fetcher->CancelRequest();
  EXPECT_OCMOCK_VERIFY(mock_web_view_);
}

// Tests that the successful case works properly by starting a MergeSession
// request, making it succeed and controlling that the consumer is properly
// called.
TEST_F(GaiaAuthFetcherIOSTest, StartMergeSession) {
  if (!experimental_flags::IsWKWebViewEnabled()) {
    return;
  }

  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnMergeSessionSuccess("data")).Times(1);

  scoped_ptr<GaiaAuthFetcherIOS> gaia_auth_fetcher(
      GetGaiaAuthFetcher(&consumer, true));
  GaiaAuthFetcherIOSBridge* bridge = GetBridge(gaia_auth_fetcher.get());
  [static_cast<WKWebView*>([[mock_web_view_ expect] andDo:^(NSInvocation*) {
    bridge->URLFetchSuccess("data");
  }]) loadRequest:[OCMArg any]];

  gaia_auth_fetcher->StartMergeSession("uber_token", "");
  EXPECT_OCMOCK_VERIFY(mock_web_view_);
}

// Tests that the failure case works properly by starting a LogOut request,
// making it fail, and controlling that the consumer is properly called.
TEST_F(GaiaAuthFetcherIOSTest, StartLogOutError) {
  if (!experimental_flags::IsWKWebViewEnabled()) {
    return;
  }

  GoogleServiceAuthError expected_error =
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED);
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnLogOutFailure(expected_error)).Times(1);

  scoped_ptr<GaiaAuthFetcherIOS> gaia_auth_fetcher(
      GetGaiaAuthFetcher(&consumer, true));
  GaiaAuthFetcherIOSBridge* bridge = GetBridge(gaia_auth_fetcher.get());
  [static_cast<WKWebView*>([[mock_web_view_ expect] andDo:^(NSInvocation*) {
    bridge->URLFetchFailure(false);
  }]) loadRequest:[OCMArg any]];

  gaia_auth_fetcher->StartLogOut();
  EXPECT_OCMOCK_VERIFY(mock_web_view_);
}

// Tests that requests that do not require cookies are using the original
// GaiaAuthFetcher and not the GaiaAuthFetcherIOS specialization.
TEST_F(GaiaAuthFetcherIOSTest, StartGetCheckConnectionInfo) {
  std::string data(
      "[{\"carryBackToken\": \"token1\", \"url\": \"http://www.google.com\"}]");
  MockGaiaConsumer consumer;
  EXPECT_CALL(consumer, OnGetCheckConnectionInfoSuccess(data)).Times(1);

  // Set up the fake URL Fetcher.
  scoped_ptr<net::FakeURLFetcherFactory> fake_url_fetcher_factory(
      new net::FakeURLFetcherFactory(new net::URLFetcherImplFactory()));
  fake_url_fetcher_factory->SetFakeResponse(
      GaiaUrls::GetInstance()->GetCheckConnectionInfoURLWithSource(
          std::string()),
      data, net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  scoped_ptr<GaiaAuthFetcherIOS> gaia_auth_fetcher(
      GetGaiaAuthFetcher(&consumer, false));
  gaia_auth_fetcher->StartGetCheckConnectionInfo();
  message_loop_.RunUntilIdle();
}
