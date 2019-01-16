// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_DELEGATE_MASH_H_
#define ASH_SHELL_DELEGATE_MASH_H_

#include <memory>

#include "ash/shell_delegate.h"
#include "base/macros.h"

namespace ash {

class ShellDelegateMash : public ShellDelegate {
 public:
  ShellDelegateMash();
  ~ShellDelegateMash() override;

  // ShellDelegate:
  bool CanShowWindowForUser(const aura::Window* window) const override;
  std::unique_ptr<ScreenshotDelegate> CreateScreenshotDelegate() override;
  AccessibilityDelegate* CreateAccessibilityDelegate() override;
  ws::InputDeviceControllerClient* GetInputDeviceControllerClient() override;

 private:
  std::unique_ptr<ws::InputDeviceControllerClient>
      input_device_controller_client_;

  DISALLOW_COPY_AND_ASSIGN(ShellDelegateMash);
};

}  // namespace ash

#endif  // ASH_SHELL_DELEGATE_MASH_H_
