// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/stub_notification_display_service.h"

#include <algorithm>

#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile.h"

// static
std::unique_ptr<KeyedService> StubNotificationDisplayService::FactoryForTests(
    content::BrowserContext* context) {
  return std::make_unique<StubNotificationDisplayService>(
      Profile::FromBrowserContext(context));
}

StubNotificationDisplayService::StubNotificationDisplayService(Profile* profile)
    : NotificationDisplayService(profile), profile_(profile) {}

StubNotificationDisplayService::~StubNotificationDisplayService() = default;

void StubNotificationDisplayService::SetNotificationAddedClosure(
    base::RepeatingClosure closure) {
  notification_added_closure_ = std::move(closure);
}

std::vector<message_center::Notification>
StubNotificationDisplayService::GetDisplayedNotificationsForType(
    NotificationCommon::Type type) const {
  std::vector<message_center::Notification> notifications;
  for (const auto& data : notifications_) {
    if (data.type != type)
      continue;

    notifications.push_back(data.notification);
  }

  return notifications;
}

const NotificationCommon::Metadata*
StubNotificationDisplayService::GetMetadataForNotification(
    const message_center::Notification& notification) {
  auto iter = std::find_if(notifications_.begin(), notifications_.end(),
                           [notification](const NotificationData& data) {
                             return data.notification.id() == notification.id();
                           });

  if (iter == notifications_.end())
    return nullptr;

  return iter->metadata.get();
}

void StubNotificationDisplayService::RemoveNotification(
    NotificationCommon::Type notification_type,
    const std::string& notification_id,
    bool by_user,
    bool silent) {
  auto iter = std::find_if(
      notifications_.begin(), notifications_.end(),
      [notification_type, notification_id](const NotificationData& data) {
        return data.type == notification_type &&
               data.notification.id() == notification_id;
      });

  if (iter == notifications_.end())
    return;

  if (!silent) {
    NotificationHandler* handler = GetNotificationHandler(notification_type);
    DCHECK(handler);

    handler->OnClose(profile_, iter->notification.origin_url().spec(),
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
    if (iter->type == notification_type) {
      handler->OnClose(profile_, iter->notification.origin_url().spec(),
                       iter->notification.id(), by_user);
      iter = notifications_.erase(iter);
    } else {
      iter++;
    }
  }
}

void StubNotificationDisplayService::Display(
    NotificationCommon::Type notification_type,
    const std::string& notification_id,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  // This mimics notification replacement behaviour; the Close() method on a
  // notification's delegate is not meant to be invoked in this situation.
  Close(notification_type, notification_id);

  NotificationHandler* handler = GetNotificationHandler(notification_type);
  DCHECK(handler);

  handler->OnShow(profile_, notification_id);
  if (notification_added_closure_)
    notification_added_closure_.Run();

  notifications_.emplace_back(notification_type, notification,
                              std::move(metadata));
}

void StubNotificationDisplayService::Close(
    NotificationCommon::Type notification_type,
    const std::string& notification_id) {
  notifications_.erase(
      std::remove_if(
          notifications_.begin(), notifications_.end(),
          [notification_type, notification_id](const NotificationData& data) {
            return data.type == notification_type &&
                   data.notification.id() == notification_id;
          }),
      notifications_.end());
}

void StubNotificationDisplayService::GetDisplayed(
    const DisplayedNotificationsCallback& callback) {
  std::unique_ptr<std::set<std::string>> notifications =
      std::make_unique<std::set<std::string>>();

  for (const auto& notification_data : notifications_)
    notifications->insert(notification_data.notification.id());

  callback.Run(std::move(notifications), true /* supports_synchronization */);
}

StubNotificationDisplayService::NotificationData::NotificationData(
    NotificationCommon::Type type,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata)
    : type(type), notification(notification), metadata(std::move(metadata)) {}

StubNotificationDisplayService::NotificationData::NotificationData(
    NotificationData&& other)
    : type(other.type),
      notification(other.notification),
      metadata(std::move(other.metadata)) {}

StubNotificationDisplayService::NotificationData::~NotificationData() {}

StubNotificationDisplayService::NotificationData&
StubNotificationDisplayService::NotificationData::operator=(
    NotificationData&& other) {
  type = other.type;
  notification = std::move(other.notification);
  metadata = std::move(other.metadata);
  return *this;
}
