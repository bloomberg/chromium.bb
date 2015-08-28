// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <WebKit/WebKit.h>

#import "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/signin/ios/browser/account_consistency_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "ios/web/public/test/test_browser_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "ios/web/public/test/web_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

namespace {
// URL of the Google domain where the X-CHROME-CONNECTED cookie is set/removed.
NSURL* const kGoogleUrl = [NSURL URLWithString:@"https://google.com/"];
// URL of the Youtube domain where the X-CHROME-CONNECTED cookie is set/removed.
NSURL* const kYoutubeUrl = [NSURL URLWithString:@"https://youtube.com/"];

// AccountConsistencyService specialization that fakes the creation of the
// WKWebView in order to mock it. This allows tests to intercept the calls to
// the Web view and control they are correct.
class FakeAccountConsistencyService : public AccountConsistencyService {
 public:
  FakeAccountConsistencyService(
      web::BrowserState* browser_state,
      scoped_refptr<content_settings::CookieSettings> cookie_settings,
      GaiaCookieManagerService* gaia_cookie_manager_service,
      SigninManager* signin_manager)
      : AccountConsistencyService(browser_state,
                                  cookie_settings,
                                  gaia_cookie_manager_service,
                                  signin_manager),
        mock_web_view_(nil) {}

 private:
  WKWebView* CreateWKWebView() override {
    if (!mock_web_view_) {
      mock_web_view_ = [OCMockObject niceMockForClass:[WKWebView class]];
    }
    return [mock_web_view_ retain];
  }
  id mock_web_view_;
};
}  // namespace

class AccountConsistencyServiceTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    web::BrowserState::GetActiveStateManager(&browser_state_)->SetActive(true);
    gaia_cookie_manager_service_.reset(new GaiaCookieManagerService(
        nullptr, GaiaConstants::kChromeSource, nullptr));
    signin_client_.reset(new TestSigninClient(nullptr));
    signin_manager_.reset(new FakeSigninManager(
        signin_client_.get(), nullptr, &account_tracker_service_, nullptr));
    account_consistency_service_.reset(new FakeAccountConsistencyService(
        &browser_state_, nullptr, gaia_cookie_manager_service_.get(),
        signin_manager_.get()));
  }

  void TearDown() override {
    account_consistency_service_->Shutdown();
    web::BrowserState::GetActiveStateManager(&browser_state_)->SetActive(false);
    PlatformTest::TearDown();
  }

  void FireGoogleSigninSucceeded() {
    account_consistency_service_->GoogleSigninSucceeded("", "", "");
  }

  void FireGoogleSignedOut() {
    account_consistency_service_->GoogleSignedOut("", "");
  }

  id GetMockWKWebView() { return account_consistency_service_->web_view_; }
  id GetNavigationDelegate() {
    return account_consistency_service_->navigation_delegate_;
  }

  // Creates test threads, necessary for ActiveStateManager that needs a UI
  // thread.
  web::TestWebThreadBundle thread_bundle_;
  AccountTrackerService account_tracker_service_;
  web::TestBrowserState browser_state_;
  // AccountConsistencyService being tested. Actually a
  // FakeAccountConsistencyService to be able to use a mock web view.
  scoped_ptr<AccountConsistencyService> account_consistency_service_;
  scoped_ptr<TestSigninClient> signin_client_;
  scoped_ptr<FakeSigninManager> signin_manager_;
  scoped_ptr<GaiaCookieManagerService> gaia_cookie_manager_service_;
};

// Tests whether the WKWebView is actually stopped when the browser state is
// inactive.
TEST_F(AccountConsistencyServiceTest, OnInactive) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();
  [[GetMockWKWebView() expect] stopLoading];
  web::BrowserState::GetActiveStateManager(&browser_state_)->SetActive(false);
  EXPECT_OCMOCK_VERIFY(GetMockWKWebView());
}

