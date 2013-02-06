// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CAPS_LOCK_DELEGATE_VIEWS_H_
#define CHROME_BROWSER_UI_ASH_CAPS_LOCK_DELEGATE_VIEWS_H_

#include "ash/caps_lock_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

// A class which toggles Caps Lock state when the hotkey for Caps Lock
// is pressed.
class CapsLockDelegate : public ash::CapsLockDelegate {
 public:
  CapsLockDelegate() {}
  virtual ~CapsLockDelegate() {}

  // Overridden from ash::CapsLockDelegate:
  virtual bool IsCapsLockEnabled() const OVERRIDE;
  virtual void SetCapsLockEnabled(bool enabled) OVERRIDE {}
  virtual void ToggleCapsLock() OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CapsLockDelegate);
};

#endif  // CHROME_BROWSER_UI_ASH_CAPS_LOCK_DELEGATE_VIEWS_H_
