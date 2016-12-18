// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/today_extension/lock_screen_state.h"

#import <UIKit/UIKit.h>

#import "base/ios/weak_nsobject.h"

namespace {

// Name of the protected file used to detect if extension is triggered on lock
// screen.
NSString* const kLockScreenFileName = @"LockScreenDetector";

// Return the path of a file that will always have NSFileProtectionComplete
// protection option. This file cannot be accessed from the lock screen and can
// be used to detect if the device is locked.
NSString* LockedDeviceFilename() {
  NSString* path = [NSSearchPathForDirectoriesInDomains(
      NSLibraryDirectory, NSUserDomainMask, YES) objectAtIndex:0];
  return [path stringByAppendingPathComponent:kLockScreenFileName];
}

}  // namespace

@interface LockScreenState ()

- (instancetype)init;

// Returns if the protected file is accessible.
// Note: the function tests the access to protected file. Access is revoked 10
// seconds after screen is locked. So this method returns YES if screen has been
// locked for more than 10 seconds.
// Returns NO if CreateLockScreenDetector has never been called.
- (BOOL)isProtectedFileAccessible;

// Periodically checks (every second) if the protected file is still accessible.
// Checks |remainingTests| times.
- (void)recheckProtectedFile:(int)remainingTests;

// Creates a protected file that will be used to detect if screen is locked.
- (void)createLockScreenDetector;

@end

@implementation LockScreenState {
  id<LockScreenStateDelegate> _delegate;
  BOOL _screenLocked;
  BOOL _lockScreenStateKnown;
  BOOL _protectedFileCreated;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(screenLocked:)
               name:UIApplicationProtectedDataWillBecomeUnavailable
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(screenUnlocked:)
               name:UIApplicationProtectedDataDidBecomeAvailable
             object:nil];
    _delegate = nil;
  }
  return self;
}

+ (instancetype)sharedInstance {
  static LockScreenState* instance = [[LockScreenState alloc] init];
  return instance;
}

- (void)screenLocked:(id)arguments {
  _screenLocked = YES;
  _lockScreenStateKnown = YES;
  [_delegate lockScreenStateDidChange:self];
}

- (void)screenUnlocked:(id)arguments {
  _screenLocked = NO;
  _lockScreenStateKnown = YES;
  [_delegate lockScreenStateDidChange:self];
}

- (void)setDelegate:(id)delegate {
  _delegate = delegate;
}

- (BOOL)isScreenLocked {
  if (_lockScreenStateKnown) {
    return _screenLocked;
  }
  [self createLockScreenDetector];
  if (!_protectedFileCreated) {
    // File could not be created. Screen is locked.
    _screenLocked = YES;
    _lockScreenStateKnown = YES;
    return YES;
  }
  _screenLocked = ![self isProtectedFileAccessible];
  _lockScreenStateKnown = YES;

  if (!_screenLocked) {
    // This test is only valid if screen has been locked for more than 10
    // seconds (https://www.apple.com/business/docs/iOS_Security_Guide.pdf).
    // In that case, no notification is received after the delay.
    // Reschedule tests every second for 15 seconds.
    base::WeakNSObject<LockScreenState> weakSelf(self);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 static_cast<int64_t>(1 * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
                     [weakSelf recheckProtectedFile:14];
                   });
  }
  return _screenLocked;
}

- (void)recheckProtectedFile:(int)remainingTests {
  BOOL screenLocked = ![self isProtectedFileAccessible];
  if (screenLocked) {
    // |_screenLocked| may already be set to YES if we received a notification.
    if (!_screenLocked) {
      _screenLocked = YES;
      [_delegate lockScreenStateDidChange:self];
    }
    return;
  }
  if (remainingTests) {
    base::WeakNSObject<LockScreenState> weakSelf(self);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 static_cast<int64_t>(1 * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
                     [weakSelf recheckProtectedFile:remainingTests - 1];
                   });
  }
}

- (BOOL)isProtectedFileAccessible {
  NSError* error = nil;
  if (![[NSFileManager defaultManager]
          fileExistsAtPath:LockedDeviceFilename()]) {
    return NO;
  }
  [NSData dataWithContentsOfFile:LockedDeviceFilename()
                         options:NSDataReadingMappedIfSafe
                           error:&error];
  return !error;
}

- (void)createLockScreenDetector {
  if (_protectedFileCreated)
    return;
  NSError* error = nil;
  if ([[NSFileManager defaultManager]
          fileExistsAtPath:LockedDeviceFilename()]) {
    NSDictionary* attributes = [[NSFileManager defaultManager]
        attributesOfItemAtPath:LockedDeviceFilename()
                         error:&error];
    if ([[attributes valueForKey:NSFileProtectionKey]
            isEqualToString:NSFileProtectionComplete]) {
      _protectedFileCreated = YES;
      return;
    }
    BOOL removed =
        [[NSFileManager defaultManager] removeItemAtPath:LockedDeviceFilename()
                                                   error:&error];
    if (!removed || error) {
      // File could not be removed. FileSystem may be locked ?
      return;
    }
  }
  BOOL created = [[NSFileManager defaultManager]
      createFileAtPath:LockedDeviceFilename()
              contents:[@"Unlock" dataUsingEncoding:NSUTF8StringEncoding]
            attributes:@{NSFileProtectionKey : NSFileProtectionComplete}];
  if (!created) {
    // File could not be created. FileSystem may be locked ?
    return;
  }
  NSDictionary* attributes = [[NSFileManager defaultManager]
      attributesOfItemAtPath:LockedDeviceFilename()
                       error:&error];
  if ([[attributes valueForKey:NSFileProtectionKey]
          isEqualToString:NSFileProtectionComplete]) {
    _protectedFileCreated = YES;
    return;
  }
  [[NSFileManager defaultManager] removeItemAtPath:LockedDeviceFilename()
                                             error:&error];
}

@end
