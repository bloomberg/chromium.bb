// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/power_button_observer.h"

#include "ash/shell.h"
#include "ash/wm/power_button_controller.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/power/power_button_controller_delegate_chromeos.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"

namespace chromeos {

PowerButtonObserver::PowerButtonObserver() {
  ash::PowerButtonController* controller =
      ash::Shell::GetInstance()->power_button_controller();
  controller->set_delegate(new PowerButtonControllerDelegateChromeos);

  registrar_.Add(
      this,
      chrome::NOTIFICATION_LOGIN_USER_CHANGED,
      content::NotificationService::AllSources());
  registrar_.Add(
      this,
      chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
      content::NotificationService::AllSources());

  // Tell the controller about the initial state.
  const UserManager* user_manager = UserManager::Get();
  bool logged_in = user_manager->user_is_logged_in();
  bool is_guest = logged_in && user_manager->logged_in_user().is_guest();
  controller->OnLoginStateChange(logged_in, is_guest);

  const ScreenLocker* locker = ScreenLocker::default_screen_locker();
  bool locked = locker && locker->locked();
  controller->OnLockStateChange(locked);
}

PowerButtonObserver::~PowerButtonObserver() {
}

void PowerButtonObserver::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_LOGIN_USER_CHANGED: {
      const User* user = content::Details<const User>(details).ptr();
      ash::Shell::GetInstance()->power_button_controller()->
          OnLoginStateChange(true /* logged_in */, user->is_guest());
      break;
    }
    case chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED: {
      bool locked = *content::Details<bool>(details).ptr();
      ash::Shell::GetInstance()->power_button_controller()->
          OnLockStateChange(locked);
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification " << type;
  }
}

void PowerButtonObserver::PowerButtonStateChanged(
    bool down, const base::TimeTicks& timestamp) {
  ash::Shell::GetInstance()->power_button_controller()->
      OnPowerButtonEvent(down, timestamp);
}

void PowerButtonObserver::LockButtonStateChanged(
    bool down, const base::TimeTicks& timestamp) {
  ash::Shell::GetInstance()->power_button_controller()->
      OnLockButtonEvent(down, timestamp);
}

void PowerButtonObserver::LockScreen() {
  ash::Shell::GetInstance()->power_button_controller()->OnStartingLock();
}

}  // namespace chromeos
