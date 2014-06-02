// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_BROWSER_SHELL_BROWSER_MAIN_DELEGATE_H_
#define APPS_SHELL_BROWSER_SHELL_BROWSER_MAIN_DELEGATE_H_

namespace content {
class BrowserContext;
}

namespace apps {

class ShellDesktopController;

class ShellBrowserMainDelegate {
 public:
  virtual ~ShellBrowserMainDelegate() {}

  // Called to start an application after all initialization processes that are
  // necesary to run apps are completed.
  virtual void Start(content::BrowserContext* context) = 0;

  // Called after the main message looop has stopped, but before
  // other services such as BrowserContext / extension system are shut down.
  virtual void Shutdown() = 0;

  // Creates the ShellDesktopController instance to initialize the root window
  // and window manager. Subclass may return its subclass to customize the
  // windo manager.
  virtual ShellDesktopController* CreateDesktopController() = 0;
};

}  // namespace apps

#endif  // APPS_SHELL_BROWSER_SHELL_BROWSER_MAIN_DELEGATE_H_
