// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/extension_app_window_launcher_controller.h"

#include "ash/shelf/shelf_util.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/extension_app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/common/extension.h"
#include "ui/aura/window_event_dispatcher.h"

using extensions::AppWindow;
using extensions::AppWindowRegistry;

namespace {

std::string GetAppShelfId(AppWindow* app_window) {
  if (app_window->window_type_is_panel())
    return base::StringPrintf("panel:%d", app_window->session_id().id());
  return app_window->extension_id();
}

}  // namespace

ExtensionAppWindowLauncherController::ExtensionAppWindowLauncherController(
    ChromeLauncherController* owner)
    : AppWindowLauncherController(owner) {
  AppWindowRegistry* registry = AppWindowRegistry::Get(owner->profile());
  registry_.insert(registry);
  registry->AddObserver(this);
}

ExtensionAppWindowLauncherController::~ExtensionAppWindowLauncherController() {
  for (std::set<AppWindowRegistry*>::iterator it = registry_.begin();
       it != registry_.end(); ++it)
    (*it)->RemoveObserver(this);

  for (WindowToAppShelfIdMap::iterator iter =
           window_to_app_shelf_id_map_.begin();
       iter != window_to_app_shelf_id_map_.end(); ++iter) {
    iter->first->RemoveObserver(this);
  }
}

void ExtensionAppWindowLauncherController::AdditionalUserAddedToSession(
    Profile* profile) {
  // TODO(skuhne): This was added for the legacy side by side mode in M32. If
  // this mode gets no longer pursued this special case can be removed.
  if (chrome::MultiUserWindowManager::GetMultiProfileMode() !=
      chrome::MultiUserWindowManager::MULTI_PROFILE_MODE_MIXED)
    return;

  AppWindowRegistry* registry = AppWindowRegistry::Get(profile);
  if (registry_.find(registry) != registry_.end())
    return;

  registry->AddObserver(this);
  registry_.insert(registry);
}

void ExtensionAppWindowLauncherController::OnAppWindowIconChanged(
    AppWindow* app_window) {
  const std::string app_shelf_id = GetAppShelfId(app_window);
  AppControllerMap::iterator iter = app_controller_map_.find(app_shelf_id);
  if (iter == app_controller_map_.end())
    return;
  ExtensionAppWindowLauncherItemController* controller = iter->second;
  controller->set_image_set_by_controller(true);
  owner()->SetLauncherItemImage(controller->shelf_id(),
                                app_window->app_icon().AsImageSkia());
}

void ExtensionAppWindowLauncherController::OnAppWindowShown(
    AppWindow* app_window,
    bool was_hidden) {
  aura::Window* window = app_window->GetNativeWindow();
  if (!IsRegisteredApp(window))
    RegisterApp(app_window);
}

void ExtensionAppWindowLauncherController::OnAppWindowHidden(
    AppWindow* app_window) {
  aura::Window* window = app_window->GetNativeWindow();
  if (IsRegisteredApp(window))
    UnregisterApp(window);
}

// Called from aura::Window::~Window(), before delegate_->OnWindowDestroyed()
// which destroys AppWindow, so both |window| and the associated AppWindow
// are valid here.
void ExtensionAppWindowLauncherController::OnWindowDestroying(
    aura::Window* window) {
  UnregisterApp(window);
}

void ExtensionAppWindowLauncherController::RegisterApp(AppWindow* app_window) {
  // Windows created by IME extension should be treated the same way as the
  // virtual keyboard window, which does not register itself in launcher.
  if (app_window->is_ime_window())
    return;

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
    ExtensionAppWindowLauncherItemController* controller = iter->second;
    DCHECK(controller->app_id() == app_id);
    shelf_id = controller->shelf_id();
    controller->AddAppWindow(app_window);
  } else {
    LauncherItemController::Type type =
        app_window->window_type_is_panel()
            ? LauncherItemController::TYPE_APP_PANEL
            : LauncherItemController::TYPE_APP;
    ExtensionAppWindowLauncherItemController* controller =
        new ExtensionAppWindowLauncherItemController(type, app_shelf_id, app_id,
                                                     owner());
    controller->AddAppWindow(app_window);
    // If the app shelf id is not unique, and there is already a shelf
    // item for this app id (e.g. pinned), use that shelf item.
    if (app_shelf_id == app_id)
      shelf_id = owner()->GetShelfIDForAppID(app_id);
    if (shelf_id == 0) {
      shelf_id = owner()->CreateAppLauncherItem(controller, app_id, status);
      // Restore any existing app icon and flag as set.
      const gfx::Image& app_icon = app_window->app_icon();
      if (!app_icon.IsEmpty()) {
        owner()->SetLauncherItemImage(shelf_id, app_icon.AsImageSkia());
        controller->set_image_set_by_controller(true);
      }
    } else {
      owner()->SetItemController(shelf_id, controller);
    }
    const std::string app_shelf_id = GetAppShelfId(app_window);
    app_controller_map_[app_shelf_id] = controller;
  }
  owner()->SetItemStatus(shelf_id, status);
  ash::SetShelfIDForWindow(shelf_id, window);
}

void ExtensionAppWindowLauncherController::UnregisterApp(aura::Window* window) {
  WindowToAppShelfIdMap::iterator iter1 =
      window_to_app_shelf_id_map_.find(window);
  DCHECK(iter1 != window_to_app_shelf_id_map_.end());
  std::string app_shelf_id = iter1->second;
  window_to_app_shelf_id_map_.erase(iter1);
  window->RemoveObserver(this);

  AppControllerMap::iterator iter2 = app_controller_map_.find(app_shelf_id);
  DCHECK(iter2 != app_controller_map_.end());
  ExtensionAppWindowLauncherItemController* controller = iter2->second;
  controller->RemoveWindow(controller->GetAppWindow(window));
  if (controller->window_count() == 0) {
    // If this is the last window associated with the app shelf id, close the
    // shelf item.
    ash::ShelfID shelf_id = controller->shelf_id();
    owner()->CloseLauncherItem(shelf_id);
    app_controller_map_.erase(iter2);
  }
}

bool ExtensionAppWindowLauncherController::IsRegisteredApp(
    aura::Window* window) {
  return window_to_app_shelf_id_map_.find(window) !=
         window_to_app_shelf_id_map_.end();
}

// Private Methods

AppWindowLauncherItemController*
ExtensionAppWindowLauncherController::ControllerForWindow(
    aura::Window* window) {
  WindowToAppShelfIdMap::iterator iter1 =
      window_to_app_shelf_id_map_.find(window);
  if (iter1 == window_to_app_shelf_id_map_.end())
    return nullptr;
  std::string app_shelf_id = iter1->second;
  AppControllerMap::iterator iter2 = app_controller_map_.find(app_shelf_id);
  if (iter2 == app_controller_map_.end())
    return nullptr;
  return iter2->second;
}
