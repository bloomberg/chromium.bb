// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/stub_notification_display_service.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"

// static
std::unique_ptr<KeyedService> StubNotificationDisplayService::FactoryForTests(
    content::BrowserContext* context) {
  return base::MakeUnique<StubNotificationDisplayService>(
      Profile::FromBrowserContext(context));
}

StubNotificationDisplayService::StubNotificationDisplayService(Profile* profile)
    : NotificationDisplayService(profile) {}

StubNotificationDisplayService::~StubNotificationDisplayService() = default;

void StubNotificationDisplayService::RemoveNotification(
    NotificationCommon::Type notification_type,
    const std::string& notification_id,
    bool by_user) {
  auto iter = std::find_if(
      notifications_.begin(), notifications_.end(),
      [notification_type, notification_id](const NotificationData& data) {
        return data.first == notification_type &&
               data.second.delegate_id() == notification_id;
      });

  if (iter == notifications_.end())
    return;

  // TODO(peter): Invoke the handlers when that has been generalized.
  iter->second.delegate()->Close(by_user);

  notifications_.erase(iter);
}

void StubNotificationDisplayService::RemoveAllNotifications(
    NotificationCommon::Type notification_type,
    bool by_user) {
  for (auto iter = notifications_.begin(); iter != notifications_.end();) {
    if (iter->first == notification_type) {
      // TODO(peter): Invoke the handlers when that has been generalized.
      iter->second.delegate()->Close(by_user);

      iter = notifications_.erase(iter);
    } else {
      iter++;
    }
  }
}

void StubNotificationDisplayService::Display(
    NotificationCommon::Type notification_type,
    const std::string& notification_id,
    const Notification& notification) {
  // This mimics notification replacement behaviour; the Close() method on a
  // notification's delegate is not meant to be invoked in this situation.
  Close(notification_type, notification_id);

  notifications_.emplace_back(notification_type, notification);
}

void StubNotificationDisplayService::Close(
    NotificationCommon::Type notification_type,
    const std::string& notification_id) {
  notifications_.erase(
      std::remove_if(
          notifications_.begin(), notifications_.end(),
          [notification_type, notification_id](const NotificationData& data) {
            return data.first == notification_type &&
                   data.second.delegate_id() == notification_id;
          }),
      notifications_.end());
}

void StubNotificationDisplayService::GetDisplayed(
    const DisplayedNotificationsCallback& callback) {
  std::unique_ptr<std::set<std::string>> notifications =
      base::MakeUnique<std::set<std::string>>();

  for (const auto& notification_data : notifications_)
    notifications->insert(notification_data.second.delegate_id());

  callback.Run(std::move(notifications), true /* supports_synchronization */);
}
