// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"

#import "ios/chrome/test/app/history_test_util.h"
#import "ios/testing/nserror_util.h"

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

@end
