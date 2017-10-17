// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_LINUX_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_LINUX_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"

class NotificationPlatformBridgeLinuxImpl;

namespace dbus {
class Bus;
}

class NotificationPlatformBridgeLinux : public NotificationPlatformBridge {
 public:
  NotificationPlatformBridgeLinux();

  ~NotificationPlatformBridgeLinux() override;

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

 private:
  friend class NotificationPlatformBridgeLinuxTest;

  // Constructor only used in unit testing.
  explicit NotificationPlatformBridgeLinux(scoped_refptr<dbus::Bus> bus);

  void CleanUp();

  scoped_refptr<NotificationPlatformBridgeLinuxImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(NotificationPlatformBridgeLinux);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_LINUX_H_
