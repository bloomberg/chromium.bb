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
class InputDeviceControllerClient;
}

namespace ash {

class AccessibilityDelegate;
class GPUSupport;
class NetworkingConfigDelegate;
class PaletteDelegate;
class WallpaperDelegate;
enum class TouchscreenEnabledSource;

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

  // Create a shell-specific keyboard::KeyboardUI.
  virtual std::unique_ptr<keyboard::KeyboardUI> CreateKeyboardUI() = 0;

  // Opens the |url| in a new browser tab.
  virtual void OpenUrlFromArc(const GURL& url) = 0;

  // Functions called when the shelf is initialized and shut down.
  // TODO(msw): Refine ChromeLauncherController lifetime management.
  virtual void ShelfInit() = 0;
  virtual void ShelfShutdown() = 0;

  // Returns the delegate. May be null in tests.
  virtual NetworkingConfigDelegate* GetNetworkingConfigDelegate() = 0;

  // Creates a wallpaper delegate. Shell takes ownership of the delegate.
  virtual std::unique_ptr<WallpaperDelegate> CreateWallpaperDelegate() = 0;

  // Creates a accessibility delegate. Shell takes ownership of the delegate.
  virtual AccessibilityDelegate* CreateAccessibilityDelegate() = 0;

  virtual std::unique_ptr<PaletteDelegate> CreatePaletteDelegate() = 0;

  // Creates a GPU support object. Shell takes ownership of the object.
  virtual GPUSupport* CreateGPUSupport() = 0;

  // Get the product name.
  virtual base::string16 GetProductName() const = 0;

  virtual void OpenKeyboardShortcutHelpPage() const {}

  virtual gfx::Image GetDeprecatedAcceleratorImage() const = 0;

  // Returns the current touchscreen enabled status as specified by |source|.
  // Note that the actual state of the touchscreen device is automatically
  // determined based on the requests of multiple sources.
  virtual bool GetTouchscreenEnabled(TouchscreenEnabledSource source) const = 0;

  // Sets |source|'s requested touchscreen enabled status to |enabled|. Note
  // that the actual state of the touchscreen device is automatically determined
  // based on the requests of multiple sources.
  virtual void SetTouchscreenEnabled(bool enabled,
                                     TouchscreenEnabledSource source) = 0;

  // Toggles the status of touchpad between enabled and disabled.
  virtual void ToggleTouchpad() {}

  // Suspends all WebContents-associated media sessions to stop managed players.
  virtual void SuspendMediaSessions() {}

  // Creator of Shell owns this; it's assumed this outlives Shell.
  virtual ui::InputDeviceControllerClient* GetInputDeviceControllerClient() = 0;
};

}  // namespace ash

#endif  // ASH_SHELL_DELEGATE_H_
