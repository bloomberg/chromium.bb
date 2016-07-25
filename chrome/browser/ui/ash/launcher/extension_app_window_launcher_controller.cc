// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/extension_app_window_launcher_controller.h"

#include "ash/common/shelf/shelf_delegate.h"
#include "ash/common/wm_shell.h"
#include "ash/shelf/shelf_util.h"
#include "ash/wm/window_util.h"
#include "base/stl_util.h"
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
  // Set app_shelf_id default value to extension_id. If showInShelf parameter
  // is true or the window type is panel and the window key is not empty, its
  // value is appended to the app_shelf_id. Otherwise, if the window key is
  // empty, the session_id is used.
  std::string app_shelf_id = app_window->extension_id();
  if (app_window->show_in_shelf() || app_window->window_type_is_panel()) {
    if (!app_window->window_key().empty())
      app_shelf_id += app_window->window_key();
    else
      app_shelf_id += base::StringPrintf("%d", app_window->session_id().id());
  }
  return app_shelf_id;
}

}  // namespace

ExtensionAppWindowLauncherController::ExtensionAppWindowLauncherController(
    ChromeLauncherController* owner)
    : AppWindowLauncherController(owner) {
  AppWindowRegistry* registry = AppWindowRegistry::Get(owner->GetProfile());
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
  AppControllerMap::iterator app_controller_iter =
      app_controller_map_.find(app_shelf_id);
  ash::ShelfID shelf_id = 0;

  if (app_controller_iter != app_controller_map_.end()) {
    ExtensionAppWindowLauncherItemController* controller =
        app_controller_iter->second;
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
    // If there is already a shelf id mapped to this app_shelf_id (e.g. pinned),
    // use that shelf item.
    AppShelfIdToShelfIdMap::iterator app_shelf_id_iter =
        app_shelf_id_to_shelf_id_map_.find(app_shelf_id);
    if (app_shelf_id_iter != app_shelf_id_to_shelf_id_map_.end()) {
      if (owner()->IsPinned(app_shelf_id_iter->second)) {
        shelf_id = app_shelf_id_iter->second;
      } else {
        app_shelf_id_to_shelf_id_map_.erase(app_shelf_id);
      }
    } else if (app_shelf_id == app_id) {
      // show_in_shelf in false and not a panel
      shelf_id =
          ash::WmShell::Get()->shelf_delegate()->GetShelfIDForAppID(app_id);
      // Check if the shelf_id corresponds to an already opened
      // showInShelf=true window that has the same app_id. The current
      // showInShelf=false window should not fold under this shelf item,
      // so the shelf_id is set to 0 to get a new shelf_id.
      auto&& id_map = app_shelf_id_to_shelf_id_map_;
      if (std::find_if(
              id_map.begin(), id_map.end(),
              [shelf_id](const AppShelfIdToShelfIdMap::value_type& pair) {
                return pair.second == shelf_id;
              }) != id_map.end()) {
        shelf_id = 0;
      }
    }

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

    // We need to change the controller associated with app_shelf_id.
    app_controller_map_[app_shelf_id] = controller;
    app_shelf_id_to_shelf_id_map_[app_shelf_id] = shelf_id;
  }
  owner()->SetItemStatus(shelf_id, status);
  ash::SetShelfIDForWindow(shelf_id, window);
}

void ExtensionAppWindowLauncherController::UnregisterApp(aura::Window* window) {
  WindowToAppShelfIdMap::iterator window_iter =
      window_to_app_shelf_id_map_.find(window);
  DCHECK(window_iter != window_to_app_shelf_id_map_.end());
  std::string app_shelf_id = window_iter->second;
  window_to_app_shelf_id_map_.erase(window_iter);
  window->RemoveObserver(this);

  AppControllerMap::iterator app_controller_iter =
      app_controller_map_.find(app_shelf_id);
  DCHECK(app_controller_iter != app_controller_map_.end());
  ExtensionAppWindowLauncherItemController* controller;
  controller = app_controller_iter->second;

  controller->RemoveWindow(controller->GetAppWindow(window));
  if (controller->window_count() == 0) {
    // If this is the last window associated with the app window shelf id,
    // close the shelf item.
    ash::ShelfID shelf_id = controller->shelf_id();
    if (!owner()->IsPinned(shelf_id)) {
      app_shelf_id_to_shelf_id_map_.erase(app_shelf_id);
    }
    owner()->CloseLauncherItem(shelf_id);
    app_controller_map_.erase(app_controller_iter);
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
  WindowToAppShelfIdMap::iterator window_iter =
      window_to_app_shelf_id_map_.find(window);
  if (window_iter == window_to_app_shelf_id_map_.end())
    return nullptr;
  AppControllerMap::iterator app_controller_iter =
      app_controller_map_.find(window_iter->second);
  if (app_controller_iter == app_controller_map_.end())
    return nullptr;
  return app_controller_iter->second;
}
