// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/persistent_notification_delegate.h"

#include "base/bind.h"
#include "base/guid.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "content/public/common/persistent_notification_status.h"

PersistentNotificationDelegate::PersistentNotificationDelegate(
    content::BrowserContext* browser_context,
    int64_t persistent_notification_id,
    const GURL& origin,
    int notification_settings_index)
    : browser_context_(browser_context),
      persistent_notification_id_(persistent_notification_id),
      origin_(origin),
      id_(base::GenerateGUID()),
      notification_settings_index_(notification_settings_index) {}

PersistentNotificationDelegate::~PersistentNotificationDelegate() {}

void PersistentNotificationDelegate::Display() {}

void PersistentNotificationDelegate::Close(bool by_user) {
  PlatformNotificationServiceImpl::GetInstance()->OnPersistentNotificationClose(
      browser_context_,
      persistent_notification_id_,
      origin_,
      by_user);
}

void PersistentNotificationDelegate::Click() {
  PlatformNotificationServiceImpl::GetInstance()->OnPersistentNotificationClick(
      browser_context_,
      persistent_notification_id_,
      origin_,
      -1 /* action_index */);
}

void PersistentNotificationDelegate::ButtonClick(int button_index) {
  DCHECK_GE(button_index, 0);
  if (button_index == notification_settings_index_) {
    PlatformNotificationServiceImpl::GetInstance()->OpenNotificationSettings(
        browser_context_);
    return;
  }

  PlatformNotificationServiceImpl::GetInstance()->OnPersistentNotificationClick(
      browser_context_,
      persistent_notification_id_,
      origin_,
      button_index);
}

void PersistentNotificationDelegate::SettingsClick() {
  PlatformNotificationServiceImpl::GetInstance()->OpenNotificationSettings(
      browser_context_);
  return;
}

bool PersistentNotificationDelegate::ShouldDisplaySettingsButton() {
  return true;
}

std::string PersistentNotificationDelegate::id() const {
  return id_;
}
