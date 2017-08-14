// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_STUB_NOTIFICATION_DISPLAY_SERVICE_H_
#define CHROME_BROWSER_NOTIFICATIONS_STUB_NOTIFICATION_DISPLAY_SERVICE_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"

class Profile;

// Implementation of the NotificationDisplayService interface that can be used
// for testing purposes. Supports additional methods enabling instrumenting the
// faked underlying notification system.
class StubNotificationDisplayService : public NotificationDisplayService {
 public:
  // Factory function to be used with NotificationDisplayServiceFactory's
  // SetTestingFactory method, overriding the default display service.
  static std::unique_ptr<KeyedService> FactoryForTests(
      content::BrowserContext* browser_context);

  explicit StubNotificationDisplayService(Profile* profile);
  ~StubNotificationDisplayService() override;

  // Returns a vector of the displayed Notification objects.
  std::vector<Notification> GetDisplayedNotificationsForType(
      NotificationCommon::Type type) const;

  // Removes the notification identified by |notification_id|. This can
  // optionally be |silent|, which means the delegate events won't be invoked
  // to imitate behaviour on operating systems that don't support such events.
  void RemoveNotification(NotificationCommon::Type notification_type,
                          const std::string& notification_id,
                          bool by_user,
                          bool silent = false);

  // Removes all notifications shown by this display service.
  void RemoveAllNotifications(NotificationCommon::Type notification_type,
                              bool by_user);

  // NotificationDisplayService implementation:
  void Display(NotificationCommon::Type notification_type,
               const std::string& notification_id,
               const Notification& notification) override;
  void Close(NotificationCommon::Type notification_type,
             const std::string& notification_id) override;
  void GetDisplayed(const DisplayedNotificationsCallback& callback) override;

 private:
  // Data to store for a notification that's being shown through this service.
  using NotificationData = std::pair<NotificationCommon::Type, Notification>;

  std::vector<NotificationData> notifications_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(StubNotificationDisplayService);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_STUB_NOTIFICATION_DISPLAY_SERVICE_H_
