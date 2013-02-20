// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/shell_window_launcher_controller.h"

#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/shell_window_launcher_item_controller.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "ui/aura/client/activation_client.h"

namespace {

std::string GetAppLauncherId(ShellWindow* shell_window) {
  if (shell_window->window_type_is_panel())
    return StringPrintf("panel:%d", shell_window->session_id().id());
  return shell_window->extension()->id();
}

}  // namespace

ShellWindowLauncherController::ShellWindowLauncherController(
    ChromeLauncherController* owner)
    : owner_(owner),
      registry_(extensions::ShellWindowRegistry::Get(owner->profile())),
      activation_client_(NULL) {
  registry_->AddObserver(this);
  if (ash::Shell::HasInstance()) {
    if (ash::Shell::GetInstance()->GetPrimaryRootWindow()) {
      activation_client_ = aura::client::GetActivationClient(
          ash::Shell::GetInstance()->GetPrimaryRootWindow());
      if (activation_client_)
        activation_client_->AddObserver(this);
    }
  }
}


ShellWindowLauncherController::~ShellWindowLauncherController() {
  registry_->RemoveObserver(this);
  if (activation_client_)
    activation_client_->RemoveObserver(this);
  for (WindowToAppLauncherIdMap::iterator iter =
           window_to_app_launcher_id_map_.begin();
       iter != window_to_app_launcher_id_map_.end(); ++iter) {
    iter->first->RemoveObserver(this);
  }
  STLDeleteContainerPairSecondPointers(
      app_controller_map_.begin(), app_controller_map_.end());
}

void ShellWindowLauncherController::OnShellWindowAdded(
    ShellWindow* shell_window) {
  aura::Window* window = shell_window->GetNativeWindow();
  // Get the app's launcher identifier and add an entry to the map.
  DCHECK(window_to_app_launcher_id_map_.find(window) ==
         window_to_app_launcher_id_map_.end());
  const std::string app_launcher_id = GetAppLauncherId(shell_window);
  window_to_app_launcher_id_map_[window] = app_launcher_id;
  window->AddObserver(this);

  // Find or create an item controller and launcher item.
  std::string app_id = shell_window->extension()->id();
  ash::LauncherItemStatus status = ash::wm::IsActiveWindow(window) ?
      ash::STATUS_ACTIVE : ash::STATUS_RUNNING;
  AppControllerMap::iterator iter = app_controller_map_.find(app_launcher_id);
  ash::LauncherID launcher_id = 0;
  if (iter != app_controller_map_.end()) {
    ShellWindowLauncherItemController* controller = iter->second;
    DCHECK(controller->app_id() == app_id);
    launcher_id = controller->launcher_id();
    controller->AddShellWindow(shell_window, status);
  } else {
    LauncherItemController::Type type = shell_window->window_type_is_panel()
        ? LauncherItemController::TYPE_APP_PANEL
        : LauncherItemController::TYPE_APP;
    ShellWindowLauncherItemController* controller =
        new ShellWindowLauncherItemController(
            type, app_launcher_id, app_id, owner_);
    controller->AddShellWindow(shell_window, status);
    // If the app launcher id is not unique, and there is already a launcher
    // item for this app id (e.g. pinned), use that launcher item.
    if (app_launcher_id == app_id)
      launcher_id = owner_->GetLauncherIDForAppID(app_id);
    if (launcher_id == 0)
      launcher_id = owner_->CreateAppLauncherItem(controller, app_id, status);
    else
      owner_->SetItemController(launcher_id, controller);
    const std::string app_launcher_id = GetAppLauncherId(shell_window);
    app_controller_map_[app_launcher_id] = controller;
  }
  owner_->SetItemStatus(launcher_id, status);
}

void ShellWindowLauncherController::OnShellWindowIconChanged(
    ShellWindow* shell_window) {
  const std::string app_launcher_id = GetAppLauncherId(shell_window);
  AppControllerMap::iterator iter = app_controller_map_.find(app_launcher_id);
  if (iter == app_controller_map_.end())
    return;
  ShellWindowLauncherItemController* controller = iter->second;
  owner_->SetLauncherItemImage(controller->launcher_id(),
                               shell_window->app_icon().AsImageSkia());
}

void ShellWindowLauncherController::OnShellWindowRemoved(
    ShellWindow* shell_window) {
  // Do nothing here; shell_window->window() has allready been deleted and
  // OnWindowDestroying() has been called, doing the removal.
}

// Called from ~aura::Window(), before delegate_->OnWindowDestroyed() which
// destroys ShellWindow, so both |window| and the associated ShellWindow
// are valid here.
void ShellWindowLauncherController::OnWindowDestroying(aura::Window* window) {
  WindowToAppLauncherIdMap::iterator iter1 =
      window_to_app_launcher_id_map_.find(window);
  DCHECK(iter1 != window_to_app_launcher_id_map_.end());
  std::string app_launcher_id = iter1->second;
  window_to_app_launcher_id_map_.erase(iter1);
  window->RemoveObserver(this);

  AppControllerMap::iterator iter2 = app_controller_map_.find(app_launcher_id);
  DCHECK(iter2 != app_controller_map_.end());
  ShellWindowLauncherItemController* controller = iter2->second;
  controller->RemoveShellWindowForWindow(window);
  if (controller->shell_window_count() == 0) {
    // If this is the last window associated with the app launcher id, close the
    // launcher item.
    ash::LauncherID launcher_id = controller->launcher_id();
    owner_->CloseLauncherItem(launcher_id);
    app_controller_map_.erase(iter2);
    delete controller;
  }
}

void ShellWindowLauncherController::OnWindowActivated(
    aura::Window* new_active,
    aura::Window* old_active) {
  // Make the newly active window the active (first) entry in the controller.
  ShellWindowLauncherItemController* new_controller =
      ControllerForWindow(new_active);
  if (new_controller) {
    new_controller->SetActiveWindow(new_active);
    owner_->SetItemStatus(new_controller->launcher_id(), ash::STATUS_ACTIVE);
  }

  // Mark the old active window's launcher item as running (if different).
  ShellWindowLauncherItemController* old_controller =
      ControllerForWindow(old_active);
  if (old_controller && old_controller != new_controller)
    owner_->SetItemStatus(old_controller->launcher_id(), ash::STATUS_RUNNING);
}

// Private Methods

ShellWindowLauncherItemController*
ShellWindowLauncherController::ControllerForWindow(
    aura::Window* window) {
  WindowToAppLauncherIdMap::iterator iter1 =
      window_to_app_launcher_id_map_.find(window);
  if (iter1 == window_to_app_launcher_id_map_.end())
    return NULL;
  std::string app_launcher_id = iter1->second;
  AppControllerMap::iterator iter2 = app_controller_map_.find(app_launcher_id);
  if (iter2 == app_controller_map_.end())
    return NULL;
  return iter2->second;
}
