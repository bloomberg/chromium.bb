// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/earl_grey/js_test_util.h"

#import <EarlGrey/EarlGrey.h>
#import <WebKit/WebKit.h>

#include "base/timer/elapsed_timer.h"
#import "ios/testing/wait_util.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"

namespace web {

void WaitUntilWindowIdInjected(WebState* web_state) {
  bool is_window_id_injected = false;
  bool is_timeout = false;
  bool is_unrecoverable_error = false;

  base::ElapsedTimer timer;
  base::TimeDelta timeout =
      base::TimeDelta::FromSeconds(testing::kWaitForJSCompletionTimeout);

  // Keep polling until either the JavaScript execution returns with expected
  // value (indicating that Window ID is set), the timeout occurs, or an
  // unrecoverable error occurs.
  while (!is_window_id_injected && !is_timeout && !is_unrecoverable_error) {
    NSError* error = nil;
    id result = ExecuteJavaScript(web_state, @"0", &error);
    if (error) {
      is_unrecoverable_error = ![error.domain isEqual:WKErrorDomain] ||
                               error.code != WKErrorJavaScriptExceptionOccurred;
    } else {
      is_window_id_injected = [result isEqual:@0];
    }
    is_timeout = timeout < timer.Elapsed();
  }
  GREYAssertFalse(is_timeout, @"windowID injection timed out");
  GREYAssertFalse(is_unrecoverable_error, @"script execution error");
}

id ExecuteJavaScript(WebState* web_state,
                     NSString* javascript,
                     NSError** out_error) {
  __block BOOL did_complete = NO;
  __block id result = nil;
  CRWJSInjectionReceiver* receiver = web_state->GetJSInjectionReceiver();
  [receiver executeJavaScript:javascript
            completionHandler:^(id value, NSError* error) {
              did_complete = YES;
              result = [value copy];
              if (out_error)
                *out_error = [error copy];
            }];

  // Wait for completion.
  GREYCondition* condition = [GREYCondition
      conditionWithName:@"Wait for JavaScript execution to complete."
                  block:^{
                    return did_complete;
                  }];
  GREYAssert([condition waitWithTimeout:testing::kWaitForJSCompletionTimeout],
             @"Script execution timed out");

  if (out_error)
    [*out_error autorelease];
  return [result autorelease];
}

}  // namespace web
