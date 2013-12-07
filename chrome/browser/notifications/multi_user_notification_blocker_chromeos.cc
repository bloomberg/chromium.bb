// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/multi_user_notification_blocker_chromeos.h"

#include "ash/shell.h"
#include "ash/system/system_notifier.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notifier_settings.h"

MultiUserNotificationBlockerChromeOS::MultiUserNotificationBlockerChromeOS(
    message_center::MessageCenter* message_center)
    : NotificationBlocker(message_center),
      observing_(false) {
  // UserManager may not be initialized in unit tests.
  if (!chromeos::UserManager::IsInitialized())
    return;

  // This class is created in the ctor of NotificationUIManager which is created
  // when a notification is created, so ash::Shell should be initialized.
  ash::Shell::GetInstance()->AddShellObserver(this);
  chromeos::UserManager::Get()->AddSessionStateObserver(this);
  observing_ = true;
}

MultiUserNotificationBlockerChromeOS::~MultiUserNotificationBlockerChromeOS() {
  if (observing_) {
    if (ash::Shell::HasInstance())
      ash::Shell::GetInstance()->RemoveShellObserver(this);
    chromeos::UserManager::Get()->RemoveSessionStateObserver(this);
  }
}

bool MultiUserNotificationBlockerChromeOS::ShouldShowNotification(
    const message_center::NotifierId& notifier_id) const {
  if (!IsActive())
    return true;

  if (ash::system_notifier::IsAshSystemNotifier(notifier_id))
    return true;

  return notifier_id.profile_id == active_user_id_;
}

bool MultiUserNotificationBlockerChromeOS::ShouldShowNotificationAsPopup(
    const message_center::NotifierId& notifier_id) const {
  return ShouldShowNotification(notifier_id);
}

void MultiUserNotificationBlockerChromeOS::OnAppTerminating() {
  ash::Shell::GetInstance()->RemoveShellObserver(this);
  chromeos::UserManager::Get()->RemoveSessionStateObserver(this);
  observing_ = false;
}

void MultiUserNotificationBlockerChromeOS::ActiveUserChanged(
    const chromeos::User* active_user) {
  const std::string& new_user_id = active_user->email();
  if (active_user_id_ == new_user_id)
    return;

  quiet_modes_[active_user_id_] = message_center()->IsQuietMode();
  active_user_id_ = active_user->email();
  std::map<std::string, bool>::const_iterator iter =
      quiet_modes_.find(active_user_id_);
  if (iter != quiet_modes_.end() &&
      iter->second != message_center()->IsQuietMode()) {
    message_center()->SetQuietMode(iter->second);
  }
  NotifyBlockingStateChanged();
}

bool MultiUserNotificationBlockerChromeOS::IsActive() const {
  return observing_ && chrome::MultiUserWindowManager::GetMultiProfileMode() ==
      chrome::MultiUserWindowManager::MULTI_PROFILE_MODE_SEPARATED;
}
