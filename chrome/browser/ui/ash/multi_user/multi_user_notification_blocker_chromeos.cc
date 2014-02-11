// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/multi_user_notification_blocker_chromeos.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/system_notifier.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "ui/aura/window.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notifier_settings.h"

MultiUserNotificationBlockerChromeOS::MultiUserNotificationBlockerChromeOS(
    message_center::MessageCenter* message_center,
    chrome::MultiUserWindowManager* multi_user_window_manager,
    const std::string& initial_user_id)
    : NotificationBlocker(message_center),
      multi_user_window_manager_(multi_user_window_manager),
      active_user_id_(initial_user_id) {
  UpdateWindowOwners();
  multi_user_window_manager_->AddObserver(this);
}

MultiUserNotificationBlockerChromeOS::~MultiUserNotificationBlockerChromeOS() {
  multi_user_window_manager_->RemoveObserver(this);
}

void MultiUserNotificationBlockerChromeOS::UpdateWindowOwners() {
  std::set<std::string> new_ids;
  multi_user_window_manager_->GetOwnersOfVisibleWindows(&new_ids);

  if (current_user_ids_ != new_ids) {
    current_user_ids_.swap(new_ids);
    NotifyBlockingStateChanged();
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
  return (current_user_ids_.find(notifier_id.profile_id) !=
          current_user_ids_.end()) ||
      ShouldShowNotification(notifier_id);
}

void MultiUserNotificationBlockerChromeOS::OnOwnerEntryAdded(
    aura::Window* window) {
  UpdateWindowOwners();
}

void MultiUserNotificationBlockerChromeOS::OnOwnerEntryChanged(
    aura::Window* window) {
  UpdateWindowOwners();
}

void MultiUserNotificationBlockerChromeOS::OnOwnerEntryRemoved(
    aura::Window* window) {
  UpdateWindowOwners();
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
  UpdateWindowOwners();
  NotifyBlockingStateChanged();
}

bool MultiUserNotificationBlockerChromeOS::IsActive() const {
  return chrome::MultiUserWindowManager::GetMultiProfileMode() ==
      chrome::MultiUserWindowManager::MULTI_PROFILE_MODE_SEPARATED;
}
