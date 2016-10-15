// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/notifications/alert_notification_service.h"

#import "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/notifications/notification_builder_mac.h"
#include "chrome/browser/ui/cocoa/notifications/notification_constants_mac.h"

@class NSUserNotificationCenter;

@implementation AlertNotificationService

- (void)deliverNotification:(NSDictionary*)notificationData {
  base::scoped_nsobject<NotificationBuilder> builder(
      [[NotificationBuilder alloc] initWithDictionary:notificationData]);

  NSUserNotification* toast = [builder buildUserNotification];

  [[NSUserNotificationCenter defaultUserNotificationCenter]
      deliverNotification:toast];
}

- (void)closeNotificationWithId:(NSString*)notificationId
                  withProfileId:(NSString*)profileId {
  NSUserNotificationCenter* notificationCenter =
      [NSUserNotificationCenter defaultUserNotificationCenter];
  for (NSUserNotification* candidate in
       [notificationCenter deliveredNotifications]) {
    NSString* candidateId = [candidate.userInfo
        objectForKey:notification_constants::kNotificationId];

    NSString* candidateProfileId = [candidate.userInfo
        objectForKey:notification_constants::kNotificationProfileId];

    if ([candidateId isEqualToString:notificationId] &&
        [profileId isEqualToString:candidateProfileId]) {
      [notificationCenter removeDeliveredNotification:candidate];
      break;
    }
  }
}

- (void)closeAllNotifications {
  [[NSUserNotificationCenter defaultUserNotificationCenter]
      removeAllDeliveredNotifications];
}

@end
