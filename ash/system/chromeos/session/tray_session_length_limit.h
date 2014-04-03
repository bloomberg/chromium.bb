// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_SESSION_SESSION_LENGTH_LIMIT_H_
#define ASH_SYSTEM_CHROMEOS_SESSION_SESSION_LENGTH_LIMIT_H_

#include "ash/system/chromeos/session/session_length_limit_observer.h"
#include "ash/system/tray/system_tray_item.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace ash {
namespace test {
class TraySessionLengthLimitTest;
}

namespace tray {
class RemainingSessionTimeTrayView;
}

// Adds a countdown timer to the system tray if the session length is limited.
class ASH_EXPORT TraySessionLengthLimit : public SystemTrayItem,
                                          public SessionLengthLimitObserver {
 public:
  enum LimitState {
    LIMIT_NONE,
    LIMIT_SET,
    LIMIT_EXPIRING_SOON
  };

  explicit TraySessionLengthLimit(SystemTray* system_tray);
  virtual ~TraySessionLengthLimit();

  // SystemTrayItem:
  virtual views::View* CreateTrayView(user::LoginStatus status) OVERRIDE;
  virtual void DestroyTrayView() OVERRIDE;
  virtual void UpdateAfterShelfAlignmentChange(
      ShelfAlignment alignment) OVERRIDE;

  // SessionLengthLimitObserver:
  virtual void OnSessionStartTimeChanged() OVERRIDE;
  virtual void OnSessionLengthLimitChanged() OVERRIDE;

  LimitState GetLimitState() const;
  base::TimeDelta GetRemainingSessionTime() const;

 private:
  friend class test::TraySessionLengthLimitTest;

  static const char kNotificationId[];

  void Update();

  bool IsTrayViewVisibleForTest();

  tray::RemainingSessionTimeTrayView* tray_view_;

  LimitState limit_state_;
  base::TimeTicks session_start_time_;
  base::TimeDelta limit_;
  base::TimeDelta remaining_session_time_;
  scoped_ptr<base::RepeatingTimer<TraySessionLengthLimit> > timer_;

  DISALLOW_COPY_AND_ASSIGN(TraySessionLengthLimit);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_SESSION_SESSION_LENGTH_LIMIT_H_
