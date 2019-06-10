// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/wpt/cwt_webdriver_app_interface.h"

#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/navigation_test_util.h"
#import "ios/testing/earl_grey/earl_grey_app.h"
#import "ios/testing/nserror_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;

@implementation CWTWebDriverAppInterface

+ (NSError*)loadURL:(NSString*)URL timeoutInSeconds:(NSTimeInterval)timeout {
  grey_dispatch_sync_on_main_thread(^{
    chrome_test_util::LoadUrl(GURL(base::SysNSStringToUTF8(URL)));
  });

  bool success = WaitUntilConditionOrTimeout(timeout, ^bool {
    __block BOOL isLoading = NO;
    grey_dispatch_sync_on_main_thread(^{
      isLoading = chrome_test_util::GetCurrentWebState()->IsLoading();
    });
    return !isLoading;
  });

  if (success)
    return nil;

  return testing::NSErrorWithLocalizedDescription(@"Page load timed out");
}

@end
