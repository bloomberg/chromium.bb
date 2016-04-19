// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_object_proxy.h"

#include <utility>

#include "base/guid.h"
#include "base/logging.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "content/public/browser/desktop_notification_delegate.h"

NotificationObjectProxy::NotificationObjectProxy(
    content::BrowserContext* browser_context,
    std::unique_ptr<content::DesktopNotificationDelegate> delegate)
    : browser_context_(browser_context),
      delegate_(std::move(delegate)),
      displayed_(false),
      id_(base::GenerateGUID()) {}

NotificationObjectProxy::~NotificationObjectProxy() {}

void NotificationObjectProxy::Display() {
  // This method is called each time the notification is shown to the user
  // but we only want to fire the event the first time.
  if (displayed_)
    return;
  displayed_ = true;

  delegate_->NotificationDisplayed();
}

void NotificationObjectProxy::Close(bool by_user) {
  delegate_->NotificationClosed();
}

void NotificationObjectProxy::Click() {
  delegate_->NotificationClick();
}

void NotificationObjectProxy::ButtonClick(int button_index) {
  // Notification buttons not are supported for non persistent notifications.
  DCHECK_EQ(button_index, 0);
  PlatformNotificationServiceImpl::GetInstance()->OpenNotificationSettings(
      browser_context_);
}

void NotificationObjectProxy::SettingsClick() {
  PlatformNotificationServiceImpl::GetInstance()->OpenNotificationSettings(
      browser_context_);
  return;
}

bool NotificationObjectProxy::ShouldDisplaySettingsButton() {
  return true;
}

std::string NotificationObjectProxy::id() const {
  return id_;
}
