// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"

#import <Foundation/Foundation.h>

#include "base/format_macros.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_error_util.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/nserror_util.h"
#include "ios/web/public/test/element_selector.h"

#if defined(CHROME_EARL_GREY_1)
#import <WebKit/WebKit.h>

#include "components/strings/grit/components_strings.h"  // nogncheck
#import "ios/chrome/browser/ui/static_content/static_html_view_controller.h"  // nogncheck
#import "ios/chrome/test/app/bookmarks_test_util.h"                // nogncheck
#import "ios/chrome/test/app/chrome_test_util.h"                   // nogncheck
#import "ios/chrome/test/app/history_test_util.h"                  // nogncheck
#include "ios/chrome/test/app/navigation_test_util.h"              // nogncheck
#import "ios/chrome/test/app/settings_test_util.h"                 // nogncheck
#import "ios/chrome/test/app/signin_test_util.h"                   // nogncheck
#import "ios/chrome/test/app/static_html_view_test_util.h"         // nogncheck
#import "ios/chrome/test/app/sync_test_util.h"                     // nogncheck
#import "ios/chrome/test/app/tab_test_util.h"                      // nogncheck
#import "ios/web/public/test/earl_grey/js_test_util.h"             // nogncheck
#import "ios/web/public/test/web_view_content_test_util.h"         // nogncheck
#import "ios/web/public/test/web_view_interaction_test_util.h"     // nogncheck
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"  // nogncheck
#import "ios/web/public/web_state/web_state.h"                     // nogncheck
#include "ui/base/l10n/l10n_util.h"                                // nogncheck
#endif

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if defined(CHROME_EARL_GREY_2)
GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(ChromeEarlGreyAppInterface)
#endif  // defined(CHROME_EARL_GREY_2)

@implementation ChromeEarlGreyImpl

#pragma mark - History Utilities

- (void)clearBrowsingHistory {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface clearBrowsingHistory]);

  // After clearing browsing history via code, wait for the UI to be done
  // with any updates. This includes icons from the new tab page being removed.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

@end

// The helpers below only compile under EarlGrey1.
// TODO(crbug.com/922813): Update these helpers to compile under EG2 and move
// them into the main class declaration as they are converted.
#if defined(CHROME_EARL_GREY_1)

namespace chrome_test_util {

id ExecuteJavaScript(NSString* javascript,
                     NSError* __autoreleasing* out_error) {
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

  bool success =
      WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
        return did_complete;
      });
  if (!success)
    return nil;
  if (out_error) {
    NSError* __autoreleasing auto_released_error = temp_error;
    *out_error = auto_released_error;
  }
  return result;
}

}  // namespace chrome_test_util

@implementation ChromeEarlGreyImpl (EG1)

#pragma mark - Cookie Utilities

