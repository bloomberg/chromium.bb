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

  virtual bool IsFirstRunAfterBoot() const override;
  virtual bool IsIncognitoAllowed() const override;
  virtual bool IsMultiProfilesEnabled() const override;
  virtual bool IsRunningInForcedAppMode() const override;
  virtual bool IsMultiAccountEnabled() const override;
  virtual void PreInit() override;
  virtual void PreShutdown() override;
  virtual void Exit() override;
  virtual keyboard::KeyboardControllerProxy*
      CreateKeyboardControllerProxy() override;
  virtual void VirtualKeyboardActivated(bool activated) override;
  virtual void AddVirtualKeyboardStateObserver(
      VirtualKeyboardStateObserver* observer) override;
  virtual void RemoveVirtualKeyboardStateObserver(
      VirtualKeyboardStateObserver* observer) override;
  virtual content::BrowserContext* GetActiveBrowserContext() override;
  virtual app_list::AppListViewDelegate* GetAppListViewDelegate() override;
  virtual ShelfDelegate* CreateShelfDelegate(ShelfModel* model) override;
  virtual ash::SystemTrayDelegate* CreateSystemTrayDelegate() override;
  virtual ash::UserWallpaperDelegate* CreateUserWallpaperDelegate() override;
  virtual ash::SessionStateDelegate* CreateSessionStateDelegate() override;
  virtual ash::AccessibilityDelegate* CreateAccessibilityDelegate() override;
  virtual ash::NewWindowDelegate* CreateNewWindowDelegate() override;
  virtual ash::MediaDelegate* CreateMediaDelegate() override;
  virtual ui::MenuModel* CreateContextMenu(
      aura::Window* root_window,
      ash::ShelfItemDelegate* item_delegate,
      ash::ShelfItem* item) override;
  virtual GPUSupport* CreateGPUSupport() override;
  virtual base::string16 GetProductName() const override;

 private:
  // Used to update Launcher. Owned by main.
  WindowWatcher* watcher_;

  ShelfDelegateImpl* shelf_delegate_;
  content::BrowserContext* browser_context_;
  scoped_ptr<app_list::AppListViewDelegate> app_list_view_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ShellDelegateImpl);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_SHELL_DELEGATE_IMPL_H_
