// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_ASH_ACTIVATION_CONTROLLER_H_
#define ASH_WM_ASH_ACTIVATION_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/wm/activation_controller_delegate.h"
#include "base/compiler_specific.h"
#include "base/basictypes.h"

namespace ash {
namespace internal {

class ASH_EXPORT AshActivationController : public ActivationControllerDelegate {
 public:
  AshActivationController();
  virtual ~AshActivationController();

 private:
  // Overridden from ActivationControllerDelegate:
  virtual aura::Window* WillActivateWindow(aura::Window* window) OVERRIDE;
  virtual aura::Window* WillFocusWindow(aura::Window* window) OVERRIDE;

  // Returns a handle to the launcher on the active root window which will
  // be activated as fallback. Also notifies the launcher, so it can return
  // true from Launcher::CanActivate().
  aura::Window* PrepareToActivateLauncher();

  DISALLOW_COPY_AND_ASSIGN(AshActivationController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_ASH_ACTIVATION_CONTROLLER_H_
