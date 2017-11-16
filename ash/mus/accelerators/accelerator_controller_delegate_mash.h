// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_ACCELERATORS_ACCELERATOR_CONTROLLER_DELEGATE_MASH_H_
#define ASH_MUS_ACCELERATORS_ACCELERATOR_CONTROLLER_DELEGATE_MASH_H_

#include "ash/accelerators/accelerator_controller_delegate.h"
#include "base/macros.h"

namespace ash {

class WindowManager;

// Controls accelerators that are specific to mash.
class AcceleratorControllerDelegateMash : public AcceleratorControllerDelegate {
 public:
  explicit AcceleratorControllerDelegateMash(WindowManager* window_manager);
  ~AcceleratorControllerDelegateMash() override;

  // AcceleratorControllerDelegate:
  bool HandlesAction(AcceleratorAction action) override;
  bool CanPerformAction(AcceleratorAction action,
                        const ui::Accelerator& accelerator,
                        const ui::Accelerator& previous_accelerator) override;
  void PerformAction(AcceleratorAction action,
                     const ui::Accelerator& accelerator) override;

 private:
  WindowManager* window_manager_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorControllerDelegateMash);
};

}  // namespace ash

#endif  // ASH_MUS_ACCELERATORS_ACCELERATOR_CONTROLLER_DELEGATE_MASH_H_
