// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_APP_INTERFACE_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// ChromeEarlGreyAppInterface contains the app-side implementation for helpers
// that primarily work via direct model access. These helpers are compiled into
// the app binary and can be called from either app or test code.
@interface ChromeEarlGreyAppInterface : NSObject

// Clears browsing history and waits for history to finish clearing before
// returning. Returns nil on success, or else an NSError indicating why the
// operation failed.
+ (NSError*)clearBrowsingHistory;

@end

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_APP_INTERFACE_H_
