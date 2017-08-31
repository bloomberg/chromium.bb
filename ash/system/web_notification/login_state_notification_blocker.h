// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_WEB_NOTIFICATION_LOGIN_STATE_NOTIFICATION_BLOCKER_H_
#define ASH_SYSTEM_WEB_NOTIFICATION_LOGIN_STATE_NOTIFICATION_BLOCKER_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "ui/message_center/notification_blocker.h"

namespace ash {

// A notification blocker which suppresses notifications popups based on the
// login state reported by the SessionManager. Only active (logged in, unlocked)
// sessions will show popups.
class ASH_EXPORT LoginStateNotificationBlocker
    : public message_center::NotificationBlocker,
      public session_manager::SessionManagerObserver {
 public:
  explicit LoginStateNotificationBlocker(
      message_center::MessageCenter* message_center);
  ~LoginStateNotificationBlocker() override;

 private:
  // message_center::NotificationBlocker overrides:
  bool ShouldShowNotificationAsPopup(
      const message_center::Notification& notification) const override;

  // session_manager::SessionManagerObserver overrides:
  void OnSessionStateChanged() override;

  DISALLOW_COPY_AND_ASSIGN(LoginStateNotificationBlocker);
};

}  // namespace ash

#endif  // ASH_SYSTEM_WEB_NOTIFICATION_LOGIN_STATE_NOTIFICATION_BLOCKER_H_
