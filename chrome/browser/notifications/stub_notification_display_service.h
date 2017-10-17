// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_STUB_NOTIFICATION_DISPLAY_SERVICE_H_
#define CHROME_BROWSER_NOTIFICATIONS_STUB_NOTIFICATION_DISPLAY_SERVICE_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "ui/message_center/notification.h"

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

  // Sets |closure| to be invoked when any notification has been added.
  void SetNotificationAddedClosure(base::RepeatingClosure closure);

  // Returns a vector of the displayed Notification objects.
  std::vector<message_center::Notification> GetDisplayedNotificationsForType(
      NotificationCommon::Type type) const;

  const NotificationCommon::Metadata* GetMetadataForNotification(
      const message_center::Notification& notification);

  // Simulates the notification identified by |notification_id| being closed due
  // to external events, such as the user dismissing it when |by_user| is set.
  // When |silent| is set, the notification handlers won't be informed of the
  // change to immitate behaviour of operating systems that don't inform apps
  // about removed notifications.
  void RemoveNotification(NotificationCommon::Type notification_type,
                          const std::string& notification_id,
                          bool by_user,
                          bool silent);

  // Removes all notifications shown by this display service.
  void RemoveAllNotifications(NotificationCommon::Type notification_type,
                              bool by_user);

  // NotificationDisplayService implementation:
  void Display(NotificationCommon::Type notification_type,
               const std::string& notification_id,
               const message_center::Notification& notification,
               std::unique_ptr<NotificationCommon::Metadata> metadata) override;
  void Close(NotificationCommon::Type notification_type,
             const std::string& notification_id) override;
  void GetDisplayed(const DisplayedNotificationsCallback& callback) override;

 private:
  // Data to store for a notification that's being shown through this service.
  struct NotificationData {
    NotificationData(NotificationCommon::Type type,
                     const message_center::Notification& notification,
                     std::unique_ptr<NotificationCommon::Metadata> metadata);
    NotificationData(NotificationData&& other);
    ~NotificationData();

    NotificationData& operator=(NotificationData&& other);

    NotificationCommon::Type type;
    message_center::Notification notification;
    std::unique_ptr<NotificationCommon::Metadata> metadata;
  };

  base::RepeatingClosure notification_added_closure_;
  std::vector<NotificationData> notifications_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(StubNotificationDisplayService);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_STUB_NOTIFICATION_DISPLAY_SERVICE_H_
