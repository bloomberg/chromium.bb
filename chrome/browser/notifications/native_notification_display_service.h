// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NATIVE_NOTIFICATION_DISPLAY_SERVICE_H_
#define CHROME_BROWSER_NOTIFICATIONS_NATIVE_NOTIFICATION_DISPLAY_SERVICE_H_

#include <set>
#include <string>
#include "base/macros.h"
#include "chrome/browser/notifications/notification_display_service.h"

class Notification;
class NotificationPlatformBridge;
class Profile;

// A class to display and interact with notifications in native notification
// centers on platforms that support it.
class NativeNotificationDisplayService : public NotificationDisplayService {
 public:
  NativeNotificationDisplayService(
      Profile* profile,
      NotificationPlatformBridge* notification_bridge);
  ~NativeNotificationDisplayService() override;

  // NotificationDisplayService implementation.
  void Display(const std::string& notification_id,
               const Notification& notification) override;
  void Close(const std::string& notification_id) override;
  bool GetDisplayed(std::set<std::string>* notifications) const override;
  bool SupportsNotificationCenter() const override;

 private:
  Profile* profile_;
  NotificationPlatformBridge* notification_bridge_;

  DISALLOW_COPY_AND_ASSIGN(NativeNotificationDisplayService);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NATIVE_NOTIFICATION_DISPLAY_SERVICE_H_
