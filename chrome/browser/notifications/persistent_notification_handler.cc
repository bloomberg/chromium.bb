// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/persistent_notification_handler.h"

#include "base/logging.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/profiles/profile.h"

PersistentNotificationHandler::PersistentNotificationHandler() {}
PersistentNotificationHandler::~PersistentNotificationHandler() {}

void PersistentNotificationHandler::OnShow(Profile* profile,
                                           const std::string& notification_id) {
}

void PersistentNotificationHandler::OnClose(Profile* profile,
                                            const GURL& origin,
                                            const std::string& notification_id,
                                            bool by_user) {
  if (!by_user)
    return;  // no need to propagate back programmatic close events

  DCHECK(origin.is_valid());

  PlatformNotificationServiceImpl::GetInstance()->OnPersistentNotificationClose(
      profile, notification_id, origin, by_user);
}

void PersistentNotificationHandler::OnClick(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    const base::Optional<int>& action_index,
    const base::Optional<base::string16>& reply) {
  DCHECK(origin.is_valid());

  PlatformNotificationServiceImpl::GetInstance()->OnPersistentNotificationClick(
      profile, notification_id, origin, action_index, reply);
}

void PersistentNotificationHandler::OpenSettings(Profile* profile) {
  NotificationCommon::OpenNotificationSettings(profile);
}
