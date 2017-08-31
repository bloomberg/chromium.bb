// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_SHELL_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_CHROME_SHELL_DELEGATE_H_

#include <memory>
#include <string>

#include "ash/shell_delegate.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace chromeos {
class DisplayConfigurationObserver;
}

namespace keyboard {
class KeyboardUI;
}

class ChromeLauncherController;

class ChromeShellDelegate : public ash::ShellDelegate,
                            public content::NotificationObserver {
 public:
  ChromeShellDelegate();
  ~ChromeShellDelegate() override;

  // ash::ShellDelegate overrides;
  service_manager::Connector* GetShellConnector() const override;
  bool IsMultiProfilesEnabled() const override;
  bool IsIncognitoAllowed() const override;
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
  ash::NetworkingConfigDelegate* GetNetworkingConfigDelegate() override;
  std::unique_ptr<ash::WallpaperDelegate> CreateWallpaperDelegate() override;
  ash::AccessibilityDelegate* CreateAccessibilityDelegate() override;
  std::unique_ptr<ash::PaletteDelegate> CreatePaletteDelegate() override;
  ash::GPUSupport* CreateGPUSupport() override;
  base::string16 GetProductName() const override;
  void OpenKeyboardShortcutHelpPage() const override;
  gfx::Image GetDeprecatedAcceleratorImage() const override;
  bool GetTouchscreenEnabled(
      ash::TouchscreenEnabledSource source) const override;
  void SetTouchscreenEnabled(bool enabled,
                             ash::TouchscreenEnabledSource source) override;
  void ToggleTouchpad() override;
  void SuspendMediaSessions() override;
  ui::InputDeviceControllerClient* GetInputDeviceControllerClient() override;

  // content::NotificationObserver override:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  void PlatformInit();

  content::NotificationRegistrar registrar_;

  std::unique_ptr<ChromeLauncherController> launcher_controller_;

  std::unique_ptr<chromeos::DisplayConfigurationObserver>
      display_configuration_observer_;

  std::unique_ptr<ash::NetworkingConfigDelegate> networking_config_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ChromeShellDelegate);
};

#endif  // CHROME_BROWSER_UI_ASH_CHROME_SHELL_DELEGATE_H_
