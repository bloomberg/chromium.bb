// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/multi_profile_shell_window_launcher_controller.h"

#include "apps/app_window.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/host_desktop.h"

namespace {

bool ControlsWindow(aura::Window* window) {
  return chrome::GetHostDesktopTypeForNativeWindow(window) ==
      chrome::HOST_DESKTOP_TYPE_ASH;
}

}  // namespace


MultiProfileShellWindowLauncherController::
    MultiProfileShellWindowLauncherController(
    ChromeLauncherController* owner)
    : ShellWindowLauncherController(owner) {
}

MultiProfileShellWindowLauncherController::
    ~MultiProfileShellWindowLauncherController() {
  // We need to remove all Registry observers for added users.
  for (AppWindowRegistryList::iterator it = multi_user_registry_.begin();
       it != multi_user_registry_.end();
       ++it)
    (*it)->RemoveObserver(this);
}

void MultiProfileShellWindowLauncherController::ActiveUserChanged(
    const std::string& user_email) {
  // The active user has changed and we need to traverse our list of items to
  // show / hide them one by one. To avoid that a user dependent state
  // "survives" in a launcher item, we first delete all items making sure that
  // nothing remains and then re-create them again.
  for (AppWindowList::iterator it = app_window_list_.begin();
       it != app_window_list_.end();
       ++it) {
    apps::AppWindow* app_window = *it;
    Profile* profile =
        Profile::FromBrowserContext(app_window->browser_context());
    if (!multi_user_util::IsProfileFromActiveUser(profile) &&
        IsRegisteredApp(app_window->GetNativeWindow()))
      UnregisterApp(app_window->GetNativeWindow());
  }
  for (AppWindowList::iterator it = app_window_list_.begin();
       it != app_window_list_.end();
       ++it) {
    apps::AppWindow* app_window = *it;
    Profile* profile =
        Profile::FromBrowserContext(app_window->browser_context());
    if (multi_user_util::IsProfileFromActiveUser(profile) &&
        !IsRegisteredApp(app_window->GetNativeWindow()))
      RegisterApp(*it);
  }
}

void MultiProfileShellWindowLauncherController::AdditionalUserAddedToSession(
    Profile* profile) {
  // Each users AppWindowRegistry needs to be observed.
  apps::AppWindowRegistry* registry = apps::AppWindowRegistry::Get(profile);
  multi_user_registry_.push_back(registry);
  registry->AddObserver(this);
}

void MultiProfileShellWindowLauncherController::OnAppWindowAdded(
    apps::AppWindow* app_window) {
  if (!ControlsWindow(app_window->GetNativeWindow()))
    return;
  app_window_list_.push_back(app_window);
  Profile* profile = Profile::FromBrowserContext(app_window->browser_context());
  if (multi_user_util::IsProfileFromActiveUser(profile))
    RegisterApp(app_window);
}

void MultiProfileShellWindowLauncherController::OnAppWindowRemoved(
    apps::AppWindow* app_window) {
  if (!ControlsWindow(app_window->GetNativeWindow()))
    return;

  // If the application is registered with ShellWindowLauncher (because the user
  // is currently active), the OnWindowDestroying observer has already (or will
  // soon) unregister it independently from the shelf. If it was not registered
  // we don't need to do anything anyways. As such, all which is left to do here
  // is to get rid of our own reference.
  AppWindowList::iterator it =
      std::find(app_window_list_.begin(), app_window_list_.end(), app_window);
  DCHECK(it != app_window_list_.end());
  app_window_list_.erase(it);
}
