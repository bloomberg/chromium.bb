// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/multi_user_notification_blocker_chromeos.h"

#include "ash/system/system_notifier.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notifier_settings.h"

MultiUserNotificationBlockerChromeOS::MultiUserNotificationBlockerChromeOS(
    message_center::MessageCenter* message_center,
    const std::string& initial_user_id)
    : NotificationBlocker(message_center),
      active_user_id_(initial_user_id) {
}

MultiUserNotificationBlockerChromeOS::~MultiUserNotificationBlockerChromeOS() {
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

void MultiUserNotificationBlockerChromeOS::ActiveUserChanged(
    const std::string& user_id) {
  if (active_user_id_ == user_id)
    return;

  quiet_modes_[active_user_id_] = message_center()->IsQuietMode();
  active_user_id_ = user_id;
  std::map<std::string, bool>::const_iterator iter =
      quiet_modes_.find(active_user_id_);
  if (iter != quiet_modes_.end() &&
      iter->second != message_center()->IsQuietMode()) {
    message_center()->SetQuietMode(iter->second);
  }
  NotifyBlockingStateChanged();
}

bool MultiUserNotificationBlockerChromeOS::IsActive() const {
  return chrome::MultiUserWindowManager::GetMultiProfileMode() ==
      chrome::MultiUserWindowManager::MULTI_PROFILE_MODE_SEPARATED;
}
