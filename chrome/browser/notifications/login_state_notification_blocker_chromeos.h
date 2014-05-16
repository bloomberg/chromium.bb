// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_LOGIN_STATE_NOTIFICATION_BLOCKER_CHROMEOS_H_
#define CHROME_BROWSER_NOTIFICATIONS_LOGIN_STATE_NOTIFICATION_BLOCKER_CHROMEOS_H_

#include "ash/shell_observer.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chromeos/login/login_state.h"
#include "ui/message_center/notification_blocker.h"

// A notification blocker which checks screen lock / login state for ChromeOS.
// This is different from ScreenLockNotificationBlocker because:
//  - ScreenLockNotificationBlocker only cares about lock status but ChromeOS
//    needs to care about login-screen.
//  - ScreenLockNotificationBlocker needs a timer to check the screen lock state
//    periodically, but ash::ShellObserver gets the events directly in ChromeOS.
//  - In ChromeOS, some system notifications are allowed to be shown as popups.
class LoginStateNotificationBlockerChromeOS
    : public message_center::NotificationBlocker,
      public ash::ShellObserver,
      public chromeos::LoginState::Observer,
      public chromeos::UserAddingScreen::Observer {
 public:
  explicit LoginStateNotificationBlockerChromeOS(
      message_center::MessageCenter* message_center);
  virtual ~LoginStateNotificationBlockerChromeOS();

 private:
  // message_center::NotificationBlocker overrides:
  virtual bool ShouldShowNotificationAsPopup(
      const message_center::NotifierId& notifier_id) const OVERRIDE;

  // ash::ShellObserver overrides:
  virtual void OnLockStateChanged(bool locked) OVERRIDE;
  virtual void OnAppTerminating() OVERRIDE;

  // chromeos::LoginState::Observer overrides:
  virtual void LoggedInStateChanged() OVERRIDE;

  // chromeos::UserAddingScreen::Observer overrides:
  virtual void OnUserAddingStarted() OVERRIDE;
  virtual void OnUserAddingFinished() OVERRIDE;

  bool locked_;
  bool observing_;

  DISALLOW_COPY_AND_ASSIGN(LoginStateNotificationBlockerChromeOS);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_LOGIN_STATE_NOTIFICATION_BLOCKER_CHROMEOS_H_
