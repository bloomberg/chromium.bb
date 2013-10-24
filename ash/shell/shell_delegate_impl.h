// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_SHELL_DELEGATE_IMPL_H_
#define ASH_SHELL_SHELL_DELEGATE_IMPL_H_

#include <string>

#include "ash/shell_delegate.h"
#include "base/compiler_specific.h"

namespace keyboard {
class KeyboardControllerProxy;
}

namespace ash {
namespace shell {

class LauncherDelegateImpl;
class WindowWatcher;

class ShellDelegateImpl : public ash::ShellDelegate {
 public:
  ShellDelegateImpl();
  virtual ~ShellDelegateImpl();

  void SetWatcher(WindowWatcher* watcher);

  virtual bool IsFirstRunAfterBoot() const OVERRIDE;
  virtual bool IsMultiProfilesEnabled() const OVERRIDE;
  virtual bool IsRunningInForcedAppMode() const OVERRIDE;
  virtual void PreInit() OVERRIDE;
  virtual void Shutdown() OVERRIDE;
  virtual void Exit() OVERRIDE;
  virtual void NewTab() OVERRIDE;
  virtual void NewWindow(bool incognito) OVERRIDE;
  virtual void ToggleFullscreen() OVERRIDE;
  virtual void OpenFileManager() OVERRIDE;
  virtual void OpenCrosh() OVERRIDE;
  virtual void RestoreTab() OVERRIDE;
  virtual void ShowKeyboardOverlay() OVERRIDE;
  virtual keyboard::KeyboardControllerProxy*
      CreateKeyboardControllerProxy() OVERRIDE;
  virtual void ShowTaskManager() OVERRIDE;
  virtual content::BrowserContext* GetCurrentBrowserContext() OVERRIDE;
  virtual app_list::AppListViewDelegate* CreateAppListViewDelegate() OVERRIDE;
  virtual ash::LauncherDelegate* CreateLauncherDelegate(
      ash::LauncherModel* model) OVERRIDE;
  virtual ash::SystemTrayDelegate* CreateSystemTrayDelegate() OVERRIDE;
  virtual ash::UserWallpaperDelegate* CreateUserWallpaperDelegate() OVERRIDE;
  virtual ash::CapsLockDelegate* CreateCapsLockDelegate() OVERRIDE;
  virtual ash::SessionStateDelegate* CreateSessionStateDelegate() OVERRIDE;
  virtual ash::AccessibilityDelegate* CreateAccessibilityDelegate() OVERRIDE;
  virtual aura::client::UserActionClient* CreateUserActionClient() OVERRIDE;
  virtual void OpenFeedbackPage() OVERRIDE;
  virtual void RecordUserMetricsAction(UserMetricsAction action) OVERRIDE;
  virtual void HandleMediaNextTrack() OVERRIDE;
  virtual void HandleMediaPlayPause() OVERRIDE;
  virtual void HandleMediaPrevTrack() OVERRIDE;
  virtual ui::MenuModel* CreateContextMenu(
      aura::RootWindow* root_window) OVERRIDE;
  virtual RootWindowHostFactory* CreateRootWindowHostFactory() OVERRIDE;
  virtual base::string16 GetProductName() const OVERRIDE;

 private:
  // Used to update Launcher. Owned by main.
  WindowWatcher* watcher_;

  LauncherDelegateImpl* launcher_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ShellDelegateImpl);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_SHELL_DELEGATE_IMPL_H_
