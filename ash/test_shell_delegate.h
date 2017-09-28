// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_SHELL_DELEGATE_H_
#define ASH_TEST_SHELL_DELEGATE_H_

#include <memory>

#include "ash/shell_delegate.h"
#include "base/macros.h"

namespace ash {

class TestShellDelegate : public ShellDelegate {
 public:
  TestShellDelegate();
  ~TestShellDelegate() override;

  // Overridden from ShellDelegate:
  ::service_manager::Connector* GetShellConnector() const override;
  bool IsRunningInForcedAppMode() const override;
  bool CanShowWindowForUser(aura::Window* window) const override;
  bool IsForceMaximizeOnFirstRun() const override;
  void PreInit() override;
  void PreShutdown() override;
  void Exit() override;
  std::unique_ptr<keyboard::KeyboardUI> CreateKeyboardUI() override;
  void OpenUrlFromArc(const GURL& url) override;
  NetworkingConfigDelegate* GetNetworkingConfigDelegate() override;
  std::unique_ptr<WallpaperDelegate> CreateWallpaperDelegate() override;
  AccessibilityDelegate* CreateAccessibilityDelegate() override;
  std::unique_ptr<PaletteDelegate> CreatePaletteDelegate() override;
  GPUSupport* CreateGPUSupport() override;
  base::string16 GetProductName() const override;
  gfx::Image GetDeprecatedAcceleratorImage() const override;
  ui::InputDeviceControllerClient* GetInputDeviceControllerClient() override;

  int num_exit_requests() const { return num_exit_requests_; }

  void SetForceMaximizeOnFirstRun(bool maximize) {
    force_maximize_on_first_run_ = maximize;
  }

 private:
  int num_exit_requests_ = 0;
  bool force_maximize_on_first_run_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestShellDelegate);
};

}  // namespace ash

#endif  // ASH_TEST_SHELL_DELEGATE_H_
