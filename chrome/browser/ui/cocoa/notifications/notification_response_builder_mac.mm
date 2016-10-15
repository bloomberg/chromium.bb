// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/notifications/notification_response_builder_mac.h"

#include "base/logging.h"
#include "chrome/browser/ui/cocoa/notifications/notification_constants_mac.h"

namespace {

// Make sure this Obj-C enum is kept in sync with the
// NotificationCommon::Operation enum.
// The latter cannot be reused because the XPC service is not aware of
// PlatformNotificationCenter.
enum NotificationOperation {
  NOTIFICATION_CLICK = 0,
  NOTIFICATION_CLOSE = 1,
  NOTIFICATION_SETTINGS = 2
};
}  // namespace

@implementation NotificationResponseBuilder

+ (NSDictionary*)buildDictionary:(NSUserNotification*)notification {
  NSString* origin =
      [[notification userInfo]
          objectForKey:notification_constants::kNotificationOrigin]
          ? [[notification userInfo]
                objectForKey:notification_constants::kNotificationOrigin]
          : @"";
  DCHECK([[notification userInfo]
      objectForKey:notification_constants::kNotificationId]);
  NSString* notificationId = [[notification userInfo]
      objectForKey:notification_constants::kNotificationId];

  DCHECK([[notification userInfo]
      objectForKey:notification_constants::kNotificationProfileId]);
  NSString* profileId = [[notification userInfo]
      objectForKey:notification_constants::kNotificationProfileId];

  DCHECK([[notification userInfo]
      objectForKey:notification_constants::kNotificationIncognito]);
  NSNumber* incognito = [[notification userInfo]
      objectForKey:notification_constants::kNotificationIncognito];
  NSNumber* notificationType = [[notification userInfo]
      objectForKey:notification_constants::kNotificationType];

  // Closed notifications are not activated.
  NotificationOperation operation =
      notification.activationType == NSUserNotificationActivationTypeNone
          ? NOTIFICATION_CLOSE
          : NOTIFICATION_CLICK;
  int buttonIndex = -1;

  // Determine whether the user clicked on a button, and if they did, whether it
  // was a developer-provided button or the mandatory Settings button.
  if (notification.activationType ==
      NSUserNotificationActivationTypeActionButtonClicked) {
    NSArray* alternateButtons = @[];
    if ([notification
            respondsToSelector:@selector(_alternateActionButtonTitles)]) {
      alternateButtons =
          [notification valueForKey:@"_alternateActionButtonTitles"];
    }

    bool multipleButtons = (alternateButtons.count > 0);

    // No developer actions, just the settings button.
    if (!multipleButtons) {
      operation = NOTIFICATION_SETTINGS;
      buttonIndex = -1;
    } else {
      // 0 based array containing.
      // Button 1
      // Button 2 (optional)
      // Settings
      NSNumber* actionIndex =
          [notification valueForKey:@"_alternateActionIndex"];
      operation = (actionIndex.unsignedLongValue == alternateButtons.count - 1)
                      ? NOTIFICATION_SETTINGS
                      : NOTIFICATION_CLICK;
      buttonIndex =
          (actionIndex.unsignedLongValue == alternateButtons.count - 1)
              ? -1
              : actionIndex.intValue;
    }
  }

  return @{
    notification_constants::kNotificationOrigin : origin,
    notification_constants::kNotificationId : notificationId,
    notification_constants::kNotificationProfileId : profileId,
    notification_constants::kNotificationIncognito : incognito,
    notification_constants::kNotificationType : notificationType,
    notification_constants::
    kNotificationOperation : [NSNumber numberWithInt:operation],
    notification_constants::
    kNotificationButtonIndex : [NSNumber numberWithInt:buttonIndex],
  };
}

@end
