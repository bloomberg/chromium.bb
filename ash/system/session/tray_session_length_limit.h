// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_SESSION_TRAY_SESSION_LENGTH_LIMIT_H_
#define ASH_SYSTEM_SESSION_TRAY_SESSION_LENGTH_LIMIT_H_

#include <memory>

#include "ash/session/session_observer.h"
#include "ash/system/tray/system_tray_item.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace ash {

class LabelTrayView;

// Adds a countdown timer to the system tray if the session length is limited.
class ASH_EXPORT TraySessionLengthLimit : public SystemTrayItem,
                                          public SessionObserver {
 public:
  enum LimitState { LIMIT_NONE, LIMIT_SET, LIMIT_EXPIRING_SOON };

  explicit TraySessionLengthLimit(SystemTray* system_tray);
  ~TraySessionLengthLimit() override;

  // SystemTrayItem:
  views::View* CreateDefaultView(LoginStatus status) override;
  void OnDefaultViewDestroyed() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnSessionLengthLimitChanged() override;

 private:
  friend class TraySessionLengthLimitTest;

  static const char kNotificationId[];

  // Update state, notification and tray bubble view.  Called by the
  // RepeatingTimer in regular intervals and also by OnSession*Changed().
  void Update();

  // Recalculate |limit_state_| and |remaining_session_time_|.
  void UpdateState();

  void UpdateNotification();
  void UpdateTrayBubbleView() const;

  // These require that the state has been updated before.
  base::string16 ComposeNotificationMessage() const;
  base::string16 ComposeTrayBubbleMessage() const;

  base::TimeDelta remaining_session_time_;

  LimitState limit_state_;       // Current state.
  LimitState last_limit_state_;  // State of last notification update.
  bool has_notification_been_shown_;

  LabelTrayView* tray_bubble_view_;
  std::unique_ptr<base::RepeatingTimer> timer_;

  DISALLOW_COPY_AND_ASSIGN(TraySessionLengthLimit);
};

}  // namespace ash

#endif  // ASH_SYSTEM_SESSION_TRAY_SESSION_LENGTH_LIMIT_H_
