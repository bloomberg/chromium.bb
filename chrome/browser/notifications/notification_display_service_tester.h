// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_TESTER_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_TESTER_H_

#include <vector>

#include "base/macros.h"
#include "chrome/browser/notifications/notification_common.h"

class Notification;
class Profile;
class StubNotificationDisplayService;

// Helper class that enables use of the NotificationDisplayService in tests. The
// Profile* passed when constructing an instance must outlive this class, as
// the service (or internals of the service) may be overridden.
//
// This class must only be used for testing purposes.
class NotificationDisplayServiceTester {
 public:
  explicit NotificationDisplayServiceTester(Profile* profile);
  ~NotificationDisplayServiceTester();

  // Synchronously gets a vector of the displayed Notifications for the |type|.
  std::vector<Notification> GetDisplayedNotificationsForType(
      NotificationCommon::Type type);

  // Simulates the notification identified by |notification_id| being closed due
  // to external events, such as the user dismissing it when |by_user| is set.
  // When |silent| is set, the notification handlers won't be informed of the
  // change to immitate behaviour of operating systems that don't inform apps
  // about removed notifications.
  void RemoveNotification(NotificationCommon::Type type,
                          const std::string& notification_id,
                          bool by_user,
                          bool silent = false);

  // Removes all notifications of the given |type|.
  void RemoveAllNotifications(NotificationCommon::Type type, bool by_user);

 private:
  Profile* profile_;
  StubNotificationDisplayService* display_service_;

  DISALLOW_COPY_AND_ASSIGN(NotificationDisplayServiceTester);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_TESTER_H_
