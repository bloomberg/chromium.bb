// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_LOCKING_MANAGER_H_
#define ASH_SHELF_SHELF_LOCKING_MANAGER_H_

#include "ash/ash_export.h"
#include "ash/session/session_state_observer.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shell_observer.h"
#include "ash/wm/lock_state_observer.h"

namespace ash {

class Shelf;

// ShelfLockingManager observes screen and session events to [un]lock the shelf.
class ASH_EXPORT ShelfLockingManager : public ShellObserver,
                                       public SessionStateObserver,
                                       public LockStateObserver {
 public:
  explicit ShelfLockingManager(Shelf* shelf);
  ~ShelfLockingManager() override;

  // ShellObserver:
  void OnLockStateChanged(bool locked) override;

  // SessionStateObserver:
  void SessionStateChanged(SessionStateDelegate::SessionState state) override;

  // LockStateObserver:
  void OnLockStateEvent(EventType event) override;

 private:
  // Update the shelf state for session and screen lock changes.
  void UpdateLockedState();

  Shelf* shelf_;
  bool session_locked_ = false;
  bool screen_locked_ = false;
  ShelfAlignment stored_alignment_ = SHELF_ALIGNMENT_BOTTOM;

  DISALLOW_COPY_AND_ASSIGN(ShelfLockingManager);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_LOCKING_MANAGER_H_
