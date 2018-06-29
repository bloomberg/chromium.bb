// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/test/earl_grey/shell_earl_grey.h"

#import <EarlGrey/EarlGrey.h>

#import "ios/testing/wait_util.h"
#import "ios/web/public/test/earl_grey/js_test_util.h"
#include "ios/web/public/test/element_selector.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#include "ios/web/shell/test/app/navigation_test_util.h"
#import "ios/web/shell/test/app/web_shell_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ShellEarlGrey

+ (void)loadURL:(const GURL&)URL {
  web::shell_test_util::LoadUrl(URL);

  GREYCondition* condition =
      [GREYCondition conditionWithName:@"Wait for page to complete loading."
                                 block:^BOOL {
                                   return !web::shell_test_util::IsLoading();
                                 }];
  GREYAssert([condition waitWithTimeout:testing::kWaitForPageLoadTimeout],
             @"Page did not complete loading.");

  web::WebState* webState = web::shell_test_util::GetCurrentWebState();
  if (webState->ContentIsHTML())
    web::WaitUntilWindowIdInjected(webState);

  // Ensure any UI elements handled by EarlGrey become idle for any subsequent
  // EarlGrey steps.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

+ (void)waitForWebViewContainingText:(std::string)text {
  GREYCondition* condition = [GREYCondition
      conditionWithName:@"Wait for web view containing text"
                  block:^BOOL {
                    return web::test::IsWebViewContainingText(
                        web::shell_test_util::GetCurrentWebState(), text);
                  }];
  GREYAssert([condition waitWithTimeout:testing::kWaitForUIElementTimeout],
             @"Failed waiting for web view containing %s", text.c_str());
}

+ (void)waitForWebViewContainingElement:
    (const web::test::ElementSelector)selector {
  GREYCondition* condition = [GREYCondition
      conditionWithName:@"Wait for web view containing element"
                  block:^BOOL {
                    return web::test::IsWebViewContainingElement(
                        web::shell_test_util::GetCurrentWebState(), selector);
                  }];
  GREYAssert([condition waitWithTimeout:testing::kWaitForUIElementTimeout],
             @"Failed waiting for web view containing element %s",
             selector.GetSelectorDescription().c_str());
}

+ (void)waitForWebViewNotContainingElement:
    (const web::test::ElementSelector)selector {
  GREYCondition* condition = [GREYCondition
      conditionWithName:@"Wait for web view not containing element"
                  block:^BOOL {
                    return !web::test::IsWebViewContainingElement(
                        web::shell_test_util::GetCurrentWebState(), selector);
                  }];
  GREYAssert([condition waitWithTimeout:testing::kWaitForUIElementTimeout],
             @"Failed waiting for web view not containing element %s",
             selector.GetSelectorDescription().c_str());
}

@end
