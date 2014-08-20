// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/multi_profile_app_window_launcher_controller.h"

#include "apps/app_window.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/host_desktop.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "ui/aura/window.h"

namespace {

bool ControlsWindow(aura::Window* window) {
  return chrome::GetHostDesktopTypeForNativeWindow(window) ==
         chrome::HOST_DESKTOP_TYPE_ASH;
}

}  // namespace

MultiProfileAppWindowLauncherController::
    MultiProfileAppWindowLauncherController(ChromeLauncherController* owner)
    : AppWindowLauncherController(owner) {}

MultiProfileAppWindowLauncherController::
    ~MultiProfileAppWindowLauncherController() {
  // We need to remove all Registry observers for added users.
  for (AppWindowRegistryList::iterator it = multi_user_registry_.begin();
       it != multi_user_registry_.end();
       ++it)
    (*it)->RemoveObserver(this);
}

void MultiProfileAppWindowLauncherController::ActiveUserChanged(
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
        !IsRegisteredApp(app_window->GetNativeWindow()) &&
        (app_window->GetBaseWindow()->IsMinimized() ||
         app_window->GetNativeWindow()->IsVisible()))
      RegisterApp(*it);
  }
}

void MultiProfileAppWindowLauncherController::AdditionalUserAddedToSession(
    Profile* profile) {
  // Each users AppWindowRegistry needs to be observed.
  apps::AppWindowRegistry* registry = apps::AppWindowRegistry::Get(profile);
  multi_user_registry_.push_back(registry);
  registry->AddObserver(this);
}

void MultiProfileAppWindowLauncherController::OnAppWindowAdded(
    apps::AppWindow* app_window) {
  if (!ControlsWindow(app_window->GetNativeWindow()))
    return;

  app_window_list_.push_back(app_window);
  Profile* profile = Profile::FromBrowserContext(app_window->browser_context());
  // If the window got created for a non active user but the user allowed to
  // teleport to the current user's desktop, we teleport it now.
  if (!multi_user_util::IsProfileFromActiveUser(profile) &&
      UserHasAppOnActiveDesktop(app_window)) {
    chrome::MultiUserWindowManager::GetInstance()->ShowWindowForUser(
        app_window->GetNativeWindow(), multi_user_util::GetCurrentUserId());
  }
}

void MultiProfileAppWindowLauncherController::OnAppWindowShown(
    apps::AppWindow* app_window) {
  if (!ControlsWindow(app_window->GetNativeWindow()))
    return;

  Profile* profile = Profile::FromBrowserContext(app_window->browser_context());

  if (multi_user_util::IsProfileFromActiveUser(profile) &&
      !IsRegisteredApp(app_window->GetNativeWindow())) {
    RegisterApp(app_window);
    return;
  }

  // The panel layout manager only manages windows which are anchored.
  // Since this window did never had an anchor, it would stay hidden. We
  // therefore make it visible now.
  if (UserHasAppOnActiveDesktop(app_window) &&
      app_window->GetNativeWindow()->type() == ui::wm::WINDOW_TYPE_PANEL &&
      !app_window->GetNativeWindow()->layer()->GetTargetOpacity()) {
    app_window->GetNativeWindow()->layer()->SetOpacity(1.0f);
  }
}

void MultiProfileAppWindowLauncherController::OnAppWindowHidden(
    apps::AppWindow* app_window) {
  if (!ControlsWindow(app_window->GetNativeWindow()))
    return;

  Profile* profile = Profile::FromBrowserContext(app_window->browser_context());
  if (multi_user_util::IsProfileFromActiveUser(profile) &&
      IsRegisteredApp(app_window->GetNativeWindow())) {
    UnregisterApp(app_window->GetNativeWindow());
  }
}

void MultiProfileAppWindowLauncherController::OnAppWindowRemoved(
    apps::AppWindow* app_window) {
  if (!ControlsWindow(app_window->GetNativeWindow()))
    return;

  // If the application is registered with AppWindowLauncher (because the user
  // is currently active), the OnWindowDestroying observer has already (or will
  // soon) unregister it independently from the shelf. If it was not registered
  // we don't need to do anything anyways. As such, all which is left to do here
  // is to get rid of our own reference.
  AppWindowList::iterator it =
      std::find(app_window_list_.begin(), app_window_list_.end(), app_window);
  DCHECK(it != app_window_list_.end());
  app_window_list_.erase(it);
}

bool MultiProfileAppWindowLauncherController::UserHasAppOnActiveDesktop(
    apps::AppWindow* app_window) {
  const std::string& app_id = app_window->extension_id();
  content::BrowserContext* app_context = app_window->browser_context();
  DCHECK(!app_context->IsOffTheRecord());
  const std::string& current_user = multi_user_util::GetCurrentUserId();
  chrome::MultiUserWindowManager* manager =
      chrome::MultiUserWindowManager::GetInstance();
  for (AppWindowList::iterator it = app_window_list_.begin();
       it != app_window_list_.end();
       ++it) {
    apps::AppWindow* other_window = *it;
    DCHECK(!other_window->browser_context()->IsOffTheRecord());
    if (manager->IsWindowOnDesktopOfUser(other_window->GetNativeWindow(),
                                         current_user) &&
        app_id == other_window->extension_id() &&
        app_context == other_window->browser_context()) {
      return true;
    }
  }
  return false;
}