// Tests that cookies are set on the main Google domain after signin.
TEST_F(AccountConsistencyServiceTest, GoogleSigninSucceeded) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();
  void (^finishNavigation)(NSInvocation*) = ^(NSInvocation* invocation) {
    WKWebView* web_view = nil;
    [invocation getArgument:&web_view atIndex:0];
    [GetNavigationDelegate() webView:web_view didFinishNavigation:nil];
  };
  [static_cast<WKWebView*>([[GetMockWKWebView() expect] andDo:finishNavigation])
      loadHTMLString:[OCMArg any]
             baseURL:kGoogleUrl];
  [static_cast<WKWebView*>([[GetMockWKWebView() expect] andDo:finishNavigation])
      loadHTMLString:[OCMArg any]
             baseURL:kYoutubeUrl];
  FireGoogleSigninSucceeded();
  EXPECT_OCMOCK_VERIFY(GetMockWKWebView());
}

// Tests that cookies are removed from the domains they were set on after
// signout.
TEST_F(AccountConsistencyServiceTest, GoogleSignedOut) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();
  void (^finishNavigation)(NSInvocation*) = ^(NSInvocation* invocation) {
    WKWebView* web_view = nil;
    [invocation getArgument:&web_view atIndex:0];
    [GetNavigationDelegate() webView:web_view didFinishNavigation:nil];
  };
  [static_cast<WKWebView*>([[GetMockWKWebView() expect] andDo:finishNavigation])
      loadHTMLString:[OCMArg any]
             baseURL:kGoogleUrl];
  [static_cast<WKWebView*>([[GetMockWKWebView() expect] andDo:finishNavigation])
      loadHTMLString:[OCMArg any]
             baseURL:kYoutubeUrl];
  FireGoogleSignedOut();
  EXPECT_OCMOCK_VERIFY(GetMockWKWebView());
}

// Tests that pending cookie requests are correctly applied when the browser
// state becomes active.
TEST_F(AccountConsistencyServiceTest, ApplyOnActive) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();
  void (^finishNavigation)(NSInvocation*) = ^(NSInvocation* invocation) {
    WKWebView* web_view = nil;
    [invocation getArgument:&web_view atIndex:0];
    [GetNavigationDelegate() webView:web_view didFinishNavigation:nil];
  };

  // No request is made until the browser state is active, then a WKWebView and
  // its navigation delegate are created, and the requests are processed.
  [[GetMockWKWebView() expect] setNavigationDelegate:[OCMArg isNotNil]];
  [static_cast<WKWebView*>([[GetMockWKWebView() expect] andDo:finishNavigation])
      loadHTMLString:[OCMArg any]
             baseURL:kGoogleUrl];
  [static_cast<WKWebView*>([[GetMockWKWebView() expect] andDo:finishNavigation])
      loadHTMLString:[OCMArg any]
             baseURL:kYoutubeUrl];
  web::BrowserState::GetActiveStateManager(&browser_state_)->SetActive(false);
  FireGoogleSigninSucceeded();
  web::BrowserState::GetActiveStateManager(&browser_state_)->SetActive(true);
  EXPECT_OCMOCK_VERIFY(GetMockWKWebView());
}

// Tests that cookie request being processed is correctly cancelled when the
// browser state becomes inactives and correctly re-started later when the
// browser state becomes active.
TEST_F(AccountConsistencyServiceTest, CancelOnInactiveReApplyOnActive) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();
  void (^finishNavigation)(NSInvocation*) = ^(NSInvocation* invocation) {
    WKWebView* web_view = nil;
    [invocation getArgument:&web_view atIndex:0];
    [GetNavigationDelegate() webView:web_view didFinishNavigation:nil];
  };

  // The first request starts to get applied and get cancelled as the browser
  // state becomes inactive. It is resumed after the browser state becomes
  // active again.
  [static_cast<WKWebView*>([GetMockWKWebView() expect])
      loadHTMLString:[OCMArg any]
             baseURL:kGoogleUrl];
  [static_cast<WKWebView*>([[GetMockWKWebView() expect] andDo:finishNavigation])
      loadHTMLString:[OCMArg any]
             baseURL:kGoogleUrl];
  [static_cast<WKWebView*>([[GetMockWKWebView() expect] andDo:finishNavigation])
      loadHTMLString:[OCMArg any]
             baseURL:kYoutubeUrl];
  FireGoogleSigninSucceeded();
  web::BrowserState::GetActiveStateManager(&browser_state_)->SetActive(false);
  web::BrowserState::GetActiveStateManager(&browser_state_)->SetActive(true);
  EXPECT_OCMOCK_VERIFY(GetMockWKWebView());
}
