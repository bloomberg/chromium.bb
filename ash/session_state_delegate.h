// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SESSION_STATE_DELEGATE_H_
#define ASH_SESSION_STATE_DELEGATE_H_

#include "ash/ash_export.h"

namespace ash {

// Delegate for checking and modifying the session state.
class ASH_EXPORT SessionStateDelegate {
 public:
  virtual ~SessionStateDelegate() {};

  // Returns |true| if a session is in progress and there is an active user.
  virtual bool HasActiveUser() const = 0;

  // Returns |true| if the session has been fully started for the active user.
  // When a user becomes active, the profile and browser UI are not immediately
  // available. Only once this method starts returning |true| is the browser
  // startup complete and both profile and UI are fully available.
  virtual bool IsActiveUserSessionStarted() const = 0;

  // Returns true if the screen can be locked.
  virtual bool CanLockScreen() const = 0;

  // Returns true if the screen is currently locked.
  virtual bool IsScreenLocked() const = 0;

  // Locks the screen. The locking happens asynchronously.
  virtual void LockScreen() = 0;

  // Unlocks the screen.
  virtual void UnlockScreen() = 0;
};

}  // namespace ash

#endif  // ASH_SESSION_STATE_DELEGATE_H_
