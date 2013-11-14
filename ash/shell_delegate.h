// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_DELEGATE_H_
#define ASH_SHELL_DELEGATE_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/shell.h"
#include "base/callback.h"
#include "base/strings/string16.h"

namespace app_list {
class AppListViewDelegate;
}

namespace aura {
class RootWindow;
class Window;
namespace client {
class UserActionClient;
}
}

namespace content {
class BrowserContext;
}

namespace ui {
class MenuModel;
}

namespace views {
class Widget;
}

namespace keyboard {
class KeyboardControllerProxy;
}

namespace ash {

class AccessibilityDelegate;
class CapsLockDelegate;
class LauncherDelegate;
struct LauncherItem;
class MediaDelegate;
class NewWindowDelegate;
class RootWindowHostFactory;
class SessionStateDelegate;
class ShelfModel;
class SystemTrayDelegate;
class UserWallpaperDelegate;

enum UserMetricsAction {
  UMA_ACCEL_KEYBOARD_BRIGHTNESS_DOWN_F6,
  UMA_ACCEL_KEYBOARD_BRIGHTNESS_UP_F7,
  UMA_ACCEL_LOCK_SCREEN_L,
  UMA_ACCEL_LOCK_SCREEN_LOCK_BUTTON,
  UMA_ACCEL_LOCK_SCREEN_POWER_BUTTON,
  UMA_ACCEL_FULLSCREEN_F4,
  UMA_ACCEL_MAXIMIZE_RESTORE_F4,
  UMA_ACCEL_NEWTAB_T,
  UMA_ACCEL_NEXTWINDOW_F5,
  UMA_ACCEL_NEXTWINDOW_TAB,
  UMA_ACCEL_OVERVIEW_F5,
  UMA_ACCEL_PREVWINDOW_F5,
  UMA_ACCEL_PREVWINDOW_TAB,
  UMA_ACCEL_EXIT_FIRST_Q,
  UMA_ACCEL_EXIT_SECOND_Q,
  UMA_ACCEL_SEARCH_LWIN,
  UMA_ACCEL_SHUT_DOWN_POWER_BUTTON,
  UMA_CLOSE_THROUGH_CONTEXT_MENU,
  UMA_GESTURE_OVERVIEW,
  UMA_LAUNCHER_CLICK_ON_APP,
  UMA_LAUNCHER_CLICK_ON_APPLIST_BUTTON,
  UMA_MINIMIZE_PER_KEY,
  UMA_MOUSE_DOWN,
  UMA_TOGGLE_MAXIMIZE_CAPTION_CLICK,
  UMA_TOGGLE_MAXIMIZE_CAPTION_GESTURE,
  UMA_TOUCHPAD_GESTURE_OVERVIEW,
  UMA_TOUCHSCREEN_TAP_DOWN,
  UMA_TRAY_HELP,
  UMA_TRAY_LOCK_SCREEN,
  UMA_TRAY_SHUT_DOWN,
  UMA_WINDOW_APP_CLOSE_BUTTON_CLICK,
  UMA_WINDOW_CLOSE_BUTTON_CLICK,
  UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_EXIT_FULLSCREEN,
  UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_MAXIMIZE,
  UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_MINIMIZE,
  UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_RESTORE,
  UMA_WINDOW_MAXIMIZE_BUTTON_MAXIMIZE,
  UMA_WINDOW_MAXIMIZE_BUTTON_MAXIMIZE_LEFT,
  UMA_WINDOW_MAXIMIZE_BUTTON_MAXIMIZE_RIGHT,
  UMA_WINDOW_MAXIMIZE_BUTTON_MINIMIZE,
  UMA_WINDOW_MAXIMIZE_BUTTON_RESTORE,
  UMA_WINDOW_MAXIMIZE_BUTTON_SHOW_BUBBLE,

