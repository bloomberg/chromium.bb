// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/login_lock_state_notifier.h"

#include "ash/common/login_status.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/wm_shell.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "content/public/browser/notification_service.h"

namespace ash {
class LockStateControllerDelegate;
}

namespace chromeos {

namespace {

ash::LoginStatus GetCurrentLoginStatus() {
  if (ash::WmShell::Get()->system_tray_delegate())
    return ash::WmShell::Get()->system_tray_delegate()->GetUserLoginStatus();

  return ash::LoginStatus::NOT_LOGGED_IN;
}

}  // namespace

LoginLockStateNotifier::LoginLockStateNotifier() {
  registrar_.Add(this, chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
                 content::NotificationService::AllSources());

  // Tell the controller about the initial state.
  ash::Shell::GetInstance()->OnLoginStateChanged(GetCurrentLoginStatus());

  const ScreenLocker* locker = ScreenLocker::default_screen_locker();
  bool locked = locker && locker->locked();
  ash::Shell::GetInstance()->OnLockStateChanged(locked);
}

LoginLockStateNotifier::~LoginLockStateNotifier() {}

void LoginLockStateNotifier::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_LOGIN_USER_CHANGED: {
      ash::Shell::GetInstance()->OnLoginStateChanged(GetCurrentLoginStatus());
      break;
    }
    case chrome::NOTIFICATION_APP_TERMINATING:
      ash::Shell::GetInstance()->OnAppTerminating();
      break;
    case chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED: {
      bool locked = *content::Details<bool>(details).ptr();
      ash::Shell::GetInstance()->OnLockStateChanged(locked);
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification " << type;
  }
}

}  // namespace chromeos
