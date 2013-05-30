// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_SESSION_STATE_CONTROLLER_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_POWER_SESSION_STATE_CONTROLLER_DELEGATE_CHROMEOS_H_

#include "ash/wm/lock_state_controller.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace chromeos {

class SessionStateControllerDelegateChromeos
    : public ash::LockStateControllerDelegate {
 public:
  SessionStateControllerDelegateChromeos() {}
  virtual ~SessionStateControllerDelegateChromeos() {}

 private:
  // SessionStateControllerDelegate implementation.
  virtual void RequestLockScreen() OVERRIDE;
  virtual void RequestShutdown() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(SessionStateControllerDelegateChromeos);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_SESSION_STATE_CONTROLLER_DELEGATE_CHROMEOS_H_
