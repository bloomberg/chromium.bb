// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPS_LOCK_DELEGATE_STUB_H_
#define ASH_CAPS_LOCK_DELEGATE_STUB_H_

#include "ash/ash_export.h"
#include "ash/caps_lock_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace ash {

// Stub implementation of CapsLockDelegate mainly for testing.
class ASH_EXPORT CapsLockDelegateStub : public CapsLockDelegate {
 public:
  CapsLockDelegateStub();
  virtual ~CapsLockDelegateStub();

  // Overridden from CapsLockDelegate:
  virtual bool IsCapsLockEnabled() const OVERRIDE;
  virtual void SetCapsLockEnabled(bool enabled) OVERRIDE;
  virtual void ToggleCapsLock() OVERRIDE;

 private:
  bool enabled_;

  DISALLOW_COPY_AND_ASSIGN(CapsLockDelegateStub);
};

}  // namespace ash

#endif  // ASH_CAPS_LOCK_DELEGATE_STUB
