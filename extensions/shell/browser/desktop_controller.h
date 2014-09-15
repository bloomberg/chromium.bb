// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_DESKTOP_CONTROLLER_H_
#define EXTENSIONS_SHELL_BROWSER_DESKTOP_CONTROLLER_H_

namespace aura {
class Window;
class WindowTreeHost;
}

namespace content {
class BrowserContext;
}

namespace extensions {
class AppWindow;
class Extension;
class ShellAppWindow;

// DesktopController is an interface to construct the window environment in
// extensions shell. ShellDesktopController provides a default implementation
// for app_shell, and embedder (such as athena) can provide its own.
// TODO(jamescook|oshima): Clean up this interface now that there is only one
// way to create an app window.
class DesktopController {
 public:
  DesktopController();
  virtual ~DesktopController();

  // Returns the single instance of the desktop. (Stateless functions like
  // ShellAppWindowCreateFunction need to be able to access the desktop, so
  // we need a singleton somewhere).
  static DesktopController* instance();

  // Returns the WindowTreeHost created by this DesktopController.
  virtual aura::WindowTreeHost* GetHost() = 0;

  // Creates a new app window and adds it to the desktop. The desktop maintains
  // ownership of the window. The window must be closed before |extension| is
  // destroyed.
  virtual AppWindow* CreateAppWindow(content::BrowserContext* context,
                                     const Extension* extension) = 0;

  // Attaches the window to our window hierarchy.
  virtual void AddAppWindow(aura::Window* window) = 0;

  // Closes and destroys the app windows.
  virtual void CloseAppWindows() = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_DESKTOP_CONTROLLER_H_
