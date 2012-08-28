// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPS_LOCK_DELEGATE_H_
#define ASH_CAPS_LOCK_DELEGATE_H_

#include "ash/ash_export.h"

namespace ash {

// Delegate for controlling Caps Lock.
class ASH_EXPORT CapsLockDelegate {
 public:
  virtual ~CapsLockDelegate() {}

  // Returns true if caps lock is enabled.
  virtual bool IsCapsLockEnabled() const = 0;

  // Sets the caps lock state to |enabled|.
  // The state change can occur asynchronously and calling IsCapsLockEnabled
  // just after this may return the old state.
  virtual void SetCapsLockEnabled(bool enabled) = 0;

  // Toggles the caps lock state.
  // The state change can occur asynchronously and calling IsCapsLockEnabled
  // just after this may return the old state.
  virtual void ToggleCapsLock() = 0;
};

}  // namespace ash

#endif  // ASH_CAPS_LOCK_DELEGATE_H_
