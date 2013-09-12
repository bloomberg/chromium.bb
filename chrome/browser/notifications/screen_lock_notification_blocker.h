// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCREEN_LOCK_NOTIFICATION_BLOCKER_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCREEN_LOCK_NOTIFICATION_BLOCKER_H_

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "ui/message_center/notification_blocker.h"

// A notification blocker which checks the screen lock state constantly.
class ScreenLockNotificationBlocker
    : public message_center::NotificationBlocker {
 public:
  explicit ScreenLockNotificationBlocker(
      message_center::MessageCenter* message_center);
  virtual ~ScreenLockNotificationBlocker();

  bool is_locked() const { return is_locked_; }

  // message_center::NotificationBlocker overrides:
  virtual void CheckState() OVERRIDE;
  virtual bool ShouldShowNotificationAsPopup(
      const message_center::NotifierId& notifier_id) const OVERRIDE;

 private:
  bool is_locked_;

  base::OneShotTimer<ScreenLockNotificationBlocker> timer_;

  DISALLOW_COPY_AND_ASSIGN(ScreenLockNotificationBlocker);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCREEN_LOCK_NOTIFICATION_BLOCKER_H_