- (NSDictionary*)cookies {
  NSString* const kGetCookiesScript =
      @"document.cookie ? document.cookie.split(/;\\s*/) : [];";

  NSError* error = nil;
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

- (NSError*)loadURL:(const GURL&)URL {
  chrome_test_util::LoadUrl(URL);
  bool pageLoaded = chrome_test_util::WaitForPageToFinishLoading();
  EG_TEST_HELPER_ASSERT_TRUE(pageLoaded, @"Page did not complete loading");

  web::WebState* webState = chrome_test_util::GetCurrentWebState();
  if (webState->ContentIsHTML()) {
    bool windowIDInjected = web::WaitUntilWindowIdInjected(webState);
    EG_TEST_HELPER_ASSERT_TRUE(windowIDInjected, @"WindowID failed to inject");
  }

  return nil;
}

- (NSError*)reload {
  [chrome_test_util::BrowserCommandDispatcherForMainBVC() reload];
  return [ChromeEarlGrey waitForPageToFinishLoading];
}

- (NSError*)goBack {
  [chrome_test_util::BrowserCommandDispatcherForMainBVC() goBack];

  return [ChromeEarlGrey waitForPageToFinishLoading];
}

- (NSError*)goForward {
  [chrome_test_util::BrowserCommandDispatcherForMainBVC() goForward];

  return [ChromeEarlGrey waitForPageToFinishLoading];
}

- (NSError*)openNewTab {
  chrome_test_util::OpenNewTab();
  NSError* error = [ChromeEarlGrey waitForPageToFinishLoading];
  if (!error) {
    [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  }
  return error;
}

- (NSError*)openNewIncognitoTab {
  chrome_test_util::OpenNewIncognitoTab();
  NSError* error = [ChromeEarlGrey waitForPageToFinishLoading];
  if (!error) {
    [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  }
  return error;
}

- (void)closeAllTabsInCurrentMode {
  chrome_test_util::CloseAllTabsInCurrentMode();
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

- (NSError*)closeAllIncognitoTabs {
  if (!chrome_test_util::CloseAllIncognitoTabs()) {
    return testing::NSErrorWithLocalizedDescription(@"Tabs did not close");
  }

  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  return nil;
}

- (void)closeCurrentTab {
  chrome_test_util::CloseCurrentTab();
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

- (NSError*)waitForPageToFinishLoading {
  if (!chrome_test_util::WaitForPageToFinishLoading()) {
    return testing::NSErrorWithLocalizedDescription(
        @"Page did not complete loading.");
  }

  return nil;
}

- (NSError*)tapWebViewElementWithID:(NSString*)elementID {
  BOOL success =
      web::test::TapWebViewElementWithId(chrome_test_util::GetCurrentWebState(),
                                         base::SysNSStringToUTF8(elementID));

  if (!success) {
    NSString* errorDescription = [NSString
        stringWithFormat:@"Failed to tap web view element with ID: %@",
                         elementID];
    return testing::NSErrorWithLocalizedDescription(errorDescription);
  }
  return nil;
}

- (NSError*)waitForErrorPage {
  NSString* const kErrorPageText =
      l10n_util::GetNSString(IDS_ERRORPAGES_HEADING_NOT_AVAILABLE);
  return [self waitForStaticHTMLViewContainingText:kErrorPageText];
}

- (NSError*)waitForStaticHTMLViewContainingText:(NSString*)text {
  bool success = WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return chrome_test_util::StaticHtmlViewContainingText(
        chrome_test_util::GetCurrentWebState(), base::SysNSStringToUTF8(text));
  });

  if (!success) {
    NSString* errorDescription = [NSString
        stringWithFormat:@"Failed to find static html view containing %@",
                         text];
    return testing::NSErrorWithLocalizedDescription(errorDescription);
  }

  return nil;
}

- (NSError*)waitForStaticHTMLViewNotContainingText:(NSString*)text {
  bool success = WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return !chrome_test_util::StaticHtmlViewContainingText(
        chrome_test_util::GetCurrentWebState(), base::SysNSStringToUTF8(text));
  });

  if (!success) {
    NSString* errorDescription = [NSString
        stringWithFormat:@"Failed, there was a static html view containing %@",
                         text];
    return testing::NSErrorWithLocalizedDescription(errorDescription);
  }

  return nil;
}

- (NSError*)waitForWebViewContainingText:(std::string)text {
  bool success = WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return web::test::IsWebViewContainingText(
        chrome_test_util::GetCurrentWebState(), text);
  });

  if (!success) {
    NSString* errorDescription =
        [NSString stringWithFormat:@"Failed waiting for web view containing %s",
                                   text.c_str()];
    return testing::NSErrorWithLocalizedDescription(errorDescription);
  }

  return nil;
}

- (NSError*)waitForWebViewContainingElement:(ElementSelector*)selector {
  bool success = WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return web::test::IsWebViewContainingElement(
        chrome_test_util::GetCurrentWebState(), selector);
  });

  if (!success) {
    NSString* errorDescription = [NSString
        stringWithFormat:@"Failed waiting for web view containing element %@",
                         selector.selectorDescription];
    return testing::NSErrorWithLocalizedDescription(errorDescription);
  }

  return nil;
}

- (NSError*)waitForWebViewNotContainingText:(std::string)text {
  bool success = WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return !web::test::IsWebViewContainingText(
        chrome_test_util::GetCurrentWebState(), text);
  });

  if (!success) {
    NSString* errorDescription = [NSString
        stringWithFormat:@"Failed waiting for web view not containing %s",
                         text.c_str()];
    return testing::NSErrorWithLocalizedDescription(errorDescription);
  }

  return nil;
}