  // Thumbnail sized overview of windows triggered. This is a subset of
  // UMA_WINDOW_SELECTION triggered by lingering during alt+tab cycles or
  // pressing the overview key.
  UMA_WINDOW_OVERVIEW,

  // Window selection started by beginning an alt+tab cycle or pressing the
  // overview key. This does not count each step through an alt+tab cycle.
  UMA_WINDOW_SELECTION,
};

// Delegate of the Shell.
class ASH_EXPORT ShellDelegate {
 public:
  // The Shell owns the delegate.
  virtual ~ShellDelegate() {}

  // Returns true if this is the first time that the shell has been run after
  // the system has booted.  false is returned after the shell has been
  // restarted, typically due to logging in as a guest or logging out.
  virtual bool IsFirstRunAfterBoot() const = 0;

  // Returns true if multi-profiles feature is enabled.
  virtual bool IsMultiProfilesEnabled() const = 0;

  // Returns true if incognito mode is allowed for the user.
  // Incognito windows are restricted for supervised users.
  virtual bool IsIncognitoAllowed() const = 0;

  // Returns true if we're running in forced app mode.
  virtual bool IsRunningInForcedAppMode() const = 0;

  // Called before processing |Shell::Init()| so that the delegate
  // can perform tasks necessary before the shell is initialized.
  virtual void PreInit() = 0;

  // Shuts down the environment.
  virtual void Shutdown() = 0;

  // Invoked when the user uses Ctrl-Shift-Q to close chrome.
  virtual void Exit() = 0;

  // Create a shell-specific keyboard::KeyboardControllerProxy
  virtual keyboard::KeyboardControllerProxy*
      CreateKeyboardControllerProxy() = 0;

  // Get the current browser context. This will get us the current profile.
  virtual content::BrowserContext* GetCurrentBrowserContext() = 0;

  // Invoked to create an AppListViewDelegate. Shell takes the ownership of
  // the created delegate.
  virtual app_list::AppListViewDelegate* CreateAppListViewDelegate() = 0;

  // Creates a new LauncherDelegate. Shell takes ownership of the returned
  // value.
  virtual LauncherDelegate* CreateLauncherDelegate(ShelfModel* model) = 0;

  // Creates a system-tray delegate. Shell takes ownership of the delegate.
  virtual SystemTrayDelegate* CreateSystemTrayDelegate() = 0;

  // Creates a user wallpaper delegate. Shell takes ownership of the delegate.
  virtual UserWallpaperDelegate* CreateUserWallpaperDelegate() = 0;

  // Creates a caps lock delegate. Shell takes ownership of the delegate.
  virtual CapsLockDelegate* CreateCapsLockDelegate() = 0;

  // Creates a session state delegate. Shell takes ownership of the delegate.
  virtual SessionStateDelegate* CreateSessionStateDelegate() = 0;

  // Creates a accessibility delegate. Shell takes ownership of the delegate.
  virtual AccessibilityDelegate* CreateAccessibilityDelegate() = 0;

  // Creates an application delegate. Shell takes ownership of the delegate.
  virtual NewWindowDelegate* CreateNewWindowDelegate() = 0;

  // Creates a media delegate. Shell takes ownership of the delegate.
  virtual MediaDelegate* CreateMediaDelegate() = 0;

  // Creates a user action client. Shell takes ownership of the object.
  virtual aura::client::UserActionClient* CreateUserActionClient() = 0;

  // Records that the user performed an action.
  virtual void RecordUserMetricsAction(UserMetricsAction action) = 0;

  // Creates a menu model of the context for the |root_window|.
  virtual ui::MenuModel* CreateContextMenu(aura::Window* root_window) = 0;

  // Creates a root window host factory. Shell takes ownership of the returned
  // value.
  virtual RootWindowHostFactory* CreateRootWindowHostFactory() = 0;

  // Get the product name.
  virtual base::string16 GetProductName() const = 0;
};

}  // namespace ash

#endif  // ASH_SHELL_DELEGATE_H_
