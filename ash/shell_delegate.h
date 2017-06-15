// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_DELEGATE_H_
#define ASH_SHELL_DELEGATE_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "base/callback.h"
#include "base/strings/string16.h"

class GURL;
class PrefService;

namespace aura {
class Window;
}

namespace gfx {
class Image;
}

namespace keyboard {
class KeyboardUI;
}

namespace service_manager {
class Connector;
}

namespace ui {
class MenuModel;
}

namespace ash {

class AccessibilityDelegate;
class GPUSupport;
class PaletteDelegate;
class Shelf;
struct ShelfItem;
class SystemTrayDelegate;
class WallpaperDelegate;

// Delegate of the Shell.
class ASH_EXPORT ShellDelegate {
 public:
  // The Shell owns the delegate.
  virtual ~ShellDelegate() {}

  // Returns the connector for the mojo service manager. Returns null in tests.
  virtual service_manager::Connector* GetShellConnector() const = 0;

  // Returns true if multi-profiles feature is enabled.
  virtual bool IsMultiProfilesEnabled() const = 0;

  // Returns true if incognito mode is allowed for the user.
  // Incognito windows are restricted for supervised users.
  virtual bool IsIncognitoAllowed() const = 0;

  // Returns true if we're running in forced app mode.
  virtual bool IsRunningInForcedAppMode() const = 0;

  // Returns true if |window| can be shown for the delegate's concept of current
  // user.
  virtual bool CanShowWindowForUser(aura::Window* window) const = 0;

  // Returns true if the first window shown on first run should be
  // unconditionally maximized, overriding the heuristic that normally chooses
  // the window size.
  virtual bool IsForceMaximizeOnFirstRun() const = 0;

  // Called before processing |Shell::Init()| so that the delegate
  // can perform tasks necessary before the shell is initialized.
  virtual void PreInit() = 0;

  // Called at the beginninig of Shell destructor so that
  // delegate can use Shell instance to perform cleanup tasks.
  virtual void PreShutdown() = 0;

  // Invoked when the user uses Ctrl-Shift-Q to close chrome.
  virtual void Exit() = 0;

  // Create a shell-specific keyboard::KeyboardUI
  virtual keyboard::KeyboardUI* CreateKeyboardUI() = 0;

  // Opens the |url| in a new browser tab.
  virtual void OpenUrlFromArc(const GURL& url) = 0;

  // Functions called when the shelf is initialized and shut down.
  // TODO(msw): Refine ChromeLauncherController lifetime management.
  virtual void ShelfInit() = 0;
  virtual void ShelfShutdown() = 0;

  // Creates a system-tray delegate. Shell takes ownership of the delegate.
  virtual SystemTrayDelegate* CreateSystemTrayDelegate() = 0;

  // Creates a wallpaper delegate. Shell takes ownership of the delegate.
  virtual std::unique_ptr<WallpaperDelegate> CreateWallpaperDelegate() = 0;

  // Creates a accessibility delegate. Shell takes ownership of the delegate.
  virtual AccessibilityDelegate* CreateAccessibilityDelegate() = 0;

  virtual std::unique_ptr<PaletteDelegate> CreatePaletteDelegate() = 0;

  // Creates a menu model for the |shelf| and optional shelf |item|.
  // If |item| is null, this creates a context menu for the wallpaper or shelf.
  virtual ui::MenuModel* CreateContextMenu(Shelf* shelf,
                                           const ShelfItem* item) = 0;

  // Creates a GPU support object. Shell takes ownership of the object.
  virtual GPUSupport* CreateGPUSupport() = 0;

  // Get the product name.
  virtual base::string16 GetProductName() const = 0;

  virtual void OpenKeyboardShortcutHelpPage() const {}

  virtual gfx::Image GetDeprecatedAcceleratorImage() const = 0;

  virtual PrefService* GetActiveUserPrefService() const = 0;

  // If |use_local_state| is true, returns the touchscreen status from local
  // state, otherwise from user prefs.
  virtual bool IsTouchscreenEnabledInPrefs(bool use_local_state) const = 0;

  // Sets the status of touchscreen to |enabled| in prefs. If |use_local_state|,
  // pref is set in local state, otherwise in user prefs.
  virtual void SetTouchscreenEnabledInPrefs(bool enabled,
                                            bool use_local_state) = 0;

  // Updates the enabled/disabled status of the touchscreen from prefs. Enabled
  // if both local state and user prefs are enabled, otherwise disabled.
  virtual void UpdateTouchscreenStatusFromPrefs() = 0;

  // Toggles the status of touchpad between enabled and disabled.
  virtual void ToggleTouchpad() {}

  // Suspends all WebContents-associated media sessions to stop managed players.
  virtual void SuspendMediaSessions() {}
};

}  // namespace ash

#endif  // ASH_SHELL_DELEGATE_H_
