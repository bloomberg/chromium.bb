// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/stub_notification_display_service.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile.h"

// static
std::unique_ptr<KeyedService> StubNotificationDisplayService::FactoryForTests(
    content::BrowserContext* context) {
  return base::MakeUnique<StubNotificationDisplayService>(
      Profile::FromBrowserContext(context));
}

StubNotificationDisplayService::StubNotificationDisplayService(Profile* profile)
    : NotificationDisplayService(profile), profile_(profile) {}

StubNotificationDisplayService::~StubNotificationDisplayService() = default;

void StubNotificationDisplayService::SetNotificationAddedClosure(
    base::RepeatingClosure closure) {
  notification_added_closure_ = std::move(closure);
}

std::vector<Notification>
StubNotificationDisplayService::GetDisplayedNotificationsForType(
    NotificationCommon::Type type) const {
  std::vector<Notification> notifications;
  for (const auto& pair : notifications_) {
    if (pair.first != type)
      continue;

    notifications.push_back(pair.second);
  }

  return notifications;
}

void StubNotificationDisplayService::RemoveNotification(
    NotificationCommon::Type notification_type,
    const std::string& notification_id,
    bool by_user,
    bool silent) {
  auto iter = std::find_if(
      notifications_.begin(), notifications_.end(),
      [notification_type, notification_id](const NotificationData& data) {
        return data.first == notification_type &&
               data.second.delegate_id() == notification_id;
      });

  if (iter == notifications_.end())
    return;

  if (!silent) {
    NotificationHandler* handler = GetNotificationHandler(notification_type);
    DCHECK(handler);

    handler->OnClose(profile_, iter->second.origin_url().spec(),
                     notification_id, by_user);
  }

  notifications_.erase(iter);
}

void StubNotificationDisplayService::RemoveAllNotifications(
    NotificationCommon::Type notification_type,
    bool by_user) {
  NotificationHandler* handler = GetNotificationHandler(notification_type);
  DCHECK(handler);
  for (auto iter = notifications_.begin(); iter != notifications_.end();) {
    if (iter->first == notification_type) {
      handler->OnClose(profile_, iter->second.origin_url().spec(),
                       iter->second.id(), by_user);
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

  NotificationHandler* handler = GetNotificationHandler(notification_type);
  DCHECK(handler);

  handler->OnShow(profile_, notification_id);
  if (notification_added_closure_)
    notification_added_closure_.Run();

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
