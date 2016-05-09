// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_EARL_GREY_WAIT_UTIL_H_
#define IOS_TESTING_EARL_GREY_WAIT_UTIL_H_

#import <Foundation/Foundation.h>

namespace testing {

// Constant for UI wait loop in seconds.
extern const NSTimeInterval kSpinDelaySeconds;

// Constant for timeout in seconds while waiting for UI element.
extern const NSTimeInterval kWaitForUIElementTimeout;
}

#endif  // IOS_TESTING_EARL_GREY_WAIT_UTIL_H_