// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_CHROMEOS_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_CHROMEOS_H_

#include <map>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"
#include "chrome/browser/notifications/profile_notification.h"

// The interface that a NotificationPlatformBridge uses to pass back information
// and interactions from the native notification system. TODO(estade): this
// should be hoisted into its own file, implemented by
// NativeNotificationDisplayService, and used by other platforms'
// NotificationPlatformBridge implementations. See http://crbug.com/776443
class NotificationPlatformBridgeDelegate {
 public:
  // To be called when a notification is closed. Each notification can be closed
  // at most once.
  virtual void HandleNotificationClosed(const std::string& id,
                                        bool by_user) = 0;

  // To be called when the body of a notification is clicked.
  virtual void HandleNotificationClicked(const std::string& id) = 0;

  // To be called when a button in a notification is clicked.
  virtual void HandleNotificationButtonClicked(const std::string& id,
                                               int button_index) = 0;
};

// A platform bridge that uses Ash's message center to display notifications.
// Currently under development and controlled by feature:
//   --enable-features=NativeNotifications
class NotificationPlatformBridgeChromeOs
    : public NotificationPlatformBridge,
      public NotificationPlatformBridgeDelegate {
 public:
  NotificationPlatformBridgeChromeOs();

  ~NotificationPlatformBridgeChromeOs() override;

  // NotificationPlatformBridge:
  void Display(NotificationCommon::Type notification_type,
               const std::string& notification_id,
               const std::string& profile_id,
               bool is_incognito,
               const message_center::Notification& notification,
               std::unique_ptr<NotificationCommon::Metadata> metadata) override;
  void Close(const std::string& profile_id,
             const std::string& notification_id) override;
  void GetDisplayed(
      const std::string& profile_id,
      bool incognito,
      const GetDisplayedNotificationsCallback& callback) const override;
  void SetReadyCallback(NotificationBridgeReadyCallback callback) override;

  // NotificationPlatformBridgeDelegate:
  void HandleNotificationClosed(const std::string& id, bool by_user) override;
  void HandleNotificationClicked(const std::string& id) override;
  void HandleNotificationButtonClicked(const std::string& id,
                                       int button_index) override;

 private:
  ProfileNotification* GetProfileNotification(
      const std::string& profile_notification_id);

  std::unique_ptr<NotificationPlatformBridge> impl_;

  // A container for all active notifications, where IDs are permuted to
  // uniquely identify both the notification and its source profile. The key is
  // the permuted ID.
  std::map<std::string, std::unique_ptr<ProfileNotification>>
      active_notifications_;

  DISALLOW_COPY_AND_ASSIGN(NotificationPlatformBridgeChromeOs);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_CHROMEOS_H_
