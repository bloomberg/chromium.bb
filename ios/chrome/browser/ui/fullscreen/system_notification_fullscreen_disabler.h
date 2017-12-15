// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_SYSTEM_NOTIFICATION_FULLSCREEN_DISABLER_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_SYSTEM_NOTIFICATION_FULLSCREEN_DISABLER_H_

#import <UIKit/UIKit.h>

class FullscreenController;

// Helper class that handles disabling fullscreen due to NSNotifications sent
// by system frameworks.  This class disables fullscreen:
// - when VoiceOver is enabled,
// - when the software keyboard is visible.
@interface SystemNotificationFullscreenDisabler : NSObject

// Designated initializer that disables |controller| for system notifications.
- (nullable instancetype)initWithController:
    (nonnull FullscreenController*)controller NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)init NS_UNAVAILABLE;

// Stops observing VoiceOver notifications.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_SYSTEM_NOTIFICATION_FULLSCREEN_DISABLER_H_
