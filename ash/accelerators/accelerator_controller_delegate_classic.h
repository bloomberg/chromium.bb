// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_CONTROLLER_DELEGATE_CLASSIC_H_
#define ASH_ACCELERATORS_ACCELERATOR_CONTROLLER_DELEGATE_CLASSIC_H_

#include <memory>

#include "ash/accelerators/accelerator_controller_delegate.h"
#include "base/macros.h"

namespace ash {

// Support for accelerators that only work in classic ash and not in mash,
// for example accelerators related to display management. These sorts of
// accelerators should be rare. Most new accelerators should be added to
// accelerator_controller.cc instead.
class ASH_EXPORT AcceleratorControllerDelegateClassic
    : public AcceleratorControllerDelegate {
 public:
  AcceleratorControllerDelegateClassic();
  ~AcceleratorControllerDelegateClassic() override;

  // AcceleratorControllerDelegate:
  bool HandlesAction(AcceleratorAction action) override;
  bool CanPerformAction(AcceleratorAction action,
                        const ui::Accelerator& accelerator,
                        const ui::Accelerator& previous_accelerator) override;
  void PerformAction(AcceleratorAction action,
                     const ui::Accelerator& accelerator) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AcceleratorControllerDelegateClassic);
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_CONTROLLER_DELEGATE_CLASSIC_H_
