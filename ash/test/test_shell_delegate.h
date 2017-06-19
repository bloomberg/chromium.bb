// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_SHELL_DELEGATE_H_
#define ASH_TEST_TEST_SHELL_DELEGATE_H_

#include <memory>
#include <string>

#include "ash/shell_delegate.h"
#include "base/macros.h"

class PrefService;

namespace keyboard {
class KeyboardUI;
}

namespace ash {
namespace test {

class ShelfInitializer;

class TestShellDelegate : public ShellDelegate {
 public:
  TestShellDelegate();
  ~TestShellDelegate() override;

  void set_multi_profiles_enabled(bool multi_profiles_enabled) {
    multi_profiles_enabled_ = multi_profiles_enabled;
  }

  void set_active_user_pref_service(PrefService* pref_service) {
    active_user_pref_service_ = pref_service;
  }

  // Overridden from ShellDelegate:
  ::service_manager::Connector* GetShellConnector() const override;
  bool IsIncognitoAllowed() const override;
  bool IsMultiProfilesEnabled() const override;
  bool IsRunningInForcedAppMode() const override;
  bool CanShowWindowForUser(aura::Window* window) const override;
  bool IsForceMaximizeOnFirstRun() const override;
  void PreInit() override;
  void PreShutdown() override;
  void Exit() override;
  keyboard::KeyboardUI* CreateKeyboardUI() override;
  void ShelfInit() override;
  void ShelfShutdown() override;
  void OpenUrlFromArc(const GURL& url) override;
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
  void SuspendMediaSessions() override;
#if defined(USE_OZONE)
  ui::InputDeviceControllerClient* GetInputDeviceControllerClient() override;
#endif

  int num_exit_requests() const { return num_exit_requests_; }

  void SetForceMaximizeOnFirstRun(bool maximize) {
    force_maximize_on_first_run_ = maximize;
  }

  bool media_sessions_suspended() const { return media_sessions_suspended_; }

 private:
  int num_exit_requests_ = 0;
  bool multi_profiles_enabled_ = false;
  bool force_maximize_on_first_run_ = false;
  bool touchscreen_enabled_in_local_pref_ = true;
  bool media_sessions_suspended_ = false;
  std::unique_ptr<ShelfInitializer> shelf_initializer_;
  PrefService* active_user_pref_service_ = nullptr;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(TestShellDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_SHELL_DELEGATE_H_
