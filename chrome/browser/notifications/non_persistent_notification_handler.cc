// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/non_persistent_notification_handler.h"

#include "base/strings/nullable_string16.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"

NonPersistentNotificationHandler::NonPersistentNotificationHandler() {}
NonPersistentNotificationHandler::~NonPersistentNotificationHandler() {}

void NonPersistentNotificationHandler::OnClose(
    Profile* profile,
    const std::string& origin,
    const std::string& notification_id,
    bool by_user) {
  if (notifications_.find(notification_id) != notifications_.end()) {
    notifications_[notification_id]->Close(by_user);
    notifications_.erase(notification_id);
  }
}

void NonPersistentNotificationHandler::OnClick(
    Profile* profile,
    const std::string& origin,
    const std::string& notification_id,
    int action_index,
    const base::NullableString16& reply) {
  // Buttons and replies not supported for non persistent notifications.
  DCHECK_EQ(action_index, -1);
  DCHECK(reply.is_null());

  if (notifications_.find(notification_id) != notifications_.end()) {
    notifications_[notification_id]->Click();
  }
}

void NonPersistentNotificationHandler::OpenSettings(Profile* profile) {
  NotificationCommon::OpenNotificationSettings(profile);
}

void NonPersistentNotificationHandler::RegisterNotification(
    const std::string& notification_id,
    NotificationDelegate* delegate) {
  DCHECK_EQ(notifications_.count(notification_id), 0u);
  notifications_[notification_id] =
      scoped_refptr<NotificationDelegate>(delegate);
}
