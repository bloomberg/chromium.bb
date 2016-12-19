// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_REAUTHENTICATION_PROTOCOL_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_REAUTHENTICATION_PROTOCOL_H_

#import <Foundation/Foundation.h>

@protocol ReauthenticationProtocol<NSObject>

// Checks whether Touch ID and/or passcode is enabled for the device.
- (BOOL)canAttemptReauth;

// Attempts to reauthenticate the user with Touch ID or passcode if Touch ID is
// not available and the device is running iOS 9. |handler|
// will take action depending on the result of the reauth attempt.
- (void)attemptReauthWithLocalizedReason:(NSString*)localizedReason
                                 handler:(void (^)(BOOL success))handler;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_REAUTHENTICATION_PROTOCOL_H_
