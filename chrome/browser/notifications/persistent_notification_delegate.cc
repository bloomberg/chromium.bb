// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/persistent_notification_delegate.h"

#include "base/strings/nullable_string16.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "url/gurl.h"

PersistentNotificationDelegate::PersistentNotificationDelegate(
    content::BrowserContext* browser_context,
    const std::string& notification_id,
    const GURL& origin,
    int notification_settings_index)
    : WebNotificationDelegate(browser_context, notification_id, origin),
      notification_settings_index_(notification_settings_index) {}

PersistentNotificationDelegate::~PersistentNotificationDelegate() {}

void PersistentNotificationDelegate::Display() {}

void PersistentNotificationDelegate::Close(bool by_user) {
  PlatformNotificationServiceImpl::GetInstance()->OnPersistentNotificationClose(
      browser_context(), id(), origin(), by_user);
}

void PersistentNotificationDelegate::Click() {
  PlatformNotificationServiceImpl::GetInstance()->OnPersistentNotificationClick(
      browser_context(), id(), origin(), -1 /* action_index */,
      base::NullableString16() /* reply */);
}

void PersistentNotificationDelegate::ButtonClick(int button_index) {
  DCHECK_GE(button_index, 0);
  if (button_index == notification_settings_index_) {
    NotificationCommon::OpenNotificationSettings(browser_context());
    return;
  }

  PlatformNotificationServiceImpl::GetInstance()->OnPersistentNotificationClick(
      browser_context(), id(), origin(), button_index,
      base::NullableString16() /* reply */);
}

void PersistentNotificationDelegate::ButtonClickWithReply(
    int button_index,
    const base::string16& reply) {
  DCHECK_GE(button_index, 0);
  DCHECK_NE(button_index, notification_settings_index_);
  PlatformNotificationServiceImpl::GetInstance()->OnPersistentNotificationClick(
      browser_context(), id(), origin(), button_index,
      base::NullableString16(reply, false /* is_null */));
}
