// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_CONTROLLER_DELEGATE_H_
#define ASH_ACCELERATORS_ACCELERATOR_CONTROLLER_DELEGATE_H_

#include "ash/accelerators/accelerator_table.h"
#include "ash/ash_export.h"

namespace ui {
class Accelerator;
}

namespace ash {

// Used by AcceleratorController to handle environment specific commands. This
// file is temporary while ash supports both mus and aura.
class ASH_EXPORT AcceleratorControllerDelegate {
 public:
  // Returns true if the delegate is responsible for handling |action|. This
  // should not return whether the action may be enabled, only if this delegate
  // handles the action.
  virtual bool HandlesAction(AcceleratorAction action) = 0;

  // Returns true if the delegate can perform the action at this time. Only
  // invoked if HandlesAction() returns true.
  virtual bool CanPerformAction(
      AcceleratorAction action,
      const ui::Accelerator& accelerator,
      const ui::Accelerator& previous_accelerator) = 0;

  // Performs the specified action.
  virtual void PerformAction(AcceleratorAction action,
                             const ui::Accelerator& accelerator) = 0;

 protected:
  virtual ~AcceleratorControllerDelegate() {}
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_CONTROLLER_DELEGATE_H_
