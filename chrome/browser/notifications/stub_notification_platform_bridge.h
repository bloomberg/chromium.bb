// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_STUB_NOTIFICATION_PLATFORM_BRIDGE_H_
#define CHROME_BROWSER_NOTIFICATIONS_STUB_NOTIFICATION_PLATFORM_BRIDGE_H_

#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"

// Implementation of NotificationPlatformBridge used for tests.
class StubNotificationPlatformBridge : public NotificationPlatformBridge {
 public:
  StubNotificationPlatformBridge();
  ~StubNotificationPlatformBridge() override;

  // Returns the notification being displayed at position |index|.
  Notification GetNotificationAt(std::string profile_id, size_t index);

  // NotificationPlatformBridge implementation.
  void Display(NotificationCommon::Type notification_type,
               const std::string& notification_id,
               const std::string& profile_id,
               bool incognito,
               const Notification& notification) override;
  void Close(const std::string& profile_id,
             const std::string& notification_id) override;
  bool GetDisplayed(const std::string& profile_id,
                    bool incognito,
                    std::set<std::string>* notifications) const override;

 private:
  // Map of profile Ids to list of notifications shown for said profile.
  std::unordered_map<std::string, std::vector<Notification>> notifications_;

  DISALLOW_COPY_AND_ASSIGN(StubNotificationPlatformBridge);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_STUB_NOTIFICATION_PLATFORM_BRIDGE_H_
