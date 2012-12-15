// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_SESSION_LENGTH_LIMIT_TRAY_SESSION_LENGTH_LIMIT_H_
#define ASH_SYSTEM_SESSION_LENGTH_LIMIT_TRAY_SESSION_LENGTH_LIMIT_H_

#include "ash/system/session_length_limit/session_length_limit_observer.h"
#include "ash/system/tray/system_tray_item.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace ash {
namespace internal {

namespace tray {
class RemainingSessionTimeTrayView;
}

// Adds a countdown timer to the system tray if the session length is limited.
class TraySessionLengthLimit : public SystemTrayItem,
                               public SessionLengthLimitObserver {
 public:
  explicit TraySessionLengthLimit(SystemTray* system_tray);
  virtual ~TraySessionLengthLimit();

  // SystemTrayItem:
  virtual views::View* CreateTrayView(user::LoginStatus status) OVERRIDE;
  virtual void DestroyTrayView() OVERRIDE;
  virtual void UpdateAfterShelfAlignmentChange(
      ShelfAlignment alignment) OVERRIDE;

  // SessionLengthLimitObserver:
  virtual void OnSessionStartTimeChanged(
      const base::Time& start_time) OVERRIDE;
  virtual void OnSessionLengthLimitChanged(
      const base::TimeDelta& limit) OVERRIDE;

 private:
  tray::RemainingSessionTimeTrayView* tray_view_;

  DISALLOW_COPY_AND_ASSIGN(TraySessionLengthLimit);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_SESSION_LENGTH_LIMIT_TRAY_SESSION_LENGTH_LIMIT_H_
