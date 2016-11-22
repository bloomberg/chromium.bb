// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_STUB_ALERT_DISPATCHER_
#define CHROME_BROWSER_NOTIFICATIONS_STUB_ALERT_DISPATCHER_

#import <Foundation/Foundation.h>

#import "chrome/browser/notifications/alert_dispatcher_mac.h"

@interface StubAlertDispatcher : NSObject<AlertDispatcher>

- (void)dispatchNotification:(NSDictionary*)data;

- (void)closeNotificationWithId:(NSString*)notificationId
                  withProfileId:(NSString*)profileId;

- (void)closeAllNotifications;

// Stub specific methods.
- (NSArray*)alerts;

@end

#endif  // CHROME_BROWSER_NOTIFICATIONS_STUB_ALERT_DISPATCHER_
