// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_SHELL_DELEGATE_H_
#define ASH_TEST_TEST_SHELL_DELEGATE_H_
#pragma once

#include "ash/shell_delegate.h"
#include "base/compiler_specific.h"

namespace ash {
namespace test {

class TestShellDelegate : public ShellDelegate {
 public:
  TestShellDelegate();
  virtual ~TestShellDelegate();

  // Overridden from ShellDelegate:
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
  virtual void ToggleSpokenFeedback() OVERRIDE;
  virtual AppListViewDelegate* CreateAppListViewDelegate() OVERRIDE;
  virtual void StartPartialScreenshot(
      ScreenshotDelegate* screenshot_delegate) OVERRIDE;
  virtual LauncherDelegate* CreateLauncherDelegate(
      ash::LauncherModel* model) OVERRIDE;
  virtual SystemTrayDelegate* CreateSystemTrayDelegate(SystemTray* t) OVERRIDE;
  virtual UserWallpaperDelegate* CreateUserWallpaperDelegate() OVERRIDE;

 private:
  bool locked_;

  DISALLOW_COPY_AND_ASSIGN(TestShellDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_SHELL_DELEGATE_H_
