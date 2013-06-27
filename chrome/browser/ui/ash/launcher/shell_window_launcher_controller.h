// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_SHELL_WINDOW_LAUNCHER_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_SHELL_WINDOW_LAUNCHER_CONTROLLER_H_

#include <list>
#include <map>
#include <string>

#include "chrome/browser/extensions/shell_window_registry.h"
#include "ui/aura/client/activation_change_observer.h"
#include "ui/aura/window_observer.h"

namespace aura {

class Window;

namespace client {
class ActivationClient;
}

}

class ChromeLauncherController;
class ShellWindow;
class ShellWindowLauncherItemController;

// ShellWindowLauncherController observes the Shell Window registry and the
// aura window manager. It handles adding and removing launcher items from
// ChromeLauncherController.
class ShellWindowLauncherController
    : public extensions::ShellWindowRegistry::Observer,
      public aura::WindowObserver,
      public aura::client::ActivationChangeObserver {
 public:
  explicit ShellWindowLauncherController(ChromeLauncherController* owner);
  virtual ~ShellWindowLauncherController();

  // Overridden from ShellWindowRegistry::Observer:
  virtual void OnShellWindowAdded(ShellWindow* shell_window) OVERRIDE;
  virtual void OnShellWindowIconChanged(ShellWindow* shell_window) OVERRIDE;
  virtual void OnShellWindowRemoved(ShellWindow* shell_window) OVERRIDE;

  // Overriden from aura::WindowObserver:
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;

  // Overriden from client::ActivationChangeObserver:
  virtual void OnWindowActivated(aura::Window* gained_active,
                                 aura::Window* lost_active) OVERRIDE;

  // Returns the number of running applications.
  int NumberOfAppsRunning();

  // Activate the application with the given index.
  void ActivateAppAt(int index);

 private:
  typedef std::map<std::string, ShellWindowLauncherItemController*>
      AppControllerMap;
  typedef std::map<aura::Window*, std::string> WindowToAppLauncherIdMap;

  ShellWindowLauncherItemController* ControllerForWindow(aura::Window* window);

  ChromeLauncherController* owner_;
  extensions::ShellWindowRegistry* registry_;  // Unowned convenience pointer
  aura::client::ActivationClient* activation_client_;

  // Map of app launcher id to controller.
  AppControllerMap app_controller_map_;

  // Allows us to get from an aura::Window to the app launcher id.
  WindowToAppLauncherIdMap window_to_app_launcher_id_map_;

  DISALLOW_COPY_AND_ASSIGN(ShellWindowLauncherController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_SHELL_WINDOW_LAUNCHER_CONTROLLER_H_
