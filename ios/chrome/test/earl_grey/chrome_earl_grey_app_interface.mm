// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/history_test_util.h"
#include "ios/chrome/test/app/navigation_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/testing/nserror_util.h"
#import "ios/web/public/test/earl_grey/js_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ChromeEarlGreyAppInterface

+ (NSError*)clearBrowsingHistory {
  if (chrome_test_util::ClearBrowsingHistory()) {
    return nil;
  }

  return testing::NSErrorWithLocalizedDescription(
      @"Clearing browser history timed out");
}

+ (void)startLoadingURL:(NSString*)URL {
  chrome_test_util::LoadUrl(GURL(base::SysNSStringToUTF8(URL)));
}

+ (BOOL)waitForWindowIDInjectionIfNeeded {
  web::WebState* webState = chrome_test_util::GetCurrentWebState();

  if (webState->ContentIsHTML()) {
    return web::WaitUntilWindowIdInjected(webState);
  }

  return YES;
}

+ (bool)isLoading {
  return chrome_test_util::IsLoading();
}

+ (void)openNewTab {
  chrome_test_util::OpenNewTab();
}

+ (void)openNewIncognitoTab {
  chrome_test_util::OpenNewIncognitoTab();
}

+ (void)closeAllTabsInCurrentMode {
  chrome_test_util::CloseAllTabsInCurrentMode();
}

+ (BOOL)closeAllIncognitoTabs {
  return chrome_test_util::CloseAllIncognitoTabs();
}
+ (void)goBack {
  [chrome_test_util::BrowserCommandDispatcherForMainBVC() goBack];
}

@end
