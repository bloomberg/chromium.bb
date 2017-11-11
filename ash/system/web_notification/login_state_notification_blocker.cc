// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/login_state_notification_blocker.h"

#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/system/system_notifier.h"
#include "ui/message_center/message_center.h"

using session_manager::SessionState;

namespace ash {

LoginStateNotificationBlocker::LoginStateNotificationBlocker(
    message_center::MessageCenter* message_center)
    : NotificationBlocker(message_center) {
  Shell::Get()->session_controller()->AddObserver(this);
}

LoginStateNotificationBlocker::~LoginStateNotificationBlocker() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

bool LoginStateNotificationBlocker::ShouldShowNotification(
    const message_center::Notification& notification) const {
  return !Shell::Get()->session_controller()->IsScreenLocked();
}

bool LoginStateNotificationBlocker::ShouldShowNotificationAsPopup(
    const message_center::Notification& notification) const {
  if (ash::system_notifier::ShouldAlwaysShowPopups(notification.notifier_id()))
    return true;

  return Shell::Get()->session_controller()->GetSessionState() ==
         SessionState::ACTIVE;
}

void LoginStateNotificationBlocker::OnSessionStateChanged(SessionState state) {
  NotifyBlockingStateChanged();
}

}  // namespace ash
