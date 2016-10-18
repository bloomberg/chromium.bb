// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHELF_SHELF_LOCKING_MANAGER_H_
#define ASH_COMMON_SHELF_SHELF_LOCKING_MANAGER_H_

#include "ash/ash_export.h"
#include "ash/common/session/session_state_observer.h"
#include "ash/common/shell_observer.h"
#include "ash/common/wm/lock_state_observer.h"
#include "ash/public/cpp/shelf_types.h"

namespace ash {

class WmShelf;

// ShelfLockingManager observes screen and session events to [un]lock the shelf.
class ASH_EXPORT ShelfLockingManager : public ShellObserver,
                                       public SessionStateObserver,
                                       public LockStateObserver {
 public:
  explicit ShelfLockingManager(WmShelf* shelf);
  ~ShelfLockingManager() override;

  bool is_locked() const { return session_locked_ || screen_locked_; }
  void set_stored_alignment(ShelfAlignment value) { stored_alignment_ = value; }

  // ShellObserver:
  void OnLockStateChanged(bool locked) override;

  // SessionStateObserver:
  void SessionStateChanged(session_manager::SessionState state) override;

  // LockStateObserver:
  void OnLockStateEvent(EventType event) override;

 private:
  // Update the shelf state for session and screen lock changes.
  void UpdateLockedState();

  WmShelf* shelf_;
  bool session_locked_ = false;
  bool screen_locked_ = false;
  ShelfAlignment stored_alignment_ = SHELF_ALIGNMENT_BOTTOM;

  DISALLOW_COPY_AND_ASSIGN(ShelfLockingManager);
};

}  // namespace ash

#endif  // ASH_COMMON_SHELF_SHELF_LOCKING_MANAGER_H_
