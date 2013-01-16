// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/power_button_observer.h"

#include "ash/shell.h"
#include "ash/system/user/login_status.h"
#include "ash/wm/power_button_controller.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/power/session_state_controller_delegate_chromeos.h"
#include "chrome/common/chrome_notification_types.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/root_power_manager_client.h"
#include "content/public/browser/notification_service.h"

namespace chromeos {

namespace {

ash::user::LoginStatus GetCurrentLoginStatus() {
  const UserManager* user_manager = UserManager::Get();
  if (!user_manager->IsUserLoggedIn())
    return ash::user::LOGGED_IN_NONE;
  if (user_manager->IsCurrentUserOwner())
    return ash::user::LOGGED_IN_OWNER;

  switch (user_manager->GetLoggedInUser()->GetType()) {
    case User::USER_TYPE_REGULAR:
      return ash::user::LOGGED_IN_USER;
    case User::USER_TYPE_GUEST:
      return ash::user::LOGGED_IN_GUEST;
    case User::USER_TYPE_RETAIL_MODE:
      return ash::user::LOGGED_IN_KIOSK;
    case User::USER_TYPE_PUBLIC_ACCOUNT:
      return ash::user::LOGGED_IN_PUBLIC;
    case User::USER_TYPE_LOCALLY_MANAGED:
      return ash::user::LOGGED_IN_LOCALLY_MANAGED;
  }

  NOTREACHED();
  return ash::user::LOGGED_IN_USER;
}

}  // namespace

PowerButtonObserver::PowerButtonObserver() {
  ash::Shell::GetInstance()->session_state_controller()->
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
  DBusThreadManager::Get()->GetRootPowerManagerClient()->AddObserver(this);
  DBusThreadManager::Get()->GetSessionManagerClient()->AddObserver(this);

  // Tell the controller about the initial state.
  ash::Shell::GetInstance()->OnLoginStateChanged(GetCurrentLoginStatus());

  const ScreenLocker* locker = ScreenLocker::default_screen_locker();
  bool locked = locker && locker->locked();
  ash::Shell::GetInstance()->OnLockStateChanged(locked);
}

PowerButtonObserver::~PowerButtonObserver() {
  DBusThreadManager::Get()->GetSessionManagerClient()->RemoveObserver(this);
  DBusThreadManager::Get()->GetRootPowerManagerClient()->RemoveObserver(this);
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

void PowerButtonObserver::OnPowerButtonEvent(
    bool down, const base::TimeTicks& timestamp) {
  PowerButtonEventReceived(down, timestamp);
}

void PowerButtonObserver::LockScreen() {
  ash::Shell::GetInstance()->session_state_controller()->OnStartingLock();
}

}  // namespace chromeos
