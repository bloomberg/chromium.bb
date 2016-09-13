// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/stub_notification_platform_bridge.h"

StubNotificationPlatformBridge::StubNotificationPlatformBridge()
    : NotificationPlatformBridge() {}

StubNotificationPlatformBridge::~StubNotificationPlatformBridge() {}

Notification StubNotificationPlatformBridge::GetNotificationAt(
    std::string profile_id,
    size_t index) {
  DCHECK(notifications_.find(profile_id) != notifications_.end());
  DCHECK_GT(notifications_[profile_id].size(), index);

  return notifications_[profile_id][index];
}

void StubNotificationPlatformBridge::Display(
    NotificationCommon::Type notification_type,
    const std::string& notification_id,
    const std::string& profile_id,
    bool incognito,
    const Notification& notification) {
  notifications_[profile_id].push_back(notification);
}

void StubNotificationPlatformBridge::Close(const std::string& profile_id,
                                           const std::string& notification_id) {
  if (notifications_.find(profile_id) == notifications_.end())
    return;
  std::vector<Notification> profile_notifications = notifications_[profile_id];
  for (auto iter = profile_notifications.begin();
       iter != profile_notifications.end(); ++iter) {
    if (iter->id() == notification_id) {
      profile_notifications.erase(iter);
      if (profile_notifications.empty())
        notifications_.erase(profile_id);
      break;
    }
  }
}

bool StubNotificationPlatformBridge::GetDisplayed(
    const std::string& profile_id,
    bool incognito,
    std::set<std::string>* notifications) const {
  if (notifications_.find(profile_id) == notifications_.end())
    return true;

  const std::vector<Notification>& profile_notifications =
      notifications_.at(profile_id);
  for (auto notification : profile_notifications)
    notifications->insert(notification.id());
  return true;
}