- (NSError*)waitForMainTabCount:(NSUInteger)count {
  // Allow the UI to become idle, in case any tabs are being opened or closed.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  bool success = WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return chrome_test_util::GetMainTabCount() == count;
  });

  if (!success) {
    NSString* errorDescription =
        [NSString stringWithFormat:@"Failed waiting for main tab "
                                   @"count to become %" PRIuNS,
                                   count];
    return testing::NSErrorWithLocalizedDescription(errorDescription);
  }

  return nil;
}

- (NSError*)waitForIncognitoTabCount:(NSUInteger)count {
  // Allow the UI to become idle, in case any tabs are being opened or closed.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  bool success = WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return chrome_test_util::GetIncognitoTabCount() == count;
  });

  if (!success) {
    NSString* errorDescription =
        [NSString stringWithFormat:@"Failed waiting for incognito tab "
                                   @"count to become %" PRIuNS,
                                   count];
    return testing::NSErrorWithLocalizedDescription(errorDescription);
  }

  return nil;
}

- (NSError*)waitForWebViewContainingBlockedImageElementWithID:
    (std::string)imageID {
  bool success = web::test::WaitForWebViewContainingImage(
      imageID, chrome_test_util::GetCurrentWebState(),
      web::test::IMAGE_STATE_BLOCKED);

  if (!success) {
    NSString* errorDescription = [NSString
        stringWithFormat:@"Failed waiting for web view blocked image %s",
                         imageID.c_str()];
    return testing::NSErrorWithLocalizedDescription(errorDescription);
  }

  return nil;
}

- (NSError*)waitForWebViewContainingLoadedImageElementWithID:
    (std::string)imageID {
  bool success = web::test::WaitForWebViewContainingImage(
      imageID, chrome_test_util::GetCurrentWebState(),
      web::test::IMAGE_STATE_LOADED);

  if (!success) {
    NSString* errorDescription = [NSString
        stringWithFormat:@"Failed waiting for web view loaded image %s",
                         imageID.c_str()];
    return testing::NSErrorWithLocalizedDescription(errorDescription);
  }

  return nil;
}

- (NSError*)waitForBookmarksToFinishLoading {
  bool success = WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return chrome_test_util::BookmarksLoaded();
  });

  if (!success) {
    return testing::NSErrorWithLocalizedDescription(
        @"Bookmark model did not load");
  }

  return nil;
}

- (NSError*)clearBookmarks {
  bool success = chrome_test_util::ClearBookmarks();
  if (!success) {
    return testing::NSErrorWithLocalizedDescription(
        @"Not all bookmarks were removed.");
  }
  return nil;
}

- (NSError*)waitForElementWithMatcherSufficientlyVisible:
    (id<GREYMatcher>)matcher {
  bool success = WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:matcher]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  });

  if (!success) {
    NSString* errorDescription = [NSString
        stringWithFormat:
            @"Failed waiting for element with matcher %@ to become visible",
            matcher];
    return testing::NSErrorWithLocalizedDescription(errorDescription);
  }

  return nil;
}

#pragma mark - Sync Utilities

- (void)clearSyncServerData {
  chrome_test_util::ClearSyncServerData();
}

- (void)startSync {
  chrome_test_util::StartSync();
}

- (void)stopSync {
  chrome_test_util::StopSync();
}

- (NSError*)waitForSyncInitialized:(BOOL)isInitialized
                       syncTimeout:(NSTimeInterval)timeout {
  bool success = WaitUntilConditionOrTimeout(timeout, ^{
    return chrome_test_util::IsSyncInitialized() == isInitialized;
  });

  if (!success) {
    return testing::NSErrorWithLocalizedDescription(
        @"Sync was not initialized.");
  }

  return nil;
}

- (const std::string)syncCacheGUID {
  return chrome_test_util::GetSyncCacheGuid();
}

- (NSError*)waitForSyncServerEntitiesWithType:(syncer::ModelType)type
                                         name:(const std::string&)name
                                        count:(size_t)count
                                      timeout:(NSTimeInterval)timeout {
  __block NSError* error = nil;
  ConditionBlock condition = ^{
    NSError* __autoreleasing tempError = error;
    BOOL success = chrome_test_util::VerifyNumberOfSyncEntitiesWithName(
        type, name, count, &tempError);
    error = tempError;
    DCHECK(success || error);
    return !!success;
  };
  bool success = WaitUntilConditionOrTimeout(timeout, condition);
  if (error != nil) {
    return nil;
  }
  if (!success) {
    return testing::NSErrorWithLocalizedDescription(
        [NSString stringWithFormat:@"Expected %zu entities of the %d type.",
                                   count, type]);
  }
  return nil;
}

