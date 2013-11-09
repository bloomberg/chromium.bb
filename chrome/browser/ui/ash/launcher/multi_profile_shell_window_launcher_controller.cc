// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/multi_profile_shell_window_launcher_controller.h"

#include "apps/shell_window.h"
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
  for (ShellWindowRegistryList::iterator it = multi_user_registry_.begin();
       it != multi_user_registry_.end(); ++it)
    (*it)->RemoveObserver(this);
}

void MultiProfileShellWindowLauncherController::ActiveUserChanged(
    const std::string& user_email) {
  // The active user has changed and we need to traverse our list of items to
  // show / hide them one by one. To avoid that a user dependent state
  // "survives" in a launcher item, we first delete all items making sure that
  // nothing remains and then re-create them again.
  for (ShellWindowList::iterator it = shell_window_list_.begin();
       it != shell_window_list_.end(); ++it) {
    apps::ShellWindow* shell_window = *it;
    if (!multi_user_util::IsProfileFromActiveUser(shell_window->profile()) &&
        IsRegisteredApp(shell_window->GetNativeWindow()))
      UnregisterApp(shell_window->GetNativeWindow());
  }
  for (ShellWindowList::iterator it = shell_window_list_.begin();
       it != shell_window_list_.end(); ++it) {
    apps::ShellWindow* shell_window = *it;
    if (multi_user_util::IsProfileFromActiveUser(shell_window->profile()) &&
        !IsRegisteredApp(shell_window->GetNativeWindow()))
      RegisterApp(*it);
  }
}

void MultiProfileShellWindowLauncherController::AdditionalUserAddedToSession(
    Profile* profile) {
  // Each users ShellRegistry needs to be observed.
  apps::ShellWindowRegistry* registry = apps::ShellWindowRegistry::Get(profile);
  multi_user_registry_.push_back(registry);
  registry->AddObserver(this);
}

void MultiProfileShellWindowLauncherController::OnShellWindowAdded(
    apps::ShellWindow* shell_window) {
  if (!ControlsWindow(shell_window->GetNativeWindow()))
    return;
  shell_window_list_.push_back(shell_window);
  if (multi_user_util::IsProfileFromActiveUser(shell_window->profile()))
    RegisterApp(shell_window);
}

void MultiProfileShellWindowLauncherController::OnShellWindowRemoved(
    apps::ShellWindow* shell_window) {
  if (!ControlsWindow(shell_window->GetNativeWindow()))
    return;

  // If the application is registered with ShellWindowLauncher (because the user
  // is currently active), the OnWindowDestroying observer has already (or will
  // soon) unregister it independently from the shelf. If it was not registered
  // we don't need to do anything anyways. As such, all which is left to do here
  // is to get rid of our own reference.
  ShellWindowList::iterator it = std::find(shell_window_list_.begin(),
                                           shell_window_list_.end(),
                                           shell_window);
  DCHECK(it != shell_window_list_.end());
  shell_window_list_.erase(it);
}
