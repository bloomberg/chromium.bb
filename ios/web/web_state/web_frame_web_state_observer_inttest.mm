// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/web_test_with_web_state.h"

#include "base/ios/ios_util.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Mocks WebStateObserver navigation callbacks.
class WebStateObserverMock : public web::WebStateObserver {
 public:
  WebStateObserverMock() = default;

  MOCK_METHOD2(WebFrameDidBecomeAvailable,
               void(web::WebState*, web::WebFrame*));
  MOCK_METHOD2(WebFrameWillBecomeUnavailable,
               void(web::WebState*, web::WebFrame*));
  void WebStateDestroyed(web::WebState* web_state) override { NOTREACHED(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebStateObserverMock);
};
}

namespace web {

typedef WebTestWithWebState WebFrameWebStateObserverInttest;

// No web frame event should be register on HTTP navigation.
TEST_F(WebFrameWebStateObserverInttest, SingleWebFrameHTTP) {
  testing::StrictMock<WebStateObserverMock> observer;
  web_state()->AddObserver(&observer);
  LoadHtml(@"<p></p>", GURL("http://testurl1"));
  LoadHtml(@"<p></p>", GURL("http://testurl2"));
  web_state()->RemoveObserver(&observer);
}

// Web frame event should be register on HTTPS navigation on iOS 11+.
TEST_F(WebFrameWebStateObserverInttest, SingleWebFrameHTTPS) {
  // On iOS 10, the behavior is different and is tested in
  // SingleWebFrameHTTPSiOS10
  if (!base::ios::IsRunningOnIOS11OrLater()) {
    return;
  }
  testing::StrictMock<WebStateObserverMock> observer;
  web_state()->AddObserver(&observer);
  EXPECT_CALL(observer, WebFrameDidBecomeAvailable(web_state(), testing::_))
      .Times(1);
  LoadHtml(@"<p></p>", GURL("https://testurl1"));
  EXPECT_CALL(observer, WebFrameDidBecomeAvailable(web_state(), testing::_))
      .Times(1);
  EXPECT_CALL(observer, WebFrameWillBecomeUnavailable(web_state(), testing::_))
      .Times(1);
  LoadHtml(@"<p></p>", GURL("https://testurl2"));
  web_state()->RemoveObserver(&observer);
}

// Web frame event should be register on HTTPS navigation on iOS10.
TEST_F(WebFrameWebStateObserverInttest, SingleWebFrameHTTPSiOS10) {
  // On iOS 11+, the behavior is different and is tested in
  // SingleWebFrameHTTPS
  if (base::ios::IsRunningOnIOS11OrLater()) {
    return;
  }
  testing::StrictMock<WebStateObserverMock> observer;
  web_state()->AddObserver(&observer);
  LoadHtml(@"<p></p>", GURL("https://testurl1"));
  LoadHtml(@"<p></p>", GURL("https://testurl2"));
  web_state()->RemoveObserver(&observer);
}

// Web frame event should be register on HTTPS navigation on iOS11+.
TEST_F(WebFrameWebStateObserverInttest, TwoWebFrameHTTPS) {
  // On iOS 10, the behavior is different and is tested in
  // TwoWebFrameHTTPSiOS10
  if (!base::ios::IsRunningOnIOS11OrLater()) {
    return;
  }
  testing::StrictMock<WebStateObserverMock> observer;
  web_state()->AddObserver(&observer);
  EXPECT_CALL(observer, WebFrameDidBecomeAvailable(web_state(), testing::_))
      .Times(2);
  LoadHtml(@"<p><iframe/></p>", GURL("https://testurl1"));
  EXPECT_CALL(observer, WebFrameDidBecomeAvailable(web_state(), testing::_))
      .Times(2);
  EXPECT_CALL(observer, WebFrameWillBecomeUnavailable(web_state(), testing::_))
      .Times(2);
  LoadHtml(@"<p><iframe/></p>", GURL("https://testurl2"));
  web_state()->RemoveObserver(&observer);
}

// Web frame event should be register on HTTPS navigation on iOS10.
TEST_F(WebFrameWebStateObserverInttest, TwoWebFrameHTTPSiOS10) {
  // On iOS 11+, the behavior is different and is tested in
  // TwoWebFrameHTTPS
  if (base::ios::IsRunningOnIOS11OrLater()) {
    return;
  }
  testing::StrictMock<WebStateObserverMock> observer;
  web_state()->AddObserver(&observer);
  LoadHtml(@"<p><iframe/></p>", GURL("https://testurl1"));
  LoadHtml(@"<p><iframe/></p>", GURL("https://testurl2"));
  web_state()->RemoveObserver(&observer);
}

}  // namespace web
