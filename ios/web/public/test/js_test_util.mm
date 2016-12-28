// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/js_test_util.h"

#import <WebKit/WebKit.h>

#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/web_state/js/crw_js_injection_manager.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"

namespace web {

id ExecuteJavaScript(CRWJSInjectionManager* manager, NSString* script) {
  __block base::scoped_nsobject<NSString> result;
  __block bool completed = false;
  [manager executeJavaScript:script
           completionHandler:^(id execution_result, NSError* error) {
             DCHECK(!error);
             result.reset([execution_result copy]);
             completed = true;
           }];

  base::test::ios::WaitUntilCondition(^{
    return completed;
  });
  return [[result retain] autorelease];
}

id ExecuteJavaScript(CRWJSInjectionReceiver* receiver, NSString* script) {
  base::scoped_nsobject<CRWJSInjectionManager> manager(
      [[CRWJSInjectionManager alloc] initWithReceiver:receiver]);
  return ExecuteJavaScript(manager, script);
}

id ExecuteJavaScript(WKWebView* web_view, NSString* script) {
  return ExecuteJavaScript(web_view, script, nullptr);
}

id ExecuteJavaScript(WKWebView* web_view, NSString* script, NSError** error) {
  __block base::scoped_nsobject<id> result;
  __block bool completed = false;
  [web_view evaluateJavaScript:script
             completionHandler:^(id script_result, NSError* script_error) {
               result.reset([script_result copy]);
               if (error)
                 *error = [[script_error copy] autorelease];
               completed = true;
             }];
  base::test::ios::WaitUntilCondition(^{
    return completed;
  });
  return [[result retain] autorelease];
}

}  // namespace web

