// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_DELEGATE_H_
#define ASH_SHELL_DELEGATE_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/magnifier/magnifier_constants.h"
#include "ash/shell.h"
#include "base/callback.h"
#include "base/string16.h"
#include "base/time.h"

namespace app_list {
class AppListViewDelegate;
}

namespace aura {
class RootWindow;
class Window;
namespace client {
class StackingClient;
class UserActionClient;
}
}

namespace ui {
class MenuModel;
}

namespace views {
class Widget;
}

namespace ash {

class CapsLockDelegate;
class LauncherDelegate;
class LauncherModel;
struct LauncherItem;
class RootWindowHostFactory;
class SystemTrayDelegate;
class UserWallpaperDelegate;

enum UserMetricsAction {
  UMA_ACCEL_KEYBOARD_BRIGHTNESS_DOWN_F6,
  UMA_ACCEL_KEYBOARD_BRIGHTNESS_UP_F7,
  UMA_ACCEL_MAXIMIZE_RESTORE_F4,
  UMA_ACCEL_NEWTAB_T,
  UMA_ACCEL_NEXTWINDOW_F5,
  UMA_ACCEL_NEXTWINDOW_TAB,
  UMA_ACCEL_PREVWINDOW_F5,
  UMA_ACCEL_PREVWINDOW_TAB,
  UMA_ACCEL_SEARCH_LWIN,
  UMA_LAUNCHER_CLICK_ON_APP,
  UMA_LAUNCHER_CLICK_ON_APPLIST_BUTTON,
  UMA_MOUSE_DOWN,
  UMA_TOUCHSCREEN_TAP_DOWN,
};

enum AccessibilityNotificationVisibility {
  A11Y_NOTIFICATION_NONE,
  A11Y_NOTIFICATION_SHOW,
};

// Delegate of the Shell.
class ASH_EXPORT ShellDelegate {
 public:
  // The Shell owns the delegate.
  virtual ~ShellDelegate() {}

  // Returns true if user has logged in.
  virtual bool IsUserLoggedIn() const = 0;

  // Returns true if we're logged in and browser has been started
  virtual bool IsSessionStarted() const = 0;

  // Returns true if this is the first time that the shell has been run after
  // the system has booted.  false is returned after the shell has been
  // restarted, typically due to logging in as a guest or logging out.
  virtual bool IsFirstRunAfterBoot() const = 0;

  // Returns true if a user is logged in whose session can be locked (i.e. the
  // user has a password with which to unlock the session).
  virtual bool CanLockScreen() const = 0;

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

  // Invoked when the user uses Ctrl+T to open a new tab.
  virtual void NewTab() = 0;

  // Invoked when the user uses Ctrl-N or Ctrl-Shift-N to open a new window.
  virtual void NewWindow(bool incognito) = 0;

  // Invoked when the user uses F4 to toggle window maximized state.
  virtual void ToggleMaximized() = 0;

  // Invoked when the user uses Ctrl-O to open file manager.
  virtual void OpenFileManager() = 0;

  // Invoked when the user opens Crosh.
  virtual void OpenCrosh() = 0;

  // Invoked when the user needs to set up mobile networking.
  virtual void OpenMobileSetup(const std::string& service_path) = 0;

  // Invoked when the user uses Shift+Ctrl+T to restore the closed tab.
  virtual void RestoreTab() = 0;

  // Moves keyboard focus to the next pane. Returns false if no browser window
  // is created.
  virtual bool RotatePaneFocus(Shell::Direction direction) = 0;

  // Shows the keyboard shortcut overlay.
  virtual void ShowKeyboardOverlay() = 0;

  // Shows the task manager window.
  virtual void ShowTaskManager() = 0;

  // Get the current browser context. This will get us the current profile.
  virtual content::BrowserContext* GetCurrentBrowserContext() = 0;

