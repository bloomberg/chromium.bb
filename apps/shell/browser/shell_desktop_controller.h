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

#if defined(OS_CHROMEOS) && defined(USE_X11)
#include "ui/display/chromeos/output_configurator.h"
#endif

namespace aura {
class TestScreen;
class WindowTreeHost;
}

namespace wm {
class WMTestHelper;
}

namespace apps {

// Handles desktop-related tasks for app_shell.
class ShellDesktopController
#if defined(OS_CHROMEOS) && defined(USE_X11)
    : public ui::OutputConfigurator::Observer
#endif
    {
 public:
  ShellDesktopController();
  virtual ~ShellDesktopController();

  // Returns the host for the Aura window tree.
  aura::WindowTreeHost* GetWindowTreeHost();

#if defined(OS_CHROMEOS) && defined(USE_X11)
  // ui::OutputConfigurator::Observer overrides.
  virtual void OnDisplayModeChanged(
      const std::vector<ui::OutputConfigurator::DisplayState>& outputs)
      OVERRIDE;
#endif

 private:
  // Creates the window that hosts the app.
  void CreateRootWindow();

  // Closes and destroys the root window hosting the app.
  void DestroyRootWindow();

  // Returns the dimensions (in pixels) of the primary display, or an empty size
  // if the dimensions can't be determined or no display is connected.
  gfx::Size GetPrimaryDisplaySize();

#if defined(OS_CHROMEOS) && defined(USE_X11)
  scoped_ptr<ui::OutputConfigurator> output_configurator_;
#endif

  // Enable a minimal set of views::corewm to be initialized.
  scoped_ptr<wm::WMTestHelper> wm_test_helper_;

  scoped_ptr<aura::TestScreen> test_screen_;

  DISALLOW_COPY_AND_ASSIGN(ShellDesktopController);
};

}  // namespace apps

#endif  // APPS_SHELL_BROWSER_SHELL_DESKTOP_CONTROLLER_H_
