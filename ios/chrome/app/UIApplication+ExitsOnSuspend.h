// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_UIAPPLICATION_EXITSONSUSPEND_H_
#define IOS_CHROME_APP_UIAPPLICATION_EXITSONSUSPEND_H_

#if !defined(NDEBUG)

#import <UIKit/UIKit.h>

// Key in the UserDefaults for toggling exit on suspend.
extern NSString* const kExitsOnSuspend;

// WARNING: This is intended for non AppStore builds, since it's not allowed to
// quit an app programmatically. Internally we call exit(0).
@interface UIApplication (ExitsOnSuspend)

// When the app goes in background, terminate the app when all background tasks
// have finished. Use this in -applicationDidEnterBackground:.
- (void)cr_terminateWhenDoneWithBackgroundTasks;

// Cancel termination. Use this in -applicationWillEnterForeground:.
- (void)cr_cancelTermination;

@end

#endif  // !defined(NDEBUG)

#endif  // IOS_CHROME_APP_UIAPPLICATION_EXITSONSUSPEND_H_
