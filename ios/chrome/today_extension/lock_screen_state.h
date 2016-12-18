// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TODAY_EXTENSION_LOCK_SCREEN_STATE_H_
#define IOS_CHROME_TODAY_EXTENSION_LOCK_SCREEN_STATE_H_

#import <Foundation/Foundation.h>

@protocol LockScreenStateDelegate;

// A shared object to track screen lock state. Object is statically allocated
// to get notification even when widget is not displayed.
@interface LockScreenState : NSObject

+ (instancetype)sharedInstance;

// Whether or not the screen is currently locked.
// The value can be temporary |NO| on lock screen if the widget is allocated
// in the 10 seconds following the lock.
@property(nonatomic, readonly) BOOL isScreenLocked;

// Sets the delegate to receive future |lockScreenStateDidChange:|
// notifications.
- (void)setDelegate:(id<LockScreenStateDelegate>)delegate;

@end

@protocol LockScreenStateDelegate<NSObject>

- (void)lockScreenStateDidChange:(LockScreenState*)lockScreenState;

@end

#endif  // IOS_CHROME_TODAY_EXTENSION_LOCK_SCREEN_STATE_H_
