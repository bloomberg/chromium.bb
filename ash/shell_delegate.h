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
  // Source requesting the window list.
  enum CycleSource {
    // Windows are going to be used for alt-tab (or F5).
    SOURCE_KEYBOARD,

    // Windows are going to be cycled from the launcher.
    SOURCE_LAUNCHER,
  };

  // The Shell owns the delegate.
  virtual ~ShellDelegate() {}

  // Invoked to create a new status area. Can return NULL.
  virtual views::Widget* CreateStatusArea() = 0;

  // Can we create the launcher yet?  In some cases, we may need to defer it
  // until e.g. a user has logged in and their profile has been loaded.
  virtual bool CanCreateLauncher() = 0;

  // Invoked when a user locks the screen.
  virtual void LockScreen() = 0;

  // Unlock the screen. Currently used only for tests.
  virtual void UnlockScreen() = 0;

  // Returns true if the screen is currently locked.
  virtual bool IsScreenLocked() const = 0;

  // Invoked when a user uses Ctrl-Shift-Q to close chrome.
  virtual void Exit() = 0;

  // Invoked to create an AppListViewDelegate. Shell takes the ownership of
  // the created delegate.
  virtual AppListViewDelegate* CreateAppListViewDelegate() = 0;

  // Returns a list of windows to cycle with keyboard shortcuts (e.g. alt-tab
  // or the window switching key).  If |order_by_activity| is true then windows
  // are returned in most-recently-used order with the currently active window
  // at the front of the list.  Otherwise any order may be returned.  The list
  // does not contain NULL pointers.
  virtual std::vector<aura::Window*> GetCycleWindowList(
      CycleSource source) const = 0;

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