- (void)clearAutofillProfileWithGUID:(const std::string&)GUID {
  chrome_test_util::ClearAutofillProfile(GUID);
}

- (int)numberOfSyncEntitiesWithType:(syncer::ModelType)type {
  return chrome_test_util::GetNumberOfSyncEntities(type);
}

- (void)injectBookmarkOnFakeSyncServerWithURL:(const std::string&)URL
                                bookmarkTitle:(const std::string&)title {
  chrome_test_util::InjectBookmarkOnFakeSyncServer(URL, title);
}

- (void)injectAutofillProfileOnFakeSyncServerWithGUID:(const std::string&)GUID
                                  autofillProfileName:
                                      (const std::string&)fullName {
  chrome_test_util::InjectAutofillProfileOnFakeSyncServer(GUID, fullName);
}

- (BOOL)isAutofillProfilePresentWithGUID:(const std::string&)GUID
                     autofillProfileName:(const std::string&)fullName {
  return chrome_test_util::IsAutofillProfilePresent(GUID, fullName);
}

- (void)addTypedURL:(const GURL&)URL {
  chrome_test_util::AddTypedURLOnClient(URL);
}

- (void)triggerSyncCycleForType:(syncer::ModelType)type {
  chrome_test_util::TriggerSyncCycle(type);
}

- (NSError*)waitForTypedURL:(const GURL&)URL
              expectPresent:(BOOL)expectPresent
                    timeout:(NSTimeInterval)timeout {
  __block NSError* error = nil;
  ConditionBlock condition = ^{
    NSError* __autoreleasing tempError = error;
    BOOL success = chrome_test_util::IsTypedUrlPresentOnClient(
        URL, expectPresent, &tempError);
    error = tempError;
    DCHECK(success || error);
    return !!success;
  };
  bool success = WaitUntilConditionOrTimeout(timeout, condition);
  if (error != nil) {
    return nil;
  }
  if (!success) {
    return testing::NSErrorWithLocalizedDescription(
        @"Error occurred during typed URL verification.");
  }
  return nil;
}

- (void)deleteTypedURL:(const GURL&)URL {
  chrome_test_util::DeleteTypedUrlFromClient(URL);
}

- (void)injectTypedURLOnFakeSyncServer:(const std::string&)URL {
  chrome_test_util::InjectTypedURLOnFakeSyncServer(URL);
}

- (void)deleteAutofillProfileOnFakeSyncServerWithGUID:(const std::string&)GUID {
  chrome_test_util::DeleteAutofillProfileOnFakeSyncServer(GUID);
}

- (NSError*)verifySyncServerURLs:(const std::multiset<std::string>&)URLs {
  NSError* error = nil;
  NSError* __autoreleasing tempError = error;
  BOOL success = chrome_test_util::VerifySessionsOnSyncServer(URLs, &tempError);
  error = tempError;
  if (error != nil) {
    return error;
  }
  if (!success) {
    return testing::NSErrorWithLocalizedDescription(
        @"Error occurred during verification sessions.");
  }
  return nil;
}

- (void)setUpFakeSyncServer {
  chrome_test_util::SetUpFakeSyncServer();
}

- (void)tearDownFakeSyncServer {
  chrome_test_util::TearDownFakeSyncServer();
}

#pragma mark - Settings Utilities

- (NSError*)setContentSettings:(ContentSetting)setting {
  chrome_test_util::SetContentSettingsBlockPopups(setting);
  return nil;
}

#pragma mark - Sign Utilities

- (NSError*)signOutAndClearAccounts {
  bool success = chrome_test_util::SignOutAndClearAccounts();
  if (!success) {
    return testing::NSErrorWithLocalizedDescription(
        @"Real accounts couldn't be cleared.");
  }
  return nil;
}

@end

#endif  // defined(CHROME_EARL_GREY_1)