  // Invoked to toggle spoken feedback for accessibility
  virtual void ToggleSpokenFeedback(
      AccessibilityNotificationVisibility notify) = 0;

  // Returns true if spoken feedback is enabled.
  virtual bool IsSpokenFeedbackEnabled() const = 0;

  // Invoked to toggle high contrast for accessibility.
  virtual void ToggleHighContrast() = 0;

  // Returns true if high contrast mode is enabled.
  virtual bool IsHighContrastEnabled() const = 0;

  // Invoked to enable the screen magnifier.
  virtual void SetMagnifierEnabled(bool enabled) = 0;

  // Invoked to change the type of the screen magnifier.
  virtual void SetMagnifierType(MagnifierType type) = 0;

  // Returns if the screen magnifier is enabled or not.
  virtual bool IsMagnifierEnabled() const = 0;

  // Returns the current screen magnifier mode.
  virtual MagnifierType GetMagnifierType() const = 0;

  // Returns true if the user want to show accesibility menu even when all the
  // accessibility features are disabled.
  virtual bool ShouldAlwaysShowAccessibilityMenu() const = 0;

  // Invoked to create an AppListViewDelegate. Shell takes the ownership of
  // the created delegate.
  virtual app_list::AppListViewDelegate* CreateAppListViewDelegate() = 0;

  // Creates a new LauncherDelegate. Shell takes ownership of the returned
  // value.
  virtual LauncherDelegate* CreateLauncherDelegate(
      ash::LauncherModel* model) = 0;

  // Creates a system-tray delegate. Shell takes ownership of the delegate.
  virtual SystemTrayDelegate* CreateSystemTrayDelegate() = 0;

  // Creates a user wallpaper delegate. Shell takes ownership of the delegate.
  virtual UserWallpaperDelegate* CreateUserWallpaperDelegate() = 0;

  // Creates a caps lock delegate. Shell takes ownership of the delegate.
  virtual CapsLockDelegate* CreateCapsLockDelegate() = 0;

  // Creates a user action client. Shell takes ownership of the object.
  virtual aura::client::UserActionClient* CreateUserActionClient() = 0;

  // Opens the feedback page for "Report Issue".
  virtual void OpenFeedbackPage() = 0;

  // Records that the user performed an action.
  virtual void RecordUserMetricsAction(UserMetricsAction action) = 0;

  // Handles the Next Track Media shortcut key.
  virtual void HandleMediaNextTrack() = 0;

  // Handles the Play/Pause Toggle Media shortcut key.
  virtual void HandleMediaPlayPause() = 0;

  // Handles the Previous Track Media shortcut key.
  virtual void HandleMediaPrevTrack() = 0;

  // Produces l10n-ed text of remaining time, e.g.: "13 minutes left" or
  // "13 Minuten übrig".
  // Used, for example, to display the remaining battery life.
  virtual string16 GetTimeRemainingString(base::TimeDelta delta) = 0;

  // Produces l10n-ed text for time duration, e.g.: "13 minutes" or "2 hours".
  virtual string16 GetTimeDurationLongString(base::TimeDelta delta) = 0;

  // Saves the zoom scale of the full screen magnifier.
  virtual void SaveScreenMagnifierScale(double scale) = 0;

  // Gets a saved value of the zoom scale of full screen magnifier. If a value
  // is not saved, return a negative value.
  virtual double GetSavedScreenMagnifierScale() = 0;

  // Creates a menu model of the context for the |root_window|.
  virtual ui::MenuModel* CreateContextMenu(aura::RootWindow* root_window) = 0;

  // Creates the stacking client. Shell takes ownership of the object.
  virtual aura::client::StackingClient* CreateStackingClient() = 0;

  // Creates a root window host factory. Shell takes ownership of the returned
  // value.
  virtual RootWindowHostFactory* CreateRootWindowHostFactory() = 0;
};

}  // namespace ash

#endif  // ASH_SHELL_DELEGATE_H_
