// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_MULTI_PROFILE_APP_WINDOW_LAUNCHER_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_MULTI_PROFILE_APP_WINDOW_LAUNCHER_CONTROLLER_H_

#include "chrome/browser/ui/ash/launcher/app_window_launcher_controller.h"

// Inherits from AppWindowLauncherController and overwrites the AppWindow
// observing functions to switch between users dynamically.
class MultiProfileAppWindowLauncherController
    : public AppWindowLauncherController {
 public:
  explicit MultiProfileAppWindowLauncherController(
      ChromeLauncherController* owner);
  virtual ~MultiProfileAppWindowLauncherController();

  // Overridden from AppWindowLauncherController:
  virtual void ActiveUserChanged(const std::string& user_email) OVERRIDE;
  virtual void AdditionalUserAddedToSession(Profile* profile) OVERRIDE;

  // Overridden from AppWindowRegistry::Observer:
  virtual void OnAppWindowAdded(apps::AppWindow* app_window) OVERRIDE;
  virtual void OnAppWindowRemoved(apps::AppWindow* app_window) OVERRIDE;
  virtual void OnAppWindowShown(apps::AppWindow* app_window) OVERRIDE;
  virtual void OnAppWindowHidden(apps::AppWindow* app_window) OVERRIDE;

 private:
  typedef std::vector<apps::AppWindow*> AppWindowList;
  typedef std::vector<apps::AppWindowRegistry*> AppWindowRegistryList;

  // Returns true if the owner of the given |app_window| has a window teleported
  // of the |app_window|'s application type to the current desktop.
  bool UserHasAppOnActiveDesktop(apps::AppWindow* app_window);

  // A list of all app windows for all users.
  AppWindowList app_window_list_;

  // A list of the app window registries which we additionally observe.
  AppWindowRegistryList multi_user_registry_;

  DISALLOW_COPY_AND_ASSIGN(MultiProfileAppWindowLauncherController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_MULTI_PROFILE_APP_WINDOW_LAUNCHER_CONTROLLER_H_
