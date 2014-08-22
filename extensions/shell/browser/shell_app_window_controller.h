// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_APP_WINDOW_CONTROLLER_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_APP_WINDOW_CONTROLLER_H_

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;
class ShellAppWindow;

class ShellAppWindowController {
 public:
  virtual ~ShellAppWindowController() {}

  // Creates a new app window and adds it to the desktop. This class should
  // maintain the ownership of the window.
  virtual ShellAppWindow* CreateAppWindow(content::BrowserContext* context,
                                          const Extension* extension) = 0;

  // Closes and destroys the app windows. Must be called before the extension
  // is destroyed.
  virtual void CloseAppWindows() = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_APP_WINDOW_CONTROLLER_H_
