// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_SHELL_WINDOW_LAUNCHER_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_SHELL_WINDOW_LAUNCHER_CONTROLLER_H_

#include <list>
#include <map>
#include <string>

#include "apps/shell_window_registry.h"
#include "ui/aura/client/activation_change_observer.h"
#include "ui/aura/window_observer.h"

namespace apps {
class ShellWindow;
}

namespace aura {

class Window;

namespace client {
class ActivationClient;
}

}

class ChromeLauncherController;
class Profile;
class ShellWindowLauncherItemController;

// ShellWindowLauncherController observes the Shell Window registry and the
// aura window manager. It handles adding and removing launcher items from
// ChromeLauncherController.
class ShellWindowLauncherController
    : public apps::ShellWindowRegistry::Observer,
      public aura::WindowObserver,
      public aura::client::ActivationChangeObserver {
 public:
  explicit ShellWindowLauncherController(ChromeLauncherController* owner);
  virtual ~ShellWindowLauncherController();

  // Called by ChromeLauncherController when the active user changed and the
  // items need to be updated.
  virtual void ActiveUserChanged(const std::string& user_email) {}

  // An additional user identified by |Profile|, got added to the existing
  // session.
  virtual void AdditionalUserAddedToSession(Profile* profile);

  // Overridden from ShellWindowRegistry::Observer:
  virtual void OnShellWindowAdded(apps::ShellWindow* shell_window) OVERRIDE;
  virtual void OnShellWindowIconChanged(
      apps::ShellWindow* shell_window) OVERRIDE;
  virtual void OnShellWindowRemoved(apps::ShellWindow* shell_window) OVERRIDE;

  // Overriden from aura::WindowObserver:
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;

  // Overriden from client::ActivationChangeObserver:
  virtual void OnWindowActivated(aura::Window* gained_active,
                                 aura::Window* lost_active) OVERRIDE;

 protected:
  // Registers a shell window with the shelf and this object.
  void RegisterApp(apps::ShellWindow* shell_window);

  // Unregisters a shell window with the shelf and this object.
  void UnregisterApp(aura::Window* window);

  // Check if a given window is known to the launcher controller.
  bool IsRegisteredApp(aura::Window* window);

 private:
  typedef std::map<std::string, ShellWindowLauncherItemController*>
      AppControllerMap;
  typedef std::map<aura::Window*, std::string> WindowToAppLauncherIdMap;

  ShellWindowLauncherItemController* ControllerForWindow(aura::Window* window);

  ChromeLauncherController* owner_;
  // A set of unowned ShellWindowRegistry pointers for loaded users.
  // Note that this will only be used with multiple users in the side by side
  // mode.
  std::set<apps::ShellWindowRegistry*> registry_;
  aura::client::ActivationClient* activation_client_;

  // Map of app launcher id to controller.
  AppControllerMap app_controller_map_;

  // Allows us to get from an aura::Window to the app launcher id.
  WindowToAppLauncherIdMap window_to_app_launcher_id_map_;

  DISALLOW_COPY_AND_ASSIGN(ShellWindowLauncherController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_SHELL_WINDOW_LAUNCHER_CONTROLLER_H_
