// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_DESKTOP_CONTROLLER_AURA_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_DESKTOP_CONTROLLER_AURA_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "extensions/shell/browser/desktop_controller.h"
#include "extensions/shell/browser/root_window_controller.h"
#include "ui/base/ime/input_method_delegate.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/power_manager_client.h"
#include "ui/display/manager/chromeos/display_configurator.h"
#endif

namespace aura {
class WindowTreeHost;
namespace client {
class DefaultCaptureClient;
}  // namespace client
}  // namespace aura

namespace base {
class RunLoop;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace gfx {
class Size;
}  // namespace gfx

namespace ui {
class InputMethod;
class UserActivityDetector;
#if defined(OS_CHROMEOS)
class UserActivityPowerManagerNotifier;
#endif
}  // namespace ui

namespace wm {
class CompoundEventFilter;
class CursorManager;
class FocusController;
}  // namespace wm

namespace extensions {
class AppWindowClient;
class ShellScreen;

// Simple desktop controller for app_shell. Sets up a root Aura window for the
// primary display.
class ShellDesktopControllerAura
    : public DesktopController,
      public RootWindowController::DesktopDelegate,
#if defined(OS_CHROMEOS)
      public chromeos::PowerManagerClient::Observer,
      public display::DisplayConfigurator::Observer,
#endif
      public ui::internal::InputMethodDelegate {
 public:
  explicit ShellDesktopControllerAura(content::BrowserContext* browser_context);
  ~ShellDesktopControllerAura() override;

  // DesktopController:
  void Run() override;
  AppWindow* CreateAppWindow(content::BrowserContext* context,
                             const Extension* extension) override;
  void AddAppWindow(gfx::NativeWindow window) override;
  void CloseAppWindows() override;

  // RootWindowController::DesktopDelegate:
  void CloseRootWindowController(
      RootWindowController* root_window_controller) override;

#if defined(OS_CHROMEOS)
  // chromeos::PowerManagerClient::Observer overrides:
  void PowerButtonEventReceived(bool down,
                                const base::TimeTicks& timestamp) override;

  // display::DisplayConfigurator::Observer overrides.
  void OnDisplayModeChanged(
      const display::DisplayConfigurator::DisplayStateList& displays) override;
#endif

  // ui::internal::InputMethodDelegate overrides:
  ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* key_event) override;

  // Returns the RootWindowController's WindowTreeHost.
  aura::WindowTreeHost* GetPrimaryHost();

  RootWindowController* root_window_controller() {
    return root_window_controller_.get();
  }

 protected:
  // Creates and sets the aura clients and window manager stuff. Subclass may
  // initialize different sets of the clients.
  virtual void InitWindowManager();

  // Tears down the window manager stuff set up in InitWindowManager().
  virtual void TearDownWindowManager();

 private:
  // Creates the RootWindowController that hosts the app.
  void CreateRootWindowController();

  // Finishes set-up using the RootWindowController.
  void FinalizeWindowManager();

  // Returns the desired dimensions of the RootWindowController from the command
  // line, or falls back to a default size.
  gfx::Size GetStartingWindowSize();

  // Returns the dimensions (in pixels) of the primary display, or an empty size
  // if the dimensions can't be determined or no display is connected.
  gfx::Size GetPrimaryDisplaySize();

  content::BrowserContext* const browser_context_;

#if defined(OS_CHROMEOS)
  std::unique_ptr<display::DisplayConfigurator> display_configurator_;
#endif

  std::unique_ptr<ShellScreen> screen_;

  std::unique_ptr<wm::CompoundEventFilter> root_window_event_filter_;

  // TODO(michaelpg): Support one RootWindowController per display.
  std::unique_ptr<RootWindowController> root_window_controller_;

  std::unique_ptr<ui::InputMethod> input_method_;

  std::unique_ptr<aura::client::DefaultCaptureClient> capture_client_;

  std::unique_ptr<wm::FocusController> focus_controller_;

  std::unique_ptr<wm::CursorManager> cursor_manager_;

  std::unique_ptr<ui::UserActivityDetector> user_activity_detector_;
#if defined(OS_CHROMEOS)
  std::unique_ptr<ui::UserActivityPowerManagerNotifier> user_activity_notifier_;
#endif

  std::unique_ptr<AppWindowClient> app_window_client_;

  // NativeAppWindow::Close() deletes the AppWindow.
  std::list<AppWindow*> app_windows_;

  // A pointer to the main message loop if this is run by ShellBrowserMainParts.
  base::RunLoop* run_loop_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ShellDesktopControllerAura);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_DESKTOP_CONTROLLER_AURA_H_
