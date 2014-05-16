// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/power_button_observer.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/user/login_status.h"
#include "ash/wm/power_button_controller.h"
#include "base/logging.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/power/session_state_controller_delegate_chromeos.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/browser/notification_service.h"

namespace chromeos {

namespace {

ash::user::LoginStatus GetCurrentLoginStatus() {
  if (!ash::Shell::GetInstance()->system_tray_delegate())
    return ash::user::LOGGED_IN_NONE;
  return ash::Shell::GetInstance()->system_tray_delegate()->
      GetUserLoginStatus();
}

}  // namespace

PowerButtonObserver::PowerButtonObserver() {
  ash::Shell::GetInstance()->lock_state_controller()->
      SetDelegate(new SessionStateControllerDelegateChromeos);

  registrar_.Add(
      this,
      chrome::NOTIFICATION_LOGIN_USER_CHANGED,
      content::NotificationService::AllSources());
  registrar_.Add(
      this,
      chrome::NOTIFICATION_APP_TERMINATING,
      content::NotificationService::AllSources());
  registrar_.Add(
      this,
      chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
      content::NotificationService::AllSources());

  DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
  DBusThreadManager::Get()->GetSessionManagerClient()->AddObserver(this);

  // Tell the controller about the initial state.
  ash::Shell::GetInstance()->OnLoginStateChanged(GetCurrentLoginStatus());

  const ScreenLocker* locker = ScreenLocker::default_screen_locker();
  bool locked = locker && locker->locked();
  ash::Shell::GetInstance()->OnLockStateChanged(locked);
}

PowerButtonObserver::~PowerButtonObserver() {
  DBusThreadManager::Get()->GetSessionManagerClient()->RemoveObserver(this);
  DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
}

void PowerButtonObserver::Observe(int type,
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

void PowerButtonObserver::PowerButtonEventReceived(
    bool down, const base::TimeTicks& timestamp) {
  ash::Shell::GetInstance()->power_button_controller()->
      OnPowerButtonEvent(down, timestamp);
}

}  // namespace chromeos
