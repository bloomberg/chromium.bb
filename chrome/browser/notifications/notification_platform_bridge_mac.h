// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MAC_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MAC_H_

#include <set>
#include <string>

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"

class Notification;
@class NotificationCenterDelegate;
@class NSUserNotificationCenter;
class PrefService;

// This class is an implementation of NotificationPlatformBridge that will
// send platform notifications to the the MacOSX notification center.
class NotificationPlatformBridgeMac : public NotificationPlatformBridge {
 public:
  explicit NotificationPlatformBridgeMac(
      NSUserNotificationCenter* notification_center);
  ~NotificationPlatformBridgeMac() override;

  // NotificationPlatformBridge implementation.
  void Display(const std::string& notification_id,
               const std::string& profile_id,
               bool incognito,
               const Notification& notification) override;
  void Close(const std::string& profile_id,
             const std::string& notification_id) override;
  bool GetDisplayed(const std::string& profile_id,
                    bool incognito,
                    std::set<std::string>* notifications) const override;
  bool SupportsNotificationCenter() const override;

 private:
  // Cocoa class that receives callbacks from the NSUserNotificationCenter.
  base::scoped_nsobject<NotificationCenterDelegate> delegate_;

  // The notification center to use, this can be overriden in tests
  NSUserNotificationCenter* notification_center_;

  DISALLOW_COPY_AND_ASSIGN(NotificationPlatformBridgeMac);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MAC_H_
