// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/notifications/alert_notification_service.h"

#import "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/notifications/notification_builder_mac.h"

@class NSUserNotificationCenter;

@implementation AlertNotificationService

- (void)deliverNotification:(NSDictionary*)notificationData {
  base::scoped_nsobject<NotificationBuilder> builder(
      [[NotificationBuilder alloc] initWithDictionary:notificationData]);

  NSUserNotification* toast = [builder buildUserNotification];

  [[NSUserNotificationCenter defaultUserNotificationCenter]
      deliverNotification:toast];
}

@end
