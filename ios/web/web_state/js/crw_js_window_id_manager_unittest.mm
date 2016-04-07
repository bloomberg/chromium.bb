// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/js/crw_js_window_id_manager.h"

#include "base/mac/scoped_nsobject.h"
#include "base/memory/ptr_util.h"
#import "ios/web/public/test/crw_test_js_injection_receiver.h"
#import "ios/web/public/test/js_test_util.h"
#include "ios/web/public/test/scoped_testing_web_client.h"
#include "ios/web/public/web_client.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace {

class JSWindowIDManagerTest : public PlatformTest {
 public:
  JSWindowIDManagerTest() : web_client_(base::WrapUnique(new web::WebClient)) {}

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    receiver_.reset([[CRWTestJSInjectionReceiver alloc] init]);
    manager_.reset([[CRWJSWindowIdManager alloc] initWithReceiver:receiver_]);
  }

  // Required for CRWJSWindowIdManager creation.
  base::scoped_nsobject<CRWTestJSInjectionReceiver> receiver_;
  // Testable CRWJSWindowIdManager.
  base::scoped_nsobject<CRWJSWindowIdManager> manager_;
  // WebClient required for getting early page script, which must be injected
  // before CRWJSWindowIdManager.
  web::ScopedTestingWebClient web_client_;
};

// Tests that window ID injection by a second manager results in a different
// window ID.
TEST_F(JSWindowIDManagerTest, WindowIDDifferentManager) {
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
