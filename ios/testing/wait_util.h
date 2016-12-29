// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_WAIT_UTIL_H_
#define IOS_TESTING_WAIT_UTIL_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"

namespace testing {

// Constant for UI wait loop in seconds.
extern const NSTimeInterval kSpinDelaySeconds;

// Constant for timeout in seconds while waiting for UI element.
extern const NSTimeInterval kWaitForUIElementTimeout;

// Constant for timeout in seconds while waiting for JavaScript completion.
extern const NSTimeInterval kWaitForJSCompletionTimeout;

// Constant for timeout in seconds while waiting for a download to complete.
extern const NSTimeInterval kWaitForDownloadTimeout;

// Constant for timeout in seconds while waiting for a pageload to complete.
extern const NSTimeInterval kWaitForPageLoadTimeout;

// Returns true when condition() becomes true, otherwise returns false after
// |timeout|.
bool WaitUntilConditionOrTimeout(NSTimeInterval timeout,
                                 ConditionBlock condition);

}  // namespace testing

#endif  // IOS_TESTING_WAIT_UTIL_H_
