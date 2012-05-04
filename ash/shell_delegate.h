// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_DELEGATE_H_
#define ASH_SHELL_DELEGATE_H_
#pragma once

#include <vector>

#include "ash/ash_export.h"
#include "ash/shell.h"
#include "base/callback.h"
#include "base/string16.h"

namespace aura {
class Window;
}

namespace views {
class Widget;
}

namespace ash {

class AppListViewDelegate;
class LauncherDelegate;
class LauncherModel;
struct LauncherItem;
class ScreenshotDelegate;
class SystemTray;
class SystemTrayDelegate;
class UserWallpaperDelegate;

// Delegate of the Shell.
class ASH_EXPORT ShellDelegate {
 public:
  // The Shell owns the delegate.
  virtual ~ShellDelegate() {}

  // Returns true if user has logged in.
  virtual bool IsUserLoggedIn() = 0;

  // Invoked when a user locks the screen.
  virtual void LockScreen() = 0;

  // Unlock the screen. Currently used only for tests.
  virtual void UnlockScreen() = 0;

  // Returns true if the screen is currently locked.
  virtual bool IsScreenLocked() const = 0;

  // Shuts down the environment.
  virtual void Shutdown() = 0;

  // Invoked when the user uses Ctrl-Shift-Q to close chrome.
  virtual void Exit() = 0;

  // Invoked when the user uses Ctrl-N or Ctrl-Shift-N to open a new window.
  virtual void NewWindow(bool incognito) = 0;

  // Invoked when the user presses the Search key.
  virtual void Search() = 0;

  // Invoked when the user uses Ctrl-M to open file manager.
  virtual void OpenFileManager() = 0;

  // Invoked when the user opens Crosh.
  virtual void OpenCrosh() = 0;

  // Invoked when the user needs to set up mobile networking.
  virtual void OpenMobileSetup() = 0;

  // Get the current browser context. This will get us the current profile.
  virtual content::BrowserContext* GetCurrentBrowserContext() = 0;

  // Invoked when the user presses a shortcut to toggle spoken feedback
  // for accessibility.
  virtual void ToggleSpokenFeedback() = 0;

  // Invoked to create an AppListViewDelegate. Shell takes the ownership of
  // the created delegate.
  virtual AppListViewDelegate* CreateAppListViewDelegate() = 0;

  // Invoked to start taking partial screenshot.
  virtual void StartPartialScreenshot(
      ScreenshotDelegate* screenshot_delegate) = 0;

  // Creates a new LauncherDelegate. Shell takes ownership of the returned
  // value.
  virtual LauncherDelegate* CreateLauncherDelegate(
      ash::LauncherModel* model) = 0;

  // Creates a system-tray delegate. Shell takes ownership of the delegate.
  virtual SystemTrayDelegate* CreateSystemTrayDelegate(SystemTray* tray) = 0;

  // Creates a user wallpaper delegate. Shell takes ownership of the delegate.
  virtual UserWallpaperDelegate* CreateUserWallpaperDelegate() = 0;
};

}  // namespace ash

#endif  // ASH_SHELL_DELEGATE_H_
