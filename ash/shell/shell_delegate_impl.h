// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_SHELL_DELEGATE_IMPL_H_
#define ASH_SHELL_SHELL_DELEGATE_IMPL_H_
#pragma once

#include "ash/shell_delegate.h"
#include "base/compiler_specific.h"

namespace ash {
namespace shell {

class LauncherDelegateImpl;
class WindowWatcher;

class ShellDelegateImpl : public ash::ShellDelegate {
 public:
  ShellDelegateImpl();
  virtual ~ShellDelegateImpl();

  void SetWatcher(WindowWatcher* watcher);

  virtual bool IsUserLoggedIn() OVERRIDE;
  virtual void LockScreen() OVERRIDE;
  virtual void UnlockScreen() OVERRIDE;
  virtual bool IsScreenLocked() const OVERRIDE;
  virtual void Shutdown() OVERRIDE;
  virtual void Exit() OVERRIDE;
  virtual void NewWindow(bool incognito) OVERRIDE;
  virtual void Search() OVERRIDE;
  virtual void OpenFileManager() OVERRIDE;
  virtual void OpenCrosh() OVERRIDE;
  virtual void OpenMobileSetup() OVERRIDE;
  virtual content::BrowserContext* GetCurrentBrowserContext() OVERRIDE;
  virtual void ToggleSpokenFeedback() OVERRIDE;
  virtual ash::AppListViewDelegate* CreateAppListViewDelegate() OVERRIDE;
  virtual void StartPartialScreenshot(
      ash::ScreenshotDelegate* screenshot_delegate) OVERRIDE;
  virtual ash::LauncherDelegate* CreateLauncherDelegate(
      ash::LauncherModel* model) OVERRIDE;
  virtual ash::SystemTrayDelegate* CreateSystemTrayDelegate(
      ash::SystemTray* tray) OVERRIDE;
  virtual ash::UserWallpaperDelegate* CreateUserWallpaperDelegate() OVERRIDE;

 private:
  // Used to update Launcher. Owned by main.
  WindowWatcher* watcher_;

  LauncherDelegateImpl* launcher_delegate_;

  bool locked_;

  DISALLOW_COPY_AND_ASSIGN(ShellDelegateImpl);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_SHELL_DELEGATE_IMPL_H_
