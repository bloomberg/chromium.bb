// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_SHELL_DELEGATE_IMPL_H_
#define ASH_SHELL_SHELL_DELEGATE_IMPL_H_

#include <memory>
#include <string>

#include "ash/shell_delegate.h"
#include "base/macros.h"

namespace keyboard {
class KeyboardUI;
}

namespace ash {
namespace shell {

class ShellDelegateImpl : public ShellDelegate {
 public:
  ShellDelegateImpl();
  ~ShellDelegateImpl() override;

  // ShellDelegate:
  ::service_manager::Connector* GetShellConnector() const override;
  bool IsIncognitoAllowed() const override;
  bool IsMultiProfilesEnabled() const override;
  bool IsRunningInForcedAppMode() const override;
  bool CanShowWindowForUser(aura::Window* window) const override;
  bool IsForceMaximizeOnFirstRun() const override;
  void PreInit() override;
  void PreShutdown() override;
  void Exit() override;
  std::unique_ptr<keyboard::KeyboardUI> CreateKeyboardUI() override;
  void OpenUrlFromArc(const GURL& url) override;
  void ShelfInit() override;
  void ShelfShutdown() override;
  NetworkingConfigDelegate* GetNetworkingConfigDelegate() override;
  std::unique_ptr<WallpaperDelegate> CreateWallpaperDelegate() override;
  AccessibilityDelegate* CreateAccessibilityDelegate() override;
  std::unique_ptr<PaletteDelegate> CreatePaletteDelegate() override;
  GPUSupport* CreateGPUSupport() override;
  base::string16 GetProductName() const override;
  gfx::Image GetDeprecatedAcceleratorImage() const override;
  bool IsTouchscreenEnabledInPrefs(bool use_local_state) const override;
  void SetTouchscreenEnabledInPrefs(bool enabled,
                                    bool use_local_state) override;
  void UpdateTouchscreenStatusFromPrefs() override;
  ui::InputDeviceControllerClient* GetInputDeviceControllerClient() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellDelegateImpl);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_SHELL_DELEGATE_IMPL_H_
