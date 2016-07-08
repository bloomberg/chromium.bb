// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/persistent_notification_handler.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/profiles/profile.h"

PersistentNotificationHandler::PersistentNotificationHandler() {}
PersistentNotificationHandler::~PersistentNotificationHandler() {}

void PersistentNotificationHandler::OnClose(Profile* profile,
                                            const std::string& origin,
                                            const std::string& notification_id,
                                            bool by_user) {
  // No need to propage back Close events from JS.
  if (!by_user)
    return;

  int64_t persistent_notification_id;
  GURL notification_origin(origin);
  DCHECK(notification_origin.is_valid());
  if (!base::StringToInt64(notification_id, &persistent_notification_id)) {
    LOG(ERROR) << "Unable to convert notification ID: " << notification_id
               << " to integer.";
    return;
  }
  PlatformNotificationServiceImpl::GetInstance()->OnPersistentNotificationClose(
      profile, persistent_notification_id, notification_origin, by_user);
}

void PersistentNotificationHandler::OnClick(Profile* profile,
                                            const std::string& origin,
                                            const std::string& notification_id,
                                            int action_index) {
  int64_t persistent_notification_id;
  if (!base::StringToInt64(notification_id, &persistent_notification_id)) {
    LOG(ERROR) << "Unable to convert notification ID: " << notification_id
               << " to integer.";
    return;
  }
  GURL notification_origin(origin);
  DCHECK(notification_origin.is_valid());
  PlatformNotificationServiceImpl::GetInstance()->OnPersistentNotificationClick(
      profile, persistent_notification_id, notification_origin, action_index);
}

void PersistentNotificationHandler::OpenSettings(Profile* profile) {
  NotificationCommon::OpenNotificationSettings(profile);
}

void PersistentNotificationHandler::RegisterNotification(
    const std::string& notification_id,
    NotificationDelegate* delegate) {
  // Nothing to do here since there is no state kept.
}
