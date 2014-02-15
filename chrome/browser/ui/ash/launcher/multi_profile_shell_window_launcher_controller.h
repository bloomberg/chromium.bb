// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_MULTI_PROFILE_SHELL_WINDOW_LAUNCHER_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_MULTI_PROFILE_SHELL_WINDOW_LAUNCHER_CONTROLLER_H_

#include "chrome/browser/ui/ash/launcher/shell_window_launcher_controller.h"

// Inherits from ShellWindowLauncherController and overwrites the ShellWindow
// observing functions to switch between users dynamically.
// TODO(jamescook): Rename this to MultiProfileAppWindowLauncherController.
// http://crbug.com/344079
class MultiProfileShellWindowLauncherController
    : public ShellWindowLauncherController {
 public:
  explicit MultiProfileShellWindowLauncherController(
      ChromeLauncherController* owner);
  virtual ~MultiProfileShellWindowLauncherController();

  // Overridden from ShellWindowLauncherController:
  virtual void ActiveUserChanged(const std::string& user_email) OVERRIDE;
  virtual void AdditionalUserAddedToSession(Profile* profile) OVERRIDE;

  // Overridden from AppWindowRegistry::Observer:
  virtual void OnAppWindowAdded(apps::AppWindow* app_window) OVERRIDE;
  virtual void OnAppWindowRemoved(apps::AppWindow* app_window) OVERRIDE;

 private:
  typedef std::vector<apps::AppWindow*> AppWindowList;
  typedef std::vector<apps::AppWindowRegistry*> AppWindowRegistryList;

  // A list of all app windows for all users.
  AppWindowList app_window_list_;

  // A list of the app window registries which we additionally observe.
  AppWindowRegistryList multi_user_registry_;

  DISALLOW_COPY_AND_ASSIGN(MultiProfileShellWindowLauncherController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_MULTI_PROFILE_SHELL_WINDOW_LAUNCHER_CONTROLLER_H_
