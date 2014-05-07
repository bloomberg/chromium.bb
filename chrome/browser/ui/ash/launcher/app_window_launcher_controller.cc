// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_window_launcher_controller.h"

#include "apps/app_window.h"
#include "ash/shelf/shelf_util.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/host_desktop.h"
#include "extensions/common/extension.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/wm/public/activation_client.h"

using apps::AppWindow;

namespace {

std::string GetAppShelfId(AppWindow* app_window) {
  if (app_window->window_type_is_panel())
    return base::StringPrintf("panel:%d", app_window->session_id().id());
  return app_window->extension_id();
}

bool ControlsWindow(aura::Window* window) {
  return chrome::GetHostDesktopTypeForNativeWindow(window) ==
         chrome::HOST_DESKTOP_TYPE_ASH;
}

}  // namespace

AppWindowLauncherController::AppWindowLauncherController(
    ChromeLauncherController* owner)
    : owner_(owner), activation_client_(NULL) {
  apps::AppWindowRegistry* registry =
      apps::AppWindowRegistry::Get(owner->profile());
  registry_.insert(registry);
  registry->AddObserver(this);
  if (ash::Shell::HasInstance()) {
    if (ash::Shell::GetInstance()->GetPrimaryRootWindow()) {
      activation_client_ = aura::client::GetActivationClient(
          ash::Shell::GetInstance()->GetPrimaryRootWindow());
      if (activation_client_)
        activation_client_->AddObserver(this);
    }
  }
}

AppWindowLauncherController::~AppWindowLauncherController() {
  for (std::set<apps::AppWindowRegistry*>::iterator it = registry_.begin();
       it != registry_.end();
       ++it)
    (*it)->RemoveObserver(this);

  if (activation_client_)
    activation_client_->RemoveObserver(this);
  for (WindowToAppShelfIdMap::iterator iter =
           window_to_app_shelf_id_map_.begin();
       iter != window_to_app_shelf_id_map_.end();
       ++iter) {
    iter->first->RemoveObserver(this);
  }
}

void AppWindowLauncherController::AdditionalUserAddedToSession(
    Profile* profile) {
  // TODO(skuhne): This was added for the legacy side by side mode in M32. If
  // this mode gets no longer pursued this special case can be removed.
  if (chrome::MultiUserWindowManager::GetMultiProfileMode() !=
      chrome::MultiUserWindowManager::MULTI_PROFILE_MODE_MIXED)
    return;

  apps::AppWindowRegistry* registry = apps::AppWindowRegistry::Get(profile);
  if (registry_.find(registry) != registry_.end())
    return;

  registry->AddObserver(this);
  registry_.insert(registry);
}

void AppWindowLauncherController::OnAppWindowIconChanged(
    AppWindow* app_window) {
  if (!ControlsWindow(app_window->GetNativeWindow()))
    return;

  const std::string app_shelf_id = GetAppShelfId(app_window);
  AppControllerMap::iterator iter = app_controller_map_.find(app_shelf_id);
  if (iter == app_controller_map_.end())
    return;
  AppWindowLauncherItemController* controller = iter->second;
  controller->set_image_set_by_controller(true);
  owner_->SetLauncherItemImage(controller->shelf_id(),
                               app_window->app_icon().AsImageSkia());
}

void AppWindowLauncherController::OnAppWindowShown(AppWindow* app_window) {
  aura::Window* window = app_window->GetNativeWindow();
  if (!ControlsWindow(window))
    return;

  if (!IsRegisteredApp(window))
    RegisterApp(app_window);
}

void AppWindowLauncherController::OnAppWindowHidden(AppWindow* app_window) {
  aura::Window* window = app_window->GetNativeWindow();
  if (!ControlsWindow(window))
    return;

  if (IsRegisteredApp(window))
    UnregisterApp(window);
}

// Called from aura::Window::~Window(), before delegate_->OnWindowDestroyed()
// which destroys AppWindow, so both |window| and the associated AppWindow
// are valid here.
void AppWindowLauncherController::OnWindowDestroying(aura::Window* window) {
  if (!ControlsWindow(window))
    return;
  UnregisterApp(window);
}

void AppWindowLauncherController::OnWindowActivated(aura::Window* new_active,
                                                    aura::Window* old_active) {
  // Make the newly active window the active (first) entry in the controller.
  AppWindowLauncherItemController* new_controller =
      ControllerForWindow(new_active);
  if (new_controller) {
    new_controller->SetActiveWindow(new_active);
    owner_->SetItemStatus(new_controller->shelf_id(), ash::STATUS_ACTIVE);
  }

  // Mark the old active window's launcher item as running (if different).
  AppWindowLauncherItemController* old_controller =
      ControllerForWindow(old_active);
  if (old_controller && old_controller != new_controller)
    owner_->SetItemStatus(old_controller->shelf_id(), ash::STATUS_RUNNING);
}

