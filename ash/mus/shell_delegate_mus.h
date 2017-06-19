// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_SHELL_DELEGATE_MUS_H_
#define ASH_MUS_SHELL_DELEGATE_MUS_H_

#include <memory>

#include "ash/shell_delegate.h"
#include "base/macros.h"

namespace service_manager {
class Connector;
}

namespace ash {

class ShellDelegateMus : public ShellDelegate {
 public:
  explicit ShellDelegateMus(service_manager::Connector* connector);
  ~ShellDelegateMus() override;

  // ShellDelegate:
  service_manager::Connector* GetShellConnector() const override;
  bool IsIncognitoAllowed() const override;
  bool IsMultiProfilesEnabled() const override;
  bool IsRunningInForcedAppMode() const override;
  bool CanShowWindowForUser(aura::Window* window) const override;
  bool IsForceMaximizeOnFirstRun() const override;
  void PreInit() override;
  void PreShutdown() override;
  void Exit() override;
  keyboard::KeyboardUI* CreateKeyboardUI() override;
  void OpenUrlFromArc(const GURL& url) override;
  void ShelfInit() override;
  void ShelfShutdown() override;
  SystemTrayDelegate* CreateSystemTrayDelegate() override;
  std::unique_ptr<WallpaperDelegate> CreateWallpaperDelegate() override;
  AccessibilityDelegate* CreateAccessibilityDelegate() override;
  std::unique_ptr<PaletteDelegate> CreatePaletteDelegate() override;
  ui::MenuModel* CreateContextMenu(Shelf* shelf,
                                   const ShelfItem* item) override;
  GPUSupport* CreateGPUSupport() override;
  base::string16 GetProductName() const override;
  gfx::Image GetDeprecatedAcceleratorImage() const override;
  PrefService* GetActiveUserPrefService() const override;
  bool IsTouchscreenEnabledInPrefs(bool use_local_state) const override;
  void SetTouchscreenEnabledInPrefs(bool enabled,
                                    bool use_local_state) override;
  void UpdateTouchscreenStatusFromPrefs() override;
#if defined(USE_OZONE)
  ui::InputDeviceControllerClient* GetInputDeviceControllerClient() override;
#endif

 private:
  // |connector_| may be null in tests.
  service_manager::Connector* connector_;

#if defined(USE_OZONE)
  std::unique_ptr<ui::InputDeviceControllerClient>
      input_device_controller_client_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ShellDelegateMus);
};

}  // namespace ash

#endif  // ASH_MUS_SHELL_DELEGATE_MUS_H_
