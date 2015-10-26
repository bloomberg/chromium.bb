// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/js/crw_js_early_script_manager.h"

#include "base/mac/scoped_nsobject.h"
#import "ios/web/public/test/crw_test_js_injection_receiver.h"
#include "ios/web/public/web_client.h"
#import "ios/web/web_state/js/page_script_util.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace web {
namespace {

class CRWJSEarlyScriptManagerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    SetWebClient(&web_client_);
    receiver_.reset([[CRWTestJSInjectionReceiver alloc] init]);
    earlyScriptManager_.reset(static_cast<CRWJSEarlyScriptManager*>(
        [[receiver_ instanceOfClass:[CRWJSEarlyScriptManager class]] retain]));
  }
  void TearDown() override {
    SetWebClient(nullptr);
    PlatformTest::TearDown();
  }
  // Required for CRWJSEarlyScriptManager creation.
  base::scoped_nsobject<CRWTestJSInjectionReceiver> receiver_;
  // Testable CRWJSEarlyScriptManager.
  base::scoped_nsobject<CRWJSEarlyScriptManager> earlyScriptManager_;
  // WebClient required for getting early page script.
  WebClient web_client_;
};

// Tests that CRWJSEarlyScriptManager's content is the same as returned by
// web::GetEarlyPageScript.
TEST_F(CRWJSEarlyScriptManagerTest, Content) {
  NSString* injectionContent = [earlyScriptManager_ staticInjectionContent];
  NSString* earlyScript = GetEarlyPageScript([receiver_ webViewType]);
  // |earlyScript| is a substring of |injectionContent|. The latter wraps the
  // former with "if (typeof __gCrWeb !== 'object')" check to avoid multiple
  // injections.
  EXPECT_NE(NSNotFound,
            static_cast<NSInteger>(
                [injectionContent rangeOfString:earlyScript].location));
}

}  // namespace
}  // namespace web
