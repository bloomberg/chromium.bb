// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/js/crw_js_window_id_manager.h"

#include "base/mac/scoped_nsobject.h"
#import "ios/web/public/test/crw_test_js_injection_receiver.h"
#include "ios/web/public/web_client.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace {

class JSWindowIDManagerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    receiver_.reset([[CRWTestJSInjectionReceiver alloc] init]);
    manager_.reset([[CRWJSWindowIdManager alloc] initWithReceiver:receiver_]);
    web::SetWebClient(&web_client_);
  }
  void TearDown() override {
    web::SetWebClient(nullptr);
    PlatformTest::TearDown();
  }
  // Required for CRWJSWindowIdManager creation.
  base::scoped_nsobject<CRWTestJSInjectionReceiver> receiver_;
  // Testable CRWJSWindowIdManager.
  base::scoped_nsobject<CRWJSWindowIdManager> manager_;
  // WebClient required for getting early page script, which must be injected
  // before CRWJSWindowIdManager.
  web::WebClient web_client_;
};

// TODO(jyquinn): enable this test (crbug.com/465898).
TEST_F(JSWindowIDManagerTest, DISABLED_WindowID) {
  EXPECT_TRUE(manager_.get());
  [manager_ inject];
  NSString* windowID = [manager_ windowId];
  EXPECT_EQ(32U, [windowID length]);
  // Inject a second time to check that the ID is different.
  [manager_ inject];
  NSString* windowID2 = [manager_ windowId];
  EXPECT_FALSE([windowID isEqualToString:windowID2]);
}

TEST_F(JSWindowIDManagerTest, WindowIDDifferent) {
  [manager_ inject];
  NSString* windowID = [manager_ windowId];
  base::scoped_nsobject<CRWTestJSInjectionReceiver> receiver2(
      [[CRWTestJSInjectionReceiver alloc] init]);
  base::scoped_nsobject<CRWJSWindowIdManager> manager2(
      [[CRWJSWindowIdManager alloc] initWithReceiver:receiver2]);
  [manager2 inject];
  NSString* windowID2 = [manager2 windowId];
  EXPECT_NSNE(windowID, windowID2);
}

}  // namespace