void AppWindowLauncherController::RegisterApp(AppWindow* app_window) {
  aura::Window* window = app_window->GetNativeWindow();
  // Get the app's shelf identifier and add an entry to the map.
  DCHECK(window_to_app_shelf_id_map_.find(window) ==
         window_to_app_shelf_id_map_.end());
  const std::string app_shelf_id = GetAppShelfId(app_window);
  window_to_app_shelf_id_map_[window] = app_shelf_id;
  window->AddObserver(this);

  // Find or create an item controller and launcher item.
  std::string app_id = app_window->extension_id();
  ash::ShelfItemStatus status = ash::wm::IsActiveWindow(window)
                                    ? ash::STATUS_ACTIVE
                                    : ash::STATUS_RUNNING;
  AppControllerMap::iterator iter = app_controller_map_.find(app_shelf_id);
  ash::ShelfID shelf_id = 0;
  if (iter != app_controller_map_.end()) {
    AppWindowLauncherItemController* controller = iter->second;
    DCHECK(controller->app_id() == app_id);
    shelf_id = controller->shelf_id();
    controller->AddAppWindow(app_window, status);
  } else {
    LauncherItemController::Type type =
        app_window->window_type_is_panel()
            ? LauncherItemController::TYPE_APP_PANEL
            : LauncherItemController::TYPE_APP;
    AppWindowLauncherItemController* controller =
        new AppWindowLauncherItemController(type, app_shelf_id, app_id, owner_);
    controller->AddAppWindow(app_window, status);
    // If the app shelf id is not unique, and there is already a shelf
    // item for this app id (e.g. pinned), use that shelf item.
    if (app_shelf_id == app_id)
      shelf_id = owner_->GetShelfIDForAppID(app_id);
    if (shelf_id == 0) {
      shelf_id = owner_->CreateAppLauncherItem(controller, app_id, status);
      // Restore any existing app icon and flag as set.
      const gfx::Image& app_icon = app_window->app_icon();
      if (!app_icon.IsEmpty()) {
        owner_->SetLauncherItemImage(shelf_id, app_icon.AsImageSkia());
        controller->set_image_set_by_controller(true);
      }
    } else {
      owner_->SetItemController(shelf_id, controller);
    }
    const std::string app_shelf_id = GetAppShelfId(app_window);
    app_controller_map_[app_shelf_id] = controller;
  }
  owner_->SetItemStatus(shelf_id, status);
  ash::SetShelfIDForWindow(shelf_id, window);
}

void AppWindowLauncherController::UnregisterApp(aura::Window* window) {
  WindowToAppShelfIdMap::iterator iter1 =
      window_to_app_shelf_id_map_.find(window);
  DCHECK(iter1 != window_to_app_shelf_id_map_.end());
  std::string app_shelf_id = iter1->second;
  window_to_app_shelf_id_map_.erase(iter1);
  window->RemoveObserver(this);

  AppControllerMap::iterator iter2 = app_controller_map_.find(app_shelf_id);
  DCHECK(iter2 != app_controller_map_.end());
  AppWindowLauncherItemController* controller = iter2->second;
  controller->RemoveAppWindowForWindow(window);
  if (controller->app_window_count() == 0) {
    // If this is the last window associated with the app shelf id, close the
    // shelf item.
    ash::ShelfID shelf_id = controller->shelf_id();
    owner_->CloseLauncherItem(shelf_id);
    app_controller_map_.erase(iter2);
  }
}

bool AppWindowLauncherController::IsRegisteredApp(aura::Window* window) {
  return window_to_app_shelf_id_map_.find(window) !=
         window_to_app_shelf_id_map_.end();
}

// Private Methods

AppWindowLauncherItemController*
AppWindowLauncherController::ControllerForWindow(aura::Window* window) {
  WindowToAppShelfIdMap::iterator iter1 =
      window_to_app_shelf_id_map_.find(window);
  if (iter1 == window_to_app_shelf_id_map_.end())
    return NULL;
  std::string app_shelf_id = iter1->second;
  AppControllerMap::iterator iter2 = app_controller_map_.find(app_shelf_id);
  if (iter2 == app_controller_map_.end())
    return NULL;
  return iter2->second;
}
