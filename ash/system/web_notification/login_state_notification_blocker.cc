// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/login_state_notification_blocker.h"

#include "ash/system/system_notifier.h"
#include "components/session_manager/core/session_manager.h"
#include "ui/message_center/message_center.h"

using session_manager::SessionManager;
using session_manager::SessionState;

namespace ash {

LoginStateNotificationBlocker::LoginStateNotificationBlocker(
    message_center::MessageCenter* message_center)
    : NotificationBlocker(message_center) {
  // SessionManager may not exist in some tests.
  if (SessionManager::Get())
    SessionManager::Get()->AddObserver(this);
}

LoginStateNotificationBlocker::~LoginStateNotificationBlocker() {
  if (SessionManager::Get())
    SessionManager::Get()->RemoveObserver(this);
}

bool LoginStateNotificationBlocker::ShouldShowNotificationAsPopup(
    const message_center::Notification& notification) const {
  if (ash::system_notifier::ShouldAlwaysShowPopups(notification.notifier_id()))
    return true;

  if (SessionManager::Get())
    return SessionManager::Get()->session_state() == SessionState::ACTIVE;

  return true;
}

void LoginStateNotificationBlocker::OnSessionStateChanged() {
  NotifyBlockingStateChanged();
}

}  // namespace ash
