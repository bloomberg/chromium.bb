// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/translate/ios/browser/js_translate_manager.h"

#include "base/strings/sys_string_conversions.h"
#include "components/grit/components_resources.h"
#import "ios/web/public/test/fakes/crw_test_js_injection_receiver.h"
#import "ios/web/public/test/js_test_util.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface JsTranslateManager (Testing)
- (double)performanceNow;
@end

@implementation JsTranslateManager (Testing)
// Returns the time in milliseconds.
- (double)performanceNow {
  id result = web::ExecuteJavaScript(self.receiver, @"performance.now()");
  return [result doubleValue];
}
@end

class JsTranslateManagerTest : public PlatformTest {
 protected:
  JsTranslateManagerTest() {
    receiver_ = [[CRWTestJSInjectionReceiver alloc] init];
    manager_ = [[JsTranslateManager alloc] initWithReceiver:receiver_];
    base::StringPiece script =
        ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
            IDR_TRANSLATE_JS);
    [manager_ setScript:base::SysUTF8ToNSString(script.as_string() +
                                                "('DummyKey');")];
  }

  bool IsDefined(NSString* name) {
    NSString* script =
        [NSString stringWithFormat:@"typeof %@ != 'undefined'", name];
    return [web::ExecuteJavaScript(receiver_, script) boolValue];
  }

  CRWTestJSInjectionReceiver* receiver_;
  JsTranslateManager* manager_;
};

// TODO(crbug.com/658619#c47): Test reported as flaky.
TEST_F(JsTranslateManagerTest, DISABLED_PerformancePlaceholder) {
  [manager_ inject];
  EXPECT_TRUE(IsDefined(@"performance"));
  EXPECT_TRUE(IsDefined(@"performance.now"));

  // Check that performance.now returns correct values.
  NSTimeInterval intervalInSeconds = 0.3;
  double startTime = [manager_ performanceNow];
  [NSThread sleepForTimeInterval:intervalInSeconds];
  double endTime = [manager_ performanceNow];
  double timeElapsed = endTime - startTime;
  // The tolerance is high to avoid flake.
  EXPECT_NEAR(timeElapsed, intervalInSeconds * 1000, 100);
}

TEST_F(JsTranslateManagerTest, Inject) {
  [manager_ inject];
  EXPECT_TRUE([manager_ hasBeenInjected]);
  EXPECT_EQ(nil, [manager_ script]);
  EXPECT_NSEQ(@NO,
              web::ExecuteJavaScript(manager_, @"cr.googleTranslate.libReady"));
}
