// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_DEFAULT_SHELL_APP_WINDOW_CONTROLLER_H_
#define EXTENSIONS_SHELL_BROWSER_DEFAULT_SHELL_APP_WINDOW_CONTROLLER_H_

#include "extensions/shell/browser/shell_app_window_controller.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace extensions {

class ShellDesktopController;

// The default shell app window controller for app_shell.  It manages only one
// app_window.
class DefaultShellAppWindowController : public ShellAppWindowController {
 public:
  explicit DefaultShellAppWindowController(
      ShellDesktopController* shell_desktop_controller);
  virtual ~DefaultShellAppWindowController();

  // ShellAppWindowController implementation.
  virtual ShellAppWindow* CreateAppWindow(content::BrowserContext* context,
                                          const Extension* extension) OVERRIDE;
  virtual void CloseAppWindows() OVERRIDE;

 private:
  ShellDesktopController* shell_desktop_controller_;  // Not owned

  // The desktop supports a single app window.
  scoped_ptr<ShellAppWindow> app_window_;

  DISALLOW_COPY_AND_ASSIGN(DefaultShellAppWindowController);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_DEFAULT_SHELL_APP_WINDOW_CONTROLLER_H_
