// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/history_test_util.h"
#include "ios/chrome/test/app/navigation_test_util.h"
#import "ios/testing/wait_util.h"
#import "ios/web/public/test/earl_grey/js_test_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace chrome_test_util {

id ExecuteJavaScript(NSString* javascript,
                     NSError* __unsafe_unretained* out_error) {
  __block bool did_complete = false;
  __block id result = nil;
  __block NSError* temp_error = nil;
  CRWJSInjectionReceiver* evaluator =
      chrome_test_util::GetCurrentWebState()->GetJSInjectionReceiver();
  [evaluator executeJavaScript:javascript
             completionHandler:^(id value, NSError* error) {
               did_complete = true;
               result = [value copy];
               temp_error = [error copy];
             }];

  // Wait for completion.
  GREYCondition* condition = [GREYCondition
      conditionWithName:@"Wait for JavaScript execution to complete."
                  block:^BOOL {
                    return did_complete;
                  }];
  [condition waitWithTimeout:testing::kWaitForJSCompletionTimeout];
  if (!did_complete)
    return nil;
  if (out_error) {
    NSError* __autoreleasing auto_released_error = temp_error;
    *out_error = auto_released_error;
  }
  return result;
}

}  // namespace chrome_test_util

@implementation ChromeEarlGrey

#pragma mark - History Utilities

+ (void)clearBrowsingHistory {
  chrome_test_util::ClearBrowsingHistory();
  // After clearing browsing history via code, wait for the UI to be done
  // with any updates. This includes icons from the new tab page being removed.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

#pragma mark - Navigation Utilities

+ (void)loadURL:(GURL)URL {
  chrome_test_util::LoadUrl(URL);
  // Make sure that the page started loading.
  GREYAssert(chrome_test_util::IsLoading(), @"Page did not start loading.");
  [ChromeEarlGrey waitForPageToFinishLoading];

  web::WebState* webState = chrome_test_util::GetCurrentWebState();
  if (webState->ContentIsHTML())
    web::WaitUntilWindowIdInjected(webState);
}

+ (void)waitForPageToFinishLoading {
  GREYCondition* condition =
      [GREYCondition conditionWithName:@"Wait for page to complete loading."
                                 block:^BOOL {
                                   return !chrome_test_util::IsLoading();
                                 }];
  GREYAssert([condition waitWithTimeout:testing::kWaitForPageLoadTimeout],
             @"Page did not complete loading.");
}

+ (void)tapWebViewElementWithID:(NSString*)elementID {
  BOOL success =
      web::test::TapWebViewElementWithId(chrome_test_util::GetCurrentWebState(),
                                         base::SysNSStringToUTF8(elementID));
  GREYAssertTrue(success, @"Failed to tap web view element with ID: %@",
                 elementID);
}

@end
