// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/web_test_with_web_state.h"

#include "base/ios/ios_util.h"
#include "ios/web/public/web_state/web_frame_util.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_observer.h"
#include "ios/web/web_state/web_frames_manager_impl.h"
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

// Verifies that the web frame passed to the observer is the main frame.
ACTION_P(VerifyMainWebFrame, web_state) {
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web::GetMainWebFrame(web_state), arg1);
}

// Verifies that the web frame passed to the observer is a child frame.
ACTION_P(VerifyChildWebFrame, web_state) {
  EXPECT_EQ(web_state, arg0);

  web::WebFramesManagerImpl* manager =
      web::WebFramesManagerImpl::FromWebState(web_state);
  auto frames = manager->GetAllWebFrames();
  EXPECT_TRUE(frames.end() != std::find(frames.begin(), frames.end(), arg1));
  EXPECT_NE(manager->GetMainWebFrame(), arg1);
}
}

namespace web {

typedef WebTestWithWebState WebFrameWebStateObserverInttest;

// Web frame events should be registered on HTTP navigation.
TEST_F(WebFrameWebStateObserverInttest, SingleWebFrameHTTP) {
  testing::StrictMock<WebStateObserverMock> observer;
  web_state()->AddObserver(&observer);
  EXPECT_CALL(observer, WebFrameDidBecomeAvailable(web_state(), testing::_))
      .WillOnce(VerifyMainWebFrame(web_state()));
  LoadHtml(@"<p></p>", GURL("http://testurl1"));
  EXPECT_CALL(observer, WebFrameDidBecomeAvailable(web_state(), testing::_))
      .WillOnce(VerifyMainWebFrame(web_state()));
  EXPECT_CALL(observer, WebFrameWillBecomeUnavailable(web_state(), testing::_))
      .WillOnce(VerifyMainWebFrame(web_state()));
  LoadHtml(@"<p></p>", GURL("http://testurl2"));
  web_state()->RemoveObserver(&observer);
}

// Web frame events should be registered on HTTPS navigation.
TEST_F(WebFrameWebStateObserverInttest, SingleWebFrameHTTPS) {
  testing::StrictMock<WebStateObserverMock> observer;
  web_state()->AddObserver(&observer);
  EXPECT_CALL(observer, WebFrameDidBecomeAvailable(web_state(), testing::_))
      .WillOnce(VerifyMainWebFrame(web_state()));
  LoadHtml(@"<p></p>", GURL("https://testurl1"));
  EXPECT_CALL(observer, WebFrameDidBecomeAvailable(web_state(), testing::_))
      .WillOnce(VerifyMainWebFrame(web_state()));
  EXPECT_CALL(observer, WebFrameWillBecomeUnavailable(web_state(), testing::_))
      .WillOnce(VerifyMainWebFrame(web_state()));
  LoadHtml(@"<p></p>", GURL("https://testurl2"));
  web_state()->RemoveObserver(&observer);
}

// Web frame event should be registered on HTTPS navigation with iframe.
TEST_F(WebFrameWebStateObserverInttest, TwoWebFrameHTTPS) {
  testing::StrictMock<WebStateObserverMock> observer;
  web_state()->AddObserver(&observer);
  EXPECT_CALL(observer, WebFrameDidBecomeAvailable(web_state(), testing::_))
      .Times(2)
      .WillOnce(VerifyChildWebFrame(web_state()))
      .WillOnce(VerifyMainWebFrame(web_state()));
  LoadHtml(@"<p><iframe/></p>", GURL("https://testurl1"));
  EXPECT_CALL(observer, WebFrameDidBecomeAvailable(web_state(), testing::_))
      .Times(2)
      .WillOnce(VerifyChildWebFrame(web_state()))
      .WillOnce(VerifyMainWebFrame(web_state()));
  EXPECT_CALL(observer, WebFrameWillBecomeUnavailable(web_state(), testing::_))
      .Times(2)
      .WillOnce(VerifyMainWebFrame(web_state()))
      .WillOnce(VerifyChildWebFrame(web_state()));
  LoadHtml(@"<p><iframe/></p>", GURL("https://testurl2"));
  web_state()->RemoveObserver(&observer);
}

}  // namespace web
