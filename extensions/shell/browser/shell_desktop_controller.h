// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_DESKTOP_CONTROLLER_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_DESKTOP_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/window_tree_host_observer.h"

#if defined(OS_CHROMEOS)
#include "ui/display/chromeos/display_configurator.h"
#endif

namespace aura {
class TestScreen;
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

#if defined(OS_CHROMEOS)
namespace ui {
class UserActivityPowerManagerNotifier;
}
#endif

namespace wm {
class CompoundEventFilter;
class CursorManager;
class FocusRules;
class InputMethodEventFilter;
class UserActivityDetector;
}

namespace extensions {

class ShellAppWindow;
class ShellAppWindowController;

// Handles desktop-related tasks for app_shell.
class ShellDesktopController : public aura::client::WindowTreeClient,
                               public aura::WindowTreeHostObserver
#if defined(OS_CHROMEOS)
                               ,
                               public ui::DisplayConfigurator::Observer
#endif
                               {
 public:
  ShellDesktopController();
  virtual ~ShellDesktopController();

  // Returns the single instance of the desktop. (Stateless functions like
  // ShellAppWindowCreateFunction need to be able to access the desktop, so
  // we need a singleton somewhere).
  static ShellDesktopController* instance();

  aura::WindowTreeHost* host() { return host_.get(); }

  // Creates the window that hosts the app.
  void CreateRootWindow();

  // Sets the controller to create/close the app windows. Takes the ownership of
  // |app_window_controller|.
  void SetAppWindowController(ShellAppWindowController* app_window_controller);

  // Creates a new app window and adds it to the desktop. The desktop maintains
  // ownership of the window.
  ShellAppWindow* CreateAppWindow(content::BrowserContext* context);

  // Closes and destroys the app windows.
  void CloseAppWindows();

  // Overridden from aura::client::WindowTreeClient:
  virtual aura::Window* GetDefaultParent(aura::Window* context,
                                         aura::Window* window,
                                         const gfx::Rect& bounds) OVERRIDE;

#if defined(OS_CHROMEOS)
  // ui::DisplayConfigurator::Observer overrides.
  virtual void OnDisplayModeChanged(const std::vector<
      ui::DisplayConfigurator::DisplayState>& displays) OVERRIDE;
#endif

  // aura::WindowTreeHostObserver overrides:
  virtual void OnHostCloseRequested(const aura::WindowTreeHost* host) OVERRIDE;

 protected:
  // Creates and sets the aura clients and window manager stuff. Subclass may
  // initialize different sets of the clients.
  virtual void InitWindowManager();

  // Creates a focus rule that is to be used in the InitWindowManager.
  virtual wm::FocusRules* CreateFocusRules();

 private:
  // Closes and destroys the root window hosting the app.
  void DestroyRootWindow();

  // Returns the dimensions (in pixels) of the primary display, or an empty size
  // if the dimensions can't be determined or no display is connected.
  gfx::Size GetPrimaryDisplaySize();

#if defined(OS_CHROMEOS)
  scoped_ptr<ui::DisplayConfigurator> display_configurator_;
#endif

  scoped_ptr<aura::TestScreen> test_screen_;

  scoped_ptr<aura::WindowTreeHost> host_;

  scoped_ptr<wm::CompoundEventFilter> root_window_event_filter_;

  scoped_ptr<aura::client::DefaultCaptureClient> capture_client_;

  scoped_ptr<wm::InputMethodEventFilter> input_method_filter_;

  scoped_ptr<aura::client::FocusClient> focus_client_;

  scoped_ptr<wm::CursorManager> cursor_manager_;

  scoped_ptr<wm::UserActivityDetector> user_activity_detector_;
#if defined(OS_CHROMEOS)
  scoped_ptr<ui::UserActivityPowerManagerNotifier> user_activity_notifier_;
#endif

  // The desktop supports a single app window.
  scoped_ptr<ShellAppWindowController> app_window_controller_;

  DISALLOW_COPY_AND_ASSIGN(ShellDesktopController);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_DESKTOP_CONTROLLER_H_
