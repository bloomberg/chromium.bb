// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_MULTI_PROFILE_SHELL_WINDOW_LAUNCHER_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_MULTI_PROFILE_SHELL_WINDOW_LAUNCHER_CONTROLLER_H_

#include "chrome/browser/ui/ash/launcher/shell_window_launcher_controller.h"

// Inherits from ShellWindowLauncherController and overwrites the ShellWindow
// observing functions to switch between users dynamically.
class MultiProfileShellWindowLauncherController
    : public ShellWindowLauncherController {
 public:
  explicit MultiProfileShellWindowLauncherController(
      ChromeLauncherController* owner);
  virtual ~MultiProfileShellWindowLauncherController();

  // Overridden from ShellWindowLauncherController:
  virtual void ActiveUserChanged(const std::string& user_email) OVERRIDE;
  virtual void AdditionalUserAddedToSession(Profile* profile) OVERRIDE;

  // Overridden from ShellWindowRegistry::Observer:
  virtual void OnShellWindowAdded(apps::ShellWindow* shell_window) OVERRIDE;
  virtual void OnShellWindowRemoved(apps::ShellWindow* shell_window) OVERRIDE;

 private:
  typedef std::vector<apps::ShellWindow*> ShellWindowList;
  typedef std::vector<apps::ShellWindowRegistry*> ShellWindowRegistryList;

  // Returns true when the given window is from the active user.
  bool IsShellWindowFromActiveUser(apps::ShellWindow* shell_window);

  // A list of all shell windows for all users.
  ShellWindowList shell_window_list_;

  // A list of the shell window registries which we additionally observe.
  ShellWindowRegistryList multi_user_registry_;

  DISALLOW_COPY_AND_ASSIGN(MultiProfileShellWindowLauncherController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_MULTI_PROFILE_SHELL_WINDOW_LAUNCHER_CONTROLLER_H_
