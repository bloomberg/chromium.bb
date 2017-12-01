// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_CONTROLLER_DELEGATE_MASH_H_
#define ASH_ACCELERATORS_ACCELERATOR_CONTROLLER_DELEGATE_MASH_H_

#include "ash/accelerators/accelerator_controller_delegate.h"
#include "base/macros.h"

namespace ash {

// Controls accelerators that are specific to mash.
// TODO(jamescook): Eliminate this class and inline the checks for mash into
// AcceleratorController.
class AcceleratorControllerDelegateMash : public AcceleratorControllerDelegate {
 public:
  AcceleratorControllerDelegateMash();
  ~AcceleratorControllerDelegateMash() override;

  // AcceleratorControllerDelegate:
  bool HandlesAction(AcceleratorAction action) override;
  bool CanPerformAction(AcceleratorAction action,
                        const ui::Accelerator& accelerator,
                        const ui::Accelerator& previous_accelerator) override;
  void PerformAction(AcceleratorAction action,
                     const ui::Accelerator& accelerator) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AcceleratorControllerDelegateMash);
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_CONTROLLER_DELEGATE_MASH_H_
