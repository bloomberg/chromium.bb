// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/extension_app_window_launcher_controller.h"

#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shelf/shelf_model.h"
#include "ash/wm/window_util.h"
#include "ash/wm_window.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/extension_app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/common/extension.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"

using extensions::AppWindow;
using extensions::AppWindowRegistry;

namespace {

// Get the ShelfID for a given |app_window|.
ash::ShelfID GetShelfId(AppWindow* app_window) {
  // Set launch_id default value to an empty string. If showInShelf parameter
  // is true or the window type is panel and the window key is not empty, its
  // value is appended to the launch_id. Otherwise, if the window key is
  // empty, the session_id is used.
  std::string launch_id;
  if (app_window->show_in_shelf() || app_window->window_type_is_panel()) {
    if (!app_window->window_key().empty())
      launch_id = app_window->window_key();
    else
      launch_id = base::StringPrintf("%d", app_window->session_id().id());
  }
  return ash::ShelfID(app_window->extension_id(), launch_id);
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
  for (extensions::AppWindowRegistry* iter : registry_)
    iter->RemoveObserver(this);

  for (const auto& iter : window_to_shelf_id_map_)
    iter.first->RemoveObserver(this);
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
  const ash::ShelfID shelf_id = GetShelfId(app_window);
  AppControllerMap::iterator iter = app_controller_map_.find(shelf_id);
  if (iter == app_controller_map_.end())
    return;

  // Check if the window actually overrides its default icon. Otherwise use app
  // icon loader provided by owner.
  if (!app_window->HasCustomIcon() || app_window->app_icon().IsEmpty())
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
  const ash::ShelfID shelf_id = GetShelfId(app_window);
  DCHECK(!shelf_id.IsNull());
  aura::Window* window = app_window->GetNativeWindow();
  window->SetProperty(ash::kShelfIDKey, new ash::ShelfID(shelf_id));

  // Windows created by IME extension should be treated the same way as the
  // virtual keyboard window, which does not register itself in launcher.
  // Ash's ShelfWindowWatcher handles app panel windows separately.
  if (app_window->is_ime_window() || app_window->window_type_is_panel())
    return;

  // Get the app's shelf identifier and add an entry to the map.
  DCHECK_EQ(window_to_shelf_id_map_.count(window), 0u);
  window_to_shelf_id_map_[window] = shelf_id;
  window->AddObserver(this);

  // Find or create an item controller and launcher item.
  ash::ShelfItemStatus status = ash::wm::IsActiveWindow(window)
                                    ? ash::STATUS_ACTIVE
                                    : ash::STATUS_RUNNING;
  AppControllerMap::iterator app_controller_iter =
      app_controller_map_.find(shelf_id);

  if (app_controller_iter != app_controller_map_.end()) {
    ExtensionAppWindowLauncherItemController* controller =
        app_controller_iter->second;
    DCHECK_EQ(controller->app_id(), shelf_id.app_id);
    controller->AddAppWindow(app_window);
  } else {
    std::unique_ptr<ExtensionAppWindowLauncherItemController> controller =
        base::MakeUnique<ExtensionAppWindowLauncherItemController>(shelf_id);
    app_controller_map_[shelf_id] = controller.get();
    controller->AddAppWindow(app_window);

    // Check for any existing pinned shelf item with a matching |shelf_id|.
    if (owner()->GetItem(shelf_id) == nullptr) {
      owner()->CreateAppLauncherItem(std::move(controller), status);
      // Restore any existing app icon and flag as set.
      if (app_window->HasCustomIcon() && !app_window->app_icon().IsEmpty()) {
        owner()->SetLauncherItemImage(shelf_id,
                                      app_window->app_icon().AsImageSkia());
        app_controller_map_[shelf_id]->set_image_set_by_controller(true);
      }
    } else {
      owner()->shelf_model()->SetShelfItemDelegate(shelf_id,
                                                   std::move(controller));
    }
  }

  owner()->SetItemStatus(shelf_id, status);
}

void ExtensionAppWindowLauncherController::UnregisterApp(aura::Window* window) {
  const auto window_iter = window_to_shelf_id_map_.find(window);
  DCHECK(window_iter != window_to_shelf_id_map_.end());
  ash::ShelfID shelf_id = window_iter->second;
  window_to_shelf_id_map_.erase(window_iter);
  window->RemoveObserver(this);

  AppControllerMap::iterator app_controller_iter =
      app_controller_map_.find(shelf_id);
  DCHECK(app_controller_iter != app_controller_map_.end());
  ExtensionAppWindowLauncherItemController* controller;
  controller = app_controller_iter->second;

  controller->RemoveWindow(controller->GetAppWindow(window));
  if (controller->window_count() == 0) {
    // If this is the last window associated with the app window shelf id,
    // close the shelf item.
    owner()->CloseLauncherItem(shelf_id);
    app_controller_map_.erase(app_controller_iter);
  }
}

bool ExtensionAppWindowLauncherController::IsRegisteredApp(
    aura::Window* window) {
  return window_to_shelf_id_map_.find(window) != window_to_shelf_id_map_.end();
}

// Private Methods

AppWindowLauncherItemController*
ExtensionAppWindowLauncherController::ControllerForWindow(
    aura::Window* window) {
  const auto window_iter = window_to_shelf_id_map_.find(window);
  if (window_iter == window_to_shelf_id_map_.end())
    return nullptr;
  const auto controller_iter = app_controller_map_.find(window_iter->second);
  if (controller_iter == app_controller_map_.end())
    return nullptr;
  return controller_iter->second;
}
