// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_SHELL_DELEGATE_IMPL_H_
#define ASH_SHELL_SHELL_DELEGATE_IMPL_H_

#include <string>

#include "ash/shell_delegate.h"
#include "base/compiler_specific.h"

namespace keyboard {
class KeyboardUI;
}

namespace ash {
namespace shell {

class ShelfDelegateImpl;
class WindowWatcher;

class ShellDelegateImpl : public ash::ShellDelegate {
 public:
  ShellDelegateImpl();
  ~ShellDelegateImpl() override;

  void SetWatcher(WindowWatcher* watcher);

  bool IsFirstRunAfterBoot() const override;
  bool IsIncognitoAllowed() const override;
  bool IsMultiProfilesEnabled() const override;
  bool IsRunningInForcedAppMode() const override;
  bool IsMultiAccountEnabled() const override;
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
  ash::SystemTrayDelegate* CreateSystemTrayDelegate() override;
  ash::UserWallpaperDelegate* CreateUserWallpaperDelegate() override;
  ash::SessionStateDelegate* CreateSessionStateDelegate() override;
  ash::AccessibilityDelegate* CreateAccessibilityDelegate() override;
  ash::NewWindowDelegate* CreateNewWindowDelegate() override;
  ash::MediaDelegate* CreateMediaDelegate() override;
  ui::MenuModel* CreateContextMenu(aura::Window* root_window,
                                   ash::ShelfItemDelegate* item_delegate,
                                   ash::ShelfItem* item) override;
  GPUSupport* CreateGPUSupport() override;
  base::string16 GetProductName() const override;
  gfx::Image GetDeprecatedAcceleratorImage() const override;

 private:
  // Used to update Launcher. Owned by main.
  WindowWatcher* watcher_;

  ShelfDelegateImpl* shelf_delegate_;
  scoped_ptr<app_list::AppListViewDelegate> app_list_view_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ShellDelegateImpl);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_SHELL_DELEGATE_IMPL_H_
