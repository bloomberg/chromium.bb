// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_SHELL_DELEGATE_IMPL_H_
#define ASH_SHELL_SHELL_DELEGATE_IMPL_H_

#include <string>

#include "ash/shell_delegate.h"
#include "base/compiler_specific.h"

namespace content {
class BrowserContext;
}

namespace keyboard {
class KeyboardControllerProxy;
}

namespace ash {
namespace shell {

class ShelfDelegateImpl;
class WindowWatcher;

class ShellDelegateImpl : public ash::ShellDelegate {
 public:
  ShellDelegateImpl();
  virtual ~ShellDelegateImpl();

  void SetWatcher(WindowWatcher* watcher);
  void set_browser_context(content::BrowserContext* browser_context) {
    browser_context_ = browser_context;
  }

  virtual bool IsFirstRunAfterBoot() const OVERRIDE;
  virtual bool IsIncognitoAllowed() const OVERRIDE;
  virtual bool IsMultiProfilesEnabled() const OVERRIDE;
  virtual bool IsRunningInForcedAppMode() const OVERRIDE;
  virtual bool IsMultiAccountEnabled() const OVERRIDE;
  virtual void PreInit() OVERRIDE;
  virtual void PreShutdown() OVERRIDE;
  virtual void Exit() OVERRIDE;
  virtual keyboard::KeyboardControllerProxy*
      CreateKeyboardControllerProxy() OVERRIDE;
  virtual void VirtualKeyboardActivated(bool activated) OVERRIDE;
  virtual void AddVirtualKeyboardStateObserver(
      VirtualKeyboardStateObserver* observer) OVERRIDE;
  virtual void RemoveVirtualKeyboardStateObserver(
      VirtualKeyboardStateObserver* observer) OVERRIDE;
  virtual content::BrowserContext* GetActiveBrowserContext() OVERRIDE;
  virtual app_list::AppListViewDelegate* CreateAppListViewDelegate() OVERRIDE;
  virtual ShelfDelegate* CreateShelfDelegate(ShelfModel* model) OVERRIDE;
  virtual ash::SystemTrayDelegate* CreateSystemTrayDelegate() OVERRIDE;
  virtual ash::UserWallpaperDelegate* CreateUserWallpaperDelegate() OVERRIDE;
  virtual ash::SessionStateDelegate* CreateSessionStateDelegate() OVERRIDE;
  virtual ash::AccessibilityDelegate* CreateAccessibilityDelegate() OVERRIDE;
  virtual ash::NewWindowDelegate* CreateNewWindowDelegate() OVERRIDE;
  virtual ash::MediaDelegate* CreateMediaDelegate() OVERRIDE;
  virtual ui::MenuModel* CreateContextMenu(
      aura::Window* root_window,
      ash::ShelfItemDelegate* item_delegate,
      ash::ShelfItem* item) OVERRIDE;
  virtual GPUSupport* CreateGPUSupport() OVERRIDE;
  virtual base::string16 GetProductName() const OVERRIDE;

 private:
  // Used to update Launcher. Owned by main.
  WindowWatcher* watcher_;

  ShelfDelegateImpl* shelf_delegate_;
  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(ShellDelegateImpl);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_SHELL_DELEGATE_IMPL_H_
