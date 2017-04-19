// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/static_content/static_html_view_controller.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/history_test_util.h"
#include "ios/chrome/test/app/navigation_test_util.h"
#import "ios/chrome/test/app/static_html_view_test_util.h"
#import "ios/testing/wait_util.h"
#import "ios/web/public/test/earl_grey/js_test_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#import "ios/web/public/web_state/web_state.h"
#include "ui/base/l10n/l10n_util.h"

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

#pragma mark - Cookie Utilities

+ (NSDictionary*)cookies {
  NSString* const kGetCookiesScript =
      @"document.cookie ? document.cookie.split(/;\\s*/) : [];";

  // TODO(crbug.com/690057): Remove __unsafe_unretained once all callers of
  // |ExecuteJavaScript| are converted to ARC.
  NSError* __unsafe_unretained error = nil;
  id result = chrome_test_util::ExecuteJavaScript(kGetCookiesScript, &error);

  GREYAssertTrue(result && !error, @"Failed to get cookies.");

  NSArray* nameValuePairs = base::mac::ObjCCastStrict<NSArray>(result);
  NSMutableDictionary* cookies = [NSMutableDictionary dictionary];
  for (NSString* nameValuePair in nameValuePairs) {
    NSArray* cookieNameValue = [nameValuePair componentsSeparatedByString:@"="];
    GREYAssertEqual(2U, cookieNameValue.count, @"Cookie has invalid format.");

    NSString* cookieName = cookieNameValue[0];
    NSString* cookieValue = cookieNameValue[1];
    cookies[cookieName] = cookieValue;
  }

  return cookies;
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

+ (void)reload {
  base::scoped_nsobject<GenericChromeCommand> reloadCommand(
      [[GenericChromeCommand alloc] initWithTag:IDC_RELOAD]);
  chrome_test_util::RunCommandWithActiveViewController(reloadCommand);
  [ChromeEarlGrey waitForPageToFinishLoading];
}

+ (void)goBack {
  base::scoped_nsobject<GenericChromeCommand> reloadCommand(
      [[GenericChromeCommand alloc] initWithTag:IDC_BACK]);
  chrome_test_util::RunCommandWithActiveViewController(reloadCommand);
  [ChromeEarlGrey waitForPageToFinishLoading];
}

+ (void)goForward {
  base::scoped_nsobject<GenericChromeCommand> reloadCommand(
      [[GenericChromeCommand alloc] initWithTag:IDC_FORWARD]);
  chrome_test_util::RunCommandWithActiveViewController(reloadCommand);
  [ChromeEarlGrey waitForPageToFinishLoading];
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

+ (void)waitForErrorPage {
  NSString* const kErrorPageText =
      l10n_util::GetNSString(IDS_ERRORPAGES_HEADING_NOT_AVAILABLE);
  [self waitForStaticHTMLViewContainingText:kErrorPageText];
}

+ (void)waitForStaticHTMLViewContainingText:(NSString*)text {
  GREYCondition* condition = [GREYCondition
      conditionWithName:@"Wait for static HTML text."
                  block:^BOOL {
                    return chrome_test_util::StaticHtmlViewContainingText(
                        chrome_test_util::GetCurrentWebState(),
                        base::SysNSStringToUTF8(text));
                  }];
  GREYAssert([condition waitWithTimeout:testing::kWaitForUIElementTimeout],
             @"Failed to find static html view containing %@", text);
}

+ (void)waitForStaticHTMLViewNotContainingText:(NSString*)text {
  GREYCondition* condition = [GREYCondition
      conditionWithName:@"Wait for absence of static HTML text."
                  block:^BOOL {
                    return !chrome_test_util::StaticHtmlViewContainingText(
                        chrome_test_util::GetCurrentWebState(),
                        base::SysNSStringToUTF8(text));
                  }];
  GREYAssert([condition waitWithTimeout:testing::kWaitForUIElementTimeout],
             @"Failed, there was a static html view containing %@", text);
}

@end
