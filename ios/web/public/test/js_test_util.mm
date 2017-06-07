// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/js_test_util.h"

#import <WebKit/WebKit.h>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/testing/wait_util.h"
#import "ios/web/public/web_state/js/crw_js_injection_manager.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

id ExecuteJavaScript(CRWJSInjectionManager* manager, NSString* script) {
  __block NSString* result;
  __block bool completed = false;
  [manager executeJavaScript:script
           completionHandler:^(id execution_result, NSError* error) {
             DCHECK(!error);
             result = [execution_result copy];
             completed = true;
           }];

  BOOL success = testing::WaitUntilConditionOrTimeout(
      testing::kWaitForJSCompletionTimeout, ^{
        return completed;
      });
  // Log stack trace to provide some context.
  EXPECT_TRUE(success)
      << "CRWJSInjectionManager failed to complete javascript execution.\n"
      << base::SysNSStringToUTF8(
             [[NSThread callStackSymbols] componentsJoinedByString:@"\n"]);
  return result;
}

id ExecuteJavaScript(CRWJSInjectionReceiver* receiver, NSString* script) {
  CRWJSInjectionManager* manager =
      [[CRWJSInjectionManager alloc] initWithReceiver:receiver];
  return ExecuteJavaScript(manager, script);
}

id ExecuteJavaScript(WKWebView* web_view, NSString* script) {
  return ExecuteJavaScript(web_view, script, nullptr);
}

id ExecuteJavaScript(WKWebView* web_view,
                     NSString* script,
                     NSError* __unsafe_unretained* error) {
  __block id result;
  __block bool completed = false;
  __block NSError* temp_error = nil;
  [web_view evaluateJavaScript:script
             completionHandler:^(id script_result, NSError* script_error) {
               result = [script_result copy];
               temp_error = [script_error copy];
               completed = true;
             }];
  BOOL success = testing::WaitUntilConditionOrTimeout(
      testing::kWaitForJSCompletionTimeout, ^{
        return completed;
      });
  // Log stack trace to provide some context.
  EXPECT_TRUE(success) << "WKWebView failed to complete javascript execution.\n"
                       << base::SysNSStringToUTF8([[NSThread callStackSymbols]
                              componentsJoinedByString:@"\n"]);
  if (error) {
    NSError* __autoreleasing auto_released_error = temp_error;
    *error = auto_released_error;
  }
  return result;
}

}  // namespace web

