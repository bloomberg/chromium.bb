// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification.h"

Notification::Notification(
    message_center::NotificationType type,
    const base::string16& title,
    const base::string16& body,
    const gfx::Image& icon,
    const message_center::NotifierId& notifier_id,
    const base::string16& display_source,
    const GURL& origin_url,
    const std::string& tag,
    const message_center::RichNotificationData& rich_notification_data,
    NotificationDelegate* delegate)
    : message_center::Notification(type,
                                   delegate->id(),
                                   title,
                                   body,
                                   icon,
                                   display_source,
                                   origin_url,
                                   notifier_id,
                                   rich_notification_data,
                                   delegate),
      tag_(tag),
      delegate_(delegate) {}

Notification::Notification(const std::string& id,
                           const Notification& notification)
    : message_center::Notification(id, notification),
      tag_(notification.tag()),
      service_worker_scope_(notification.service_worker_scope()),
      delegate_(notification.delegate()) {
}

Notification::Notification(const Notification& notification)
    : message_center::Notification(notification),
      tag_(notification.tag()),
      service_worker_scope_(notification.service_worker_scope()),
      delegate_(notification.delegate()) {}

Notification::~Notification() {}

Notification& Notification::operator=(const Notification& notification) {
  message_center::Notification::operator=(notification);
  tag_ = notification.tag();
  service_worker_scope_ = notification.service_worker_scope();
  delegate_ = notification.delegate();
  return *this;
}
