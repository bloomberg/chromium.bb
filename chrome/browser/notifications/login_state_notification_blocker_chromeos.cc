// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/login_state_notification_blocker_chromeos.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/system_notifier.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/wm/window_properties.h"
#include "chrome/browser/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/message_center/message_center.h"

LoginStateNotificationBlockerChromeOS::LoginStateNotificationBlockerChromeOS(
    message_center::MessageCenter* message_center)
    : NotificationBlocker(message_center),
      locked_(false),
      observing_(true) {
  // This class is created in the ctor of NotificationUIManager which is created
  // when a notification is created, so ash::Shell should be initialized.
  ash::Shell::GetInstance()->AddShellObserver(this);
}

LoginStateNotificationBlockerChromeOS::
    ~LoginStateNotificationBlockerChromeOS() {
  // In some tests, the notification blockers may be removed without calling
  // OnAppTerminating().
  if (observing_ && ash::Shell::HasInstance())
    ash::Shell::GetInstance()->RemoveShellObserver(this);
}

bool LoginStateNotificationBlockerChromeOS::ShouldShowNotificationAsPopup(
    const message_center::NotifierId& notifier_id) const {
  if (ash::system_notifier::ShouldAlwaysShowPopups(notifier_id))
    return true;

  if (!locked_)
    return true;

  ash::user::LoginStatus login_status =
      ash::Shell::GetInstance()->system_tray_delegate()->GetUserLoginStatus();
  return login_status != ash::user::LOGGED_IN_NONE &&
      login_status != ash::user::LOGGED_IN_LOCKED;
}

void LoginStateNotificationBlockerChromeOS::OnLoginStateChanged(
    ash::user::LoginStatus login_status) {
  FOR_EACH_OBSERVER(
      NotificationBlocker::Observer, observers(), OnBlockingStateChanged());
}

void LoginStateNotificationBlockerChromeOS::OnLockStateChanged(bool locked) {
  locked_ = locked;
  FOR_EACH_OBSERVER(
      NotificationBlocker::Observer, observers(), OnBlockingStateChanged());
}

void LoginStateNotificationBlockerChromeOS::OnAppTerminating() {
  ash::Shell::GetInstance()->RemoveShellObserver(this);
  observing_ = false;
}
