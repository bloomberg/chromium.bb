// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/multi_user_notification_blocker_chromeos.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/system_notifier.h"
#include "components/signin/core/account_id/account_id.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notifier_settings.h"

MultiUserNotificationBlockerChromeOS::MultiUserNotificationBlockerChromeOS(
    message_center::MessageCenter* message_center,
    const AccountId& initial_account_id)
    : NotificationBlocker(message_center),
      active_account_id_(initial_account_id) {}

MultiUserNotificationBlockerChromeOS::~MultiUserNotificationBlockerChromeOS() {
}

bool MultiUserNotificationBlockerChromeOS::ShouldShowNotification(
    const message_center::Notification& notification) const {
  if (!ash::Shell::HasInstance() ||
      !ash::Shell::Get()->shell_delegate()->IsMultiProfilesEnabled())
    return true;

  if (ash::system_notifier::IsAshSystemNotifier(notification.notifier_id()))
    return true;

  return AccountId::FromUserEmail(notification.notifier_id().profile_id) ==
         active_account_id_;
}

bool MultiUserNotificationBlockerChromeOS::ShouldShowNotificationAsPopup(
    const message_center::Notification& notification) const {
  return ShouldShowNotification(notification);
}

void MultiUserNotificationBlockerChromeOS::ActiveUserChanged(
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
