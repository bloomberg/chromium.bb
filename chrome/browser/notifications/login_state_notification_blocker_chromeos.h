// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_LOGIN_STATE_NOTIFICATION_BLOCKER_CHROMEOS_H_
#define CHROME_BROWSER_NOTIFICATIONS_LOGIN_STATE_NOTIFICATION_BLOCKER_CHROMEOS_H_

#include "base/macros.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "ui/message_center/notification_blocker.h"

// A notification blocker which checks screen lock / login state for ChromeOS.
// This is different from ScreenLockNotificationBlocker because:
//  - ScreenLockNotificationBlocker only cares about lock status but ChromeOS
//    needs to care about login-screen.
//  - ScreenLockNotificationBlocker needs a timer to check the screen lock state
//    periodically, but SessionManagerObserver gets the events directly in
//    ChromeOS.
//  - In ChromeOS, some system notifications are allowed to be shown as popups.
class LoginStateNotificationBlockerChromeOS
    : public message_center::NotificationBlocker,
      public session_manager::SessionManagerObserver {
 public:
  explicit LoginStateNotificationBlockerChromeOS(
      message_center::MessageCenter* message_center);
  ~LoginStateNotificationBlockerChromeOS() override;

 private:
  // message_center::NotificationBlocker overrides:
  bool ShouldShowNotificationAsPopup(
      const message_center::Notification& notification) const override;

  // session_manager::SessionManagerObserver overrides:
  void OnSessionStateChanged() override;

  DISALLOW_COPY_AND_ASSIGN(LoginStateNotificationBlockerChromeOS);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_LOGIN_STATE_NOTIFICATION_BLOCKER_CHROMEOS_H_
