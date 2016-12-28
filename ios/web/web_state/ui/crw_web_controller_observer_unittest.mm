// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state/crw_web_controller_observer.h"

#import "base/mac/scoped_nsobject.h"
#import "ios/web/test/crw_fake_web_controller_observer.h"
#import "ios/web/test/web_test_with_web_controller.h"

namespace web {

// Test fixture to test web controller observing.
class CRWWebControllerObserverTest : public web::WebTestWithWebController {
 protected:
  void SetUp() override {
    web::WebTestWithWebController::SetUp();
    fake_web_controller_observer_.reset(
        [[CRWFakeWebControllerObserver alloc] init]);
    [web_controller() addObserver:fake_web_controller_observer_];
  }

  void TearDown() override {
    [web_controller() removeObserver:fake_web_controller_observer_];
    fake_web_controller_observer_.reset();
    web::WebTestWithWebController::TearDown();
  }

  base::scoped_nsobject<CRWFakeWebControllerObserver>
      fake_web_controller_observer_;
};

TEST_F(CRWWebControllerObserverTest, PageLoaded) {
  EXPECT_FALSE([fake_web_controller_observer_ pageLoaded]);
  LoadHtml(@"<p></p>");
  EXPECT_TRUE([fake_web_controller_observer_ pageLoaded]);
}

}  // namespace web
