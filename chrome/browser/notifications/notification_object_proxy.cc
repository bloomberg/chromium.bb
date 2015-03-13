// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_object_proxy.h"

#include "base/guid.h"
#include "content/public/browser/desktop_notification_delegate.h"

NotificationObjectProxy::NotificationObjectProxy(
    scoped_ptr<content::DesktopNotificationDelegate> delegate)
    : delegate_(delegate.Pass()),
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

std::string NotificationObjectProxy::id() const {
  return id_;
}
