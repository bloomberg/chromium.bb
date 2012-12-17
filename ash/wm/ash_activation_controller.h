// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_ASH_ACTIVATION_CONTROLLER_H_
#define ASH_WM_ASH_ACTIVATION_CONTROLLER_H_

#include "ash/wm/activation_controller_delegate.h"
#include "base/compiler_specific.h"
#include "base/basictypes.h"

namespace ash {
namespace internal {

class AshActivationController : public ActivationControllerDelegate {
 public:
  AshActivationController();
  virtual ~AshActivationController();

 private:
  // Overridden from ActivationControllerDelegate:
  virtual aura::Window* WillActivateWindow(aura::Window* window) OVERRIDE;
  virtual aura::Window* WillFocusWindow(aura::Window* window) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AshActivationController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_ASH_ACTIVATION_CONTROLLER_H_
