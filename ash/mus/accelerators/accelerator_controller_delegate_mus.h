// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_ACCELERATORS_ACCELERATOR_CONTROLLER_DELEGATE_MUS_H_
#define ASH_MUS_ACCELERATORS_ACCELERATOR_CONTROLLER_DELEGATE_MUS_H_

#include "ash/accelerators/accelerator_controller_delegate.h"
#include "base/macros.h"
#include "services/ui/public/interfaces/display/test_display_controller.mojom.h"

namespace ash {
namespace mus {

class WindowManager;

// Controls accelerators that are specific to mash.
class AcceleratorControllerDelegateMus : public AcceleratorControllerDelegate {
 public:
  explicit AcceleratorControllerDelegateMus(WindowManager* window_manager);
  ~AcceleratorControllerDelegateMus() override;

  // AcceleratorControllerDelegate:
  bool HandlesAction(AcceleratorAction action) override;
  bool CanPerformAction(AcceleratorAction action,
                        const ui::Accelerator& accelerator,
                        const ui::Accelerator& previous_accelerator) override;
  void PerformAction(AcceleratorAction action,
                     const ui::Accelerator& accelerator) override;

 private:
  WindowManager* window_manager_;

  display::mojom::TestDisplayControllerPtr test_display_controller_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorControllerDelegateMus);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_ACCELERATORS_ACCELERATOR_CONTROLLER_DELEGATE_MUS_H_
