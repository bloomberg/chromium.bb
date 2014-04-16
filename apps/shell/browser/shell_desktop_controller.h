// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_BROWSER_SHELL_DESKTOP_CONTROLLER_H_
#define APPS_SHELL_BROWSER_SHELL_DESKTOP_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/gfx/geometry/size.h"

#if defined(OS_CHROMEOS)
#include "ui/display/chromeos/display_configurator.h"
#endif

namespace aura {
class TestScreen;
class WindowTreeHost;
}

namespace content {
class BrowserContext;
}

#if defined(OS_CHROMEOS)
namespace ui {
class UserActivityPowerManagerNotifier;
}
#endif

namespace wm {
class UserActivityDetector;
class WMTestHelper;
}

namespace apps {

class ShellAppWindow;

// Handles desktop-related tasks for app_shell.
class ShellDesktopController
#if defined(OS_CHROMEOS)
    : public ui::DisplayConfigurator::Observer
#endif
      {
 public:
  ShellDesktopController();
  virtual ~ShellDesktopController();

  // Returns the single instance of the desktop. (Stateless functions like
  // ShellAppWindowCreateFunction need to be able to access the desktop, so
  // we need a singleton somewhere).
  static ShellDesktopController* instance();

  // Creates a new app window and adds it to the desktop. The desktop maintains
  // ownership of the window.
  ShellAppWindow* CreateAppWindow(content::BrowserContext* context);

  // Closes and destroys the app window.
  void CloseAppWindow();

  // Returns the host for the Aura window tree.
  aura::WindowTreeHost* GetWindowTreeHost();

#if defined(OS_CHROMEOS)
  // ui::DisplayConfigurator::Observer overrides.
  virtual void OnDisplayModeChanged(const std::vector<
      ui::DisplayConfigurator::DisplayState>& displays) OVERRIDE;
#endif

 private:
  // Creates the window that hosts the app.
  void CreateRootWindow();

  // Closes and destroys the root window hosting the app.
  void DestroyRootWindow();

  // Returns the dimensions (in pixels) of the primary display, or an empty size
  // if the dimensions can't be determined or no display is connected.
  gfx::Size GetPrimaryDisplaySize();

#if defined(OS_CHROMEOS)
  scoped_ptr<ui::DisplayConfigurator> display_configurator_;
#endif

  // Enable a minimal set of views::corewm to be initialized.
  scoped_ptr<wm::WMTestHelper> wm_test_helper_;

  scoped_ptr<aura::TestScreen> test_screen_;

  scoped_ptr<wm::UserActivityDetector> user_activity_detector_;
#if defined(OS_CHROMEOS)
  scoped_ptr<ui::UserActivityPowerManagerNotifier> user_activity_notifier_;
#endif

  // The desktop supports a single app window.
  scoped_ptr<ShellAppWindow> app_window_;

  DISALLOW_COPY_AND_ASSIGN(ShellDesktopController);
};

}  // namespace apps

#endif  // APPS_SHELL_BROWSER_SHELL_DESKTOP_CONTROLLER_H_
