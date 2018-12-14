// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/earl_grey/js_test_util.h"

#import <EarlGrey/EarlGrey.h>
#import <WebKit/WebKit.h>

#import "base/test/ios/wait_util.h"
#import "ios/web/interstitials/web_interstitial_impl.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {

// Evaluates the given |script| on |interstitial|.
void ExecuteScriptForTesting(web::WebInterstitialImpl* interstitial,
                             NSString* script,
                             web::JavaScriptResultBlock handler) {
  DCHECK(interstitial);
  interstitial->ExecuteJavaScript(script, handler);
}

id ExecuteJavaScript(WebState* web_state,
                     NSString* javascript,
                     NSError* __autoreleasing* out_error) {
  __block bool did_complete = false;
  __block id result = nil;
  CRWJSInjectionReceiver* receiver = web_state->GetJSInjectionReceiver();
  [receiver executeJavaScript:javascript
            completionHandler:^(id value, NSError* error) {
              did_complete = true;
              result = [value copy];
              if (out_error)
                *out_error = [error copy];
            }];

  // Wait for completion.
  BOOL succeeded = WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return did_complete;
  });
  GREYAssert(succeeded, @"Script execution timed out");

  return result;
}

id ExecuteScriptOnInterstitial(WebState* web_state, NSString* script) {
  web::WebInterstitialImpl* interstitial =
      static_cast<web::WebInterstitialImpl*>(web_state->GetWebInterstitial());

  __block id script_result = nil;
  __block bool did_finish = false;
  web::ExecuteScriptForTesting(interstitial, script, ^(id result, NSError*) {
    script_result = [result copy];
    did_finish = true;
  });
  BOOL succeeded = WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return did_finish;
  });
  GREYAssert(succeeded, @"Script execution timed out");
  return script_result;
}

}  // namespace web
