// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification.h"

#include "base/strings/string_util.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "ui/base/webui/web_ui_util.h"

Notification::Notification(const GURL& origin_url,
                           const GURL& icon_url,
                           const base::string16& title,
                           const base::string16& body,
                           blink::WebTextDirection dir,
                           const base::string16& display_source,
                           const base::string16& replace_id,
                           NotificationDelegate* delegate)
    : message_center::Notification(message_center::NOTIFICATION_TYPE_SIMPLE,
                                   delegate->id(),
                                   title,
                                   body,
                                   gfx::Image(),
                                   display_source,
                                   message_center::NotifierId(origin_url),
                                   message_center::RichNotificationData(),
                                   delegate),
      origin_url_(origin_url),
      icon_url_(icon_url),
      replace_id_(replace_id),
      delegate_(delegate) {
  // "Upconvert" the string parameters to a data: URL.
  content_url_ = GURL(DesktopNotificationService::CreateDataUrl(
      icon_url, title, body, dir));
}

Notification::Notification(
    message_center::NotificationType type,
    const GURL& origin_url,
    const base::string16& title,
    const base::string16& body,
    const gfx::Image& icon,
    blink::WebTextDirection dir,
    const message_center::NotifierId& notifier_id,
    const base::string16& display_source,
    const base::string16& replace_id,
    const message_center::RichNotificationData& rich_notification_data,
    NotificationDelegate* delegate)
    : message_center::Notification(type,
                                   delegate->id(),
                                   title,
                                   body,
                                   icon,
                                   display_source,
                                   notifier_id,
                                   rich_notification_data,
                                   delegate),
      origin_url_(origin_url),
      replace_id_(replace_id),
      delegate_(delegate) {
  // It's important to leave |icon_url_| empty with rich notifications enabled,
  // to prevent "Downloading" the data url and overwriting the existing |icon|.
}

Notification::Notification(const GURL& origin_url,
                           const gfx::Image& icon,
                           const base::string16& title,
                           const base::string16& body,
                           blink::WebTextDirection dir,
                           const base::string16& display_source,
                           const base::string16& replace_id,
                           NotificationDelegate* delegate)
    : message_center::Notification(message_center::NOTIFICATION_TYPE_SIMPLE,
                                   delegate->id(),
                                   title,
                                   body,
                                   icon,
                                   display_source,
                                   message_center::NotifierId(origin_url),
                                   message_center::RichNotificationData(),
                                   delegate),
      origin_url_(origin_url),
      replace_id_(replace_id),
      delegate_(delegate) {}

Notification::Notification(const Notification& notification)
    : message_center::Notification(notification),
      origin_url_(notification.origin_url()),
      icon_url_(notification.icon_url()),
      content_url_(notification.content_url()),
      button_one_icon_url_(notification.button_one_icon_url()),
      button_two_icon_url_(notification.button_two_icon_url()),
      image_url_(notification.image_url()),
      replace_id_(notification.replace_id()),
      delegate_(notification.delegate()) {}

Notification::~Notification() {}

Notification& Notification::operator=(const Notification& notification) {
  message_center::Notification::operator=(notification);
  origin_url_ = notification.origin_url();
  icon_url_ = notification.icon_url();
  content_url_ = notification.content_url();
  button_one_icon_url_ = notification.button_one_icon_url();
  button_two_icon_url_ = notification.button_two_icon_url();
  image_url_ = notification.image_url();
  replace_id_ = notification.replace_id();
  delegate_ = notification.delegate();
  return *this;
}
