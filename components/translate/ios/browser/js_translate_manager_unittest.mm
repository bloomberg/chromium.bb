// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/translate/ios/browser/js_translate_manager.h"

#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "grit/components_resources.h"
#import "ios/web/public/test/crw_test_js_injection_receiver.h"
#import "ios/web/public/test/js_test_util.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/resource/resource_bundle.h"

using base::Time;
using base::TimeDelta;

@interface JsTranslateManager (Testing)
- (double)performanceNow;
@end

@implementation JsTranslateManager (Testing)
// Returns the time in milliseconds.
- (double)performanceNow {
  NSString* result =
      web::EvaluateJavaScriptAsString(self.receiver, @"performance.now()");
  return [result doubleValue];
}
@end

class JsTranslateManagerTest : public PlatformTest {
 protected:
  JsTranslateManagerTest() {
    receiver_.reset([[CRWTestJSInjectionReceiver alloc] init]);
    manager_.reset([[JsTranslateManager alloc] initWithReceiver:receiver_]);
    base::StringPiece script =
        ResourceBundle::GetSharedInstance().GetRawDataResource(
            IDR_TRANSLATE_JS);
    [manager_ setScript:base::SysUTF8ToNSString(script.as_string() +
                                                "('DummyKey');")];
  }

  bool IsDefined(NSString* name) {
    NSString* script =
        [NSString stringWithFormat:@"typeof %@ != 'undefined'", name];
    return [web::EvaluateJavaScriptAsString(receiver_, script) isEqual:@"true"];
  }

  base::scoped_nsobject<CRWTestJSInjectionReceiver> receiver_;
  base::scoped_nsobject<JsTranslateManager> manager_;
};

TEST_F(JsTranslateManagerTest, PerformancePlaceholder) {
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
  // TODO(shreyasv): Switch to the util function in web/ once that CL lands.
  __block BOOL block_was_called = NO;
  [manager_ evaluate:@"cr.googleTranslate.libReady"
      stringResultHandler:^(NSString* result, NSError*) {
        block_was_called = YES;
        EXPECT_NSEQ(@"false", result);
      }];
  // TODO(shreyasv): Move to |WaitUntilCondition| once that is moved to ios/.
  const NSTimeInterval kTimeout = 5.0;
  Time startTime = Time::Now();
  while (!block_was_called &&
         (Time::Now() - startTime < TimeDelta::FromSeconds(kTimeout))) {
    NSDate* beforeDate = [NSDate dateWithTimeIntervalSinceNow:.01];
    [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                             beforeDate:beforeDate];
  }

  EXPECT_TRUE(block_was_called);
}
