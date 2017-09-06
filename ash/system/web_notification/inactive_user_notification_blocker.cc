// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/inactive_user_notification_blocker.h"

#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/system_notifier.h"
#include "components/signin/core/account_id/account_id.h"
#include "ui/message_center/message_center.h"

namespace ash {

InactiveUserNotificationBlocker::InactiveUserNotificationBlocker(
    message_center::MessageCenter* message_center)
    : NotificationBlocker(message_center), scoped_observer_(this) {
  auto* session = Shell::Get()->session_controller()->GetUserSession(0);
  if (session)
    active_account_id_ = session->user_info->account_id;
}

InactiveUserNotificationBlocker::~InactiveUserNotificationBlocker() = default;

bool InactiveUserNotificationBlocker::ShouldShowNotification(
    const message_center::Notification& notification) const {
  if (!Shell::Get()->shell_delegate()->IsMultiProfilesEnabled())
    return true;

  if (system_notifier::IsAshSystemNotifier(notification.notifier_id()))
    return true;

  return AccountId::FromUserEmail(notification.notifier_id().profile_id) ==
         active_account_id_;
}

bool InactiveUserNotificationBlocker::ShouldShowNotificationAsPopup(
    const message_center::Notification& notification) const {
  return ShouldShowNotification(notification);
}

void InactiveUserNotificationBlocker::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  if (active_account_id_ == account_id)
    return;

  quiet_modes_[active_account_id_] = message_center()->IsQuietMode();
  active_account_id_ = account_id;
  std::map<AccountId, bool>::const_iterator iter =
      quiet_modes_.find(active_account_id_);
  if (iter != quiet_modes_.end() &&
      iter->second != message_center()->IsQuietMode()) {
    message_center()->SetQuietMode(iter->second);
  }
  NotifyBlockingStateChanged();
}

}  // namespace ash
