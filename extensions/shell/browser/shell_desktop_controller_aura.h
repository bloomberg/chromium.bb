// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_DESKTOP_CONTROLLER_AURA_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_DESKTOP_CONTROLLER_AURA_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "extensions/shell/browser/desktop_controller.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/base/ime/input_method_delegate.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/power_manager_client.h"
#include "ui/display/manager/chromeos/display_configurator.h"
#endif

namespace aura {
class Window;
class WindowTreeHost;
namespace client {
class DefaultCaptureClient;
class FocusClient;
}
}

namespace content {
class BrowserContext;
}

namespace gfx {
class Size;
}

namespace ui {
class UserActivityDetector;
#if defined(OS_CHROMEOS)
class UserActivityPowerManagerNotifier;
#endif
}

namespace wm {
class CompoundEventFilter;
class CursorManager;
}

namespace extensions {
class AppWindowClient;
class Extension;
class ShellScreen;

// Simple desktop controller for app_shell. Sets up a root Aura window for the
// primary display.
class ShellDesktopControllerAura
    : public DesktopController,
      public aura::client::WindowParentingClient,
#if defined(OS_CHROMEOS)
      public chromeos::PowerManagerClient::Observer,
      public display::DisplayConfigurator::Observer,
#endif
      public aura::WindowTreeHostObserver,
      public ui::internal::InputMethodDelegate {
 public:
  ShellDesktopControllerAura();
  ~ShellDesktopControllerAura() override;

  // DesktopController:
  gfx::Size GetWindowSize() override;
  AppWindow* CreateAppWindow(content::BrowserContext* context,
                             const Extension* extension) override;
  void AddAppWindow(gfx::NativeWindow window) override;
  void RemoveAppWindow(AppWindow* window) override;
  void CloseAppWindows() override;

  // aura::client::WindowParentingClient overrides:
  aura::Window* GetDefaultParent(aura::Window* window,
                                 const gfx::Rect& bounds) override;

#if defined(OS_CHROMEOS)
  // chromeos::PowerManagerClient::Observer overrides:
  void PowerButtonEventReceived(bool down,
                                const base::TimeTicks& timestamp) override;

  // display::DisplayConfigurator::Observer overrides.
  void OnDisplayModeChanged(
      const display::DisplayConfigurator::DisplayStateList& displays) override;
#endif

  // aura::WindowTreeHostObserver overrides:
  void OnHostCloseRequested(const aura::WindowTreeHost* host) override;

  // ui::internal::InputMethodDelegate overrides:
  ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* key_event) override;

 protected:
  // Creates and sets the aura clients and window manager stuff. Subclass may
  // initialize different sets of the clients.
  virtual void InitWindowManager();

 private:
  FRIEND_TEST_ALL_PREFIXES(ShellDesktopControllerAuraTest, InputEvents);

  // Creates the window that hosts the app.
  void CreateRootWindow();

  // Closes and destroys the root window hosting the app.
  void DestroyRootWindow();

  // Returns the dimensions (in pixels) of the primary display, or an empty size
  // if the dimensions can't be determined or no display is connected.
  gfx::Size GetPrimaryDisplaySize();

#if defined(OS_CHROMEOS)
  std::unique_ptr<display::DisplayConfigurator> display_configurator_;
#endif

  std::unique_ptr<ShellScreen> screen_;

  std::unique_ptr<aura::WindowTreeHost> host_;

  std::unique_ptr<wm::CompoundEventFilter> root_window_event_filter_;

  std::unique_ptr<aura::client::DefaultCaptureClient> capture_client_;

  std::unique_ptr<aura::client::FocusClient> focus_client_;

  std::unique_ptr<wm::CursorManager> cursor_manager_;

  std::unique_ptr<ui::UserActivityDetector> user_activity_detector_;
#if defined(OS_CHROMEOS)
  std::unique_ptr<ui::UserActivityPowerManagerNotifier> user_activity_notifier_;
#endif

  std::unique_ptr<AppWindowClient> app_window_client_;

  // NativeAppWindow::Close() deletes the AppWindow.
  std::vector<AppWindow*> app_windows_;

  DISALLOW_COPY_AND_ASSIGN(ShellDesktopControllerAura);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_DESKTOP_CONTROLLER_AURA_H_
