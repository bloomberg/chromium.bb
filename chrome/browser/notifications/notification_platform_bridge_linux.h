// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_LINUX_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_LINUX_H_

#include <gio/gio.h>

#include "base/macros.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"

class NotificationPlatformBridgeLinux : public NotificationPlatformBridge {
 public:
  explicit NotificationPlatformBridgeLinux(GDBusProxy* notification_proxy);

  ~NotificationPlatformBridgeLinux() override;

  // NotificationPlatformBridge:
  void Display(NotificationCommon::Type notification_type,
               const std::string& notification_id,
               const std::string& profile_id,
               bool is_incognito,
               const Notification& notification) override;
  void Close(const std::string& profile_id,
             const std::string& notification_id) override;
  void GetDisplayed(
      const std::string& profile_id,
      bool incognito,
      const DisplayedNotificationsCallback& callback) const override;

 private:
  GDBusProxy* const notification_proxy_;

  DISALLOW_COPY_AND_ASSIGN(NotificationPlatformBridgeLinux);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_LINUX_H_
