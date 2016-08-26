// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_HANDLER_AURA_H_
#define ASH_ACCELERATORS_ACCELERATOR_HANDLER_AURA_H_

#include <memory>

#include "ash/common/accelerators/accelerator_controller_delegate.h"
#include "base/macros.h"

namespace ash {

class ScreenshotDelegate;

// Contains all the accelerators that have dependencies on portions of ash
// not yet converted to ash/common. When adding a new accelerator that only
// depends on ash/common code, add it to accelerator_controller.cc instead.
class ASH_EXPORT AcceleratorControllerDelegateAura
    : public AcceleratorControllerDelegate {
 public:
  AcceleratorControllerDelegateAura();
  ~AcceleratorControllerDelegateAura() override;

  void SetScreenshotDelegate(
      std::unique_ptr<ScreenshotDelegate> screenshot_delegate);
  ScreenshotDelegate* screenshot_delegate() {
    return screenshot_delegate_.get();
  }

  // AcceleratorControllerDelegate:
  bool HandlesAction(AcceleratorAction action) override;
  bool CanPerformAction(AcceleratorAction action,
                        const ui::Accelerator& accelerator,
                        const ui::Accelerator& previous_accelerator) override;
  void PerformAction(AcceleratorAction action,
                     const ui::Accelerator& accelerator) override;
  void ShowDeprecatedAcceleratorNotification(const char* const notification_id,
                                             int message_id,
                                             int old_shortcut_id,
                                             int new_shortcut_id) override;

 private:
  std::unique_ptr<ScreenshotDelegate> screenshot_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorControllerDelegateAura);
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_HANDLER_AURA_H_
