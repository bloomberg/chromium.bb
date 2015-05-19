// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/js_test_util.h"

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#import "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "ios/web/public/web_state/js/crw_js_injection_manager.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"

using base::Time;
using base::TimeDelta;

namespace {
// The amount of time (in secs) |evaluate:StringResultHandler:| is given time to
// process until the test fails.
const NSTimeInterval kTimeout = 5.0;
}

namespace web {

NSString* EvaluateJavaScriptAsString(CRWJSInjectionManager* manager,
                                     NSString* script) {
  __block base::scoped_nsobject<NSString> evaluationResult;
  [manager evaluate:script
      stringResultHandler:^(NSString* result, NSError* error) {
        DCHECK(result);
        evaluationResult.reset([result copy]);
      }];
  // TODO(shreyasv): This is a temporary solution to have some duplicated code.
  // The right way to implement this is to use |WaitUntilCondition|. Move to
  // that when that function lives in ios/.
  Time startTime = Time::Now();
  while (!evaluationResult.get() &&
         (Time::Now() - startTime < TimeDelta::FromSeconds(kTimeout))) {
    NSDate* beforeDate = [NSDate dateWithTimeIntervalSinceNow:.01];
    [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                             beforeDate:beforeDate];
  }
  DCHECK(evaluationResult);
  return [[evaluationResult retain] autorelease];
}

NSString* EvaluateJavaScriptAsString(CRWJSInjectionReceiver* receiver,
                                     NSString* script) {
  base::scoped_nsobject<CRWJSInjectionManager> manager(
      [[CRWJSInjectionManager alloc] initWithReceiver:receiver]);
  return EvaluateJavaScriptAsString(manager, script);
}

NSString* EvaluateJavaScriptAsString(UIWebView* web_view, NSString* script) {
  return [web_view stringByEvaluatingJavaScriptFromString:script];
}

id EvaluateJavaScript(WKWebView* web_view, NSString* script) {
  __block base::scoped_nsobject<id> result;
  [web_view evaluateJavaScript:script
             completionHandler:^(id evaluationResult, NSError* error) {
               DCHECK(!error);
               result.reset([evaluationResult copy]);
             }];
  base::test::ios::WaitUntilCondition(^bool() {
    return result;
  });
  return [[result retain] autorelease];
}

}  // namespace web

