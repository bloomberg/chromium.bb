// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_SESSION_STATE_NOTIFICATION_BLOCKER_H_
#define ASH_SYSTEM_MESSAGE_CENTER_SESSION_STATE_NOTIFICATION_BLOCKER_H_

#include "ash/ash_export.h"
#include "ash/session/session_observer.h"
#include "base/macros.h"
#include "ui/message_center/notification_blocker.h"

namespace ash {

// A notification blocker which suppresses notifications popups based on the
// session state and active user PrefService readiness reported by the
// SessionController. Only active (logged in, unlocked) sessions with
// initialized PrefService will show user notifications. Kiosk mode sessions
// will never show even system notifications. System notifications with
// elevated priority will be shown regardless of the login/lock state.
class ASH_EXPORT SessionStateNotificationBlocker
    : public message_center::NotificationBlocker,
      public SessionObserver {
 public:
  explicit SessionStateNotificationBlocker(
      message_center::MessageCenter* message_center);
  ~SessionStateNotificationBlocker() override;

 private:
  // message_center::NotificationBlocker overrides:
  bool ShouldShowNotification(
      const message_center::Notification& notification) const override;
  bool ShouldShowNotificationAsPopup(
      const message_center::Notification& notification) const override;

  // SessionObserver overrides:
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  void CheckStateAndNotifyIfChanged();

  bool should_show_notification_ = false;
  bool should_show_popup_ = false;

  DISALLOW_COPY_AND_ASSIGN(SessionStateNotificationBlocker);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_SESSION_STATE_NOTIFICATION_BLOCKER_H_
