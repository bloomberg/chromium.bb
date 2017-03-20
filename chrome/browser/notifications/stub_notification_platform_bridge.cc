// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/stub_notification_platform_bridge.h"

#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_thread.h"

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

size_t StubNotificationPlatformBridge::GetNotificationCount() {
  int count = 0;
  for (const auto& pair : notifications_) {
    count += pair.second.size();
  }
  return count;
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

void StubNotificationPlatformBridge::GetDisplayed(
    const std::string& profile_id,
    bool incognito,
    const DisplayedNotificationsCallback& callback) const {
  auto displayed_notifications = base::MakeUnique<std::set<std::string>>();

  if (notifications_.find(profile_id) != notifications_.end()) {
    const std::vector<Notification>& profile_notifications =
        notifications_.at(profile_id);
    for (auto notification : profile_notifications)
      displayed_notifications->insert(notification.id());
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(callback, base::Passed(&displayed_notifications),
                 true /* supports_synchronization */));
}
