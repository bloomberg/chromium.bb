// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/non_persistent_notification_handler.h"

#include "base/strings/nullable_string16.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "content/public/browser/notification_event_dispatcher.h"

NonPersistentNotificationHandler::NonPersistentNotificationHandler() = default;
NonPersistentNotificationHandler::~NonPersistentNotificationHandler() = default;

void NonPersistentNotificationHandler::OnShow(
    Profile* profile,
    const std::string& notification_id) {
  content::NotificationEventDispatcher::GetInstance()
      ->DispatchNonPersistentShowEvent(notification_id);
}

void NonPersistentNotificationHandler::OnClose(
    Profile* profile,
    const std::string& origin,
    const std::string& notification_id,
    bool by_user) {
  content::NotificationEventDispatcher::GetInstance()
      ->DispatchNonPersistentCloseEvent(notification_id);
}

void NonPersistentNotificationHandler::OnClick(
    Profile* profile,
    const std::string& origin,
    const std::string& notification_id,
    int action_index,
    const base::NullableString16& reply) {
  DCHECK(reply.is_null());

  // Non persistent notifications don't allow buttons.
  // https://notifications.spec.whatwg.org/#create-a-notification
  DCHECK_EQ(-1, action_index);

  content::NotificationEventDispatcher::GetInstance()
      ->DispatchNonPersistentClickEvent(notification_id);
}

void NonPersistentNotificationHandler::OpenSettings(Profile* profile) {
  NotificationCommon::OpenNotificationSettings(profile);
}
