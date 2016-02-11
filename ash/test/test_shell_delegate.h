// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_SHELL_DELEGATE_H_
#define ASH_TEST_TEST_SHELL_DELEGATE_H_

#include <string>

#include "ash/media_delegate.h"
#include "ash/shell_delegate.h"
#include "ash/test/test_session_state_delegate.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"

namespace keyboard {
class KeyboardUI;
}

namespace ash {
namespace test {

class TestShellDelegate : public ShellDelegate {
 public:
  TestShellDelegate();
  ~TestShellDelegate() override;

  void set_multi_profiles_enabled(bool multi_profiles_enabled) {
    multi_profiles_enabled_ = multi_profiles_enabled;
  }

  // Overridden from ShellDelegate:
  bool IsFirstRunAfterBoot() const override;
  bool IsIncognitoAllowed() const override;
  bool IsMultiProfilesEnabled() const override;
  bool IsRunningInForcedAppMode() const override;
  bool CanShowWindowForUser(aura::Window* window) const override;
  bool IsForceMaximizeOnFirstRun() const override;
  void PreInit() override;
  void PreShutdown() override;
  void Exit() override;
  keyboard::KeyboardUI* CreateKeyboardUI() override;
  void VirtualKeyboardActivated(bool activated) override;
  void AddVirtualKeyboardStateObserver(
      VirtualKeyboardStateObserver* observer) override;
  void RemoveVirtualKeyboardStateObserver(
      VirtualKeyboardStateObserver* observer) override;
  app_list::AppListViewDelegate* GetAppListViewDelegate() override;
  ShelfDelegate* CreateShelfDelegate(ShelfModel* model) override;
  SystemTrayDelegate* CreateSystemTrayDelegate() override;
  UserWallpaperDelegate* CreateUserWallpaperDelegate() override;
  TestSessionStateDelegate* CreateSessionStateDelegate() override;
  AccessibilityDelegate* CreateAccessibilityDelegate() override;
  NewWindowDelegate* CreateNewWindowDelegate() override;
  MediaDelegate* CreateMediaDelegate() override;
  ui::MenuModel* CreateContextMenu(aura::Window* root,
                                   ash::ShelfItemDelegate* item_delegate,
                                   ash::ShelfItem* item) override;
  GPUSupport* CreateGPUSupport() override;
  base::string16 GetProductName() const override;
  gfx::Image GetDeprecatedAcceleratorImage() const override;

  int num_exit_requests() const { return num_exit_requests_; }

  void SetMediaCaptureState(MediaCaptureState state);
  void SetForceMaximizeOnFirstRun(bool maximize) {
    force_maximize_on_first_run_ = maximize;
  }

 private:
  int num_exit_requests_;
  bool multi_profiles_enabled_;
  bool force_maximize_on_first_run_;

  scoped_ptr<app_list::AppListViewDelegate> app_list_view_delegate_;

  base::ObserverList<ash::VirtualKeyboardStateObserver>
      keyboard_state_observer_list_;

  DISALLOW_COPY_AND_ASSIGN(TestShellDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_SHELL_DELEGATE_H_
