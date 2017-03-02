// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"

#include <algorithm>

#include "ash/wm/window_util.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/base_window.h"
#include "ui/wm/core/window_animations.h"

AppWindowLauncherItemController::AppWindowLauncherItemController(
    const std::string& app_id,
    const std::string& launch_id,
    ChromeLauncherController* controller)
    : LauncherItemController(app_id, launch_id, controller),
      observed_windows_(this) {}

AppWindowLauncherItemController::~AppWindowLauncherItemController() {}

void AppWindowLauncherItemController::AddWindow(ui::BaseWindow* app_window) {
  windows_.push_front(app_window);
  aura::Window* window = app_window->GetNativeWindow();
  if (window)
    observed_windows_.Add(window);
}

AppWindowLauncherItemController::WindowList::iterator
AppWindowLauncherItemController::GetFromNativeWindow(aura::Window* window) {
  return std::find_if(windows_.begin(), windows_.end(),
                      [window](ui::BaseWindow* base_window) {
                        return base_window->GetNativeWindow() == window;
                      });
}

void AppWindowLauncherItemController::RemoveWindow(ui::BaseWindow* app_window) {
  DCHECK(app_window);
  aura::Window* window = app_window->GetNativeWindow();
  if (window)
    observed_windows_.Remove(window);
  if (app_window == last_active_window_)
    last_active_window_ = nullptr;
  auto iter = std::find(windows_.begin(), windows_.end(), app_window);
  if (iter == windows_.end()) {
    NOTREACHED();
    return;
  }
  OnWindowRemoved(app_window);
  windows_.erase(iter);
}

ui::BaseWindow* AppWindowLauncherItemController::GetAppWindow(
    aura::Window* window) {
  const auto iter = GetFromNativeWindow(window);
  if (iter != windows_.end())
    return *iter;
  return nullptr;
}

void AppWindowLauncherItemController::SetActiveWindow(aura::Window* window) {
  ui::BaseWindow* app_window = GetAppWindow(window);
  if (app_window)
    last_active_window_ = app_window;
}

AppWindowLauncherItemController*
AppWindowLauncherItemController::AsAppWindowLauncherItemController() {
  return this;
}

ash::ShelfAction AppWindowLauncherItemController::ItemSelected(
    ui::EventType event_type,
    int event_flags,
    int64_t display_id,
    ash::ShelfLaunchSource source) {
  if (windows_.empty())
    return ash::SHELF_ACTION_NONE;

  ui::BaseWindow* window_to_show =
      last_active_window_ ? last_active_window_ : windows_.front();
  // If the event was triggered by a keystroke, we try to advance to the next
  // item if the window we are trying to activate is already active.
  if (windows_.size() >= 1 && window_to_show->IsActive() &&
      event_type == ui::ET_KEY_RELEASED) {
    return ActivateOrAdvanceToNextAppWindow(window_to_show);
  }

  return ShowAndActivateOrMinimize(window_to_show);
}

ash::ShelfAppMenuItemList AppWindowLauncherItemController::GetAppMenuItems(
    int event_flags) {
  // Return an empty item list to avoid showing an application menu.
  return ash::ShelfAppMenuItemList();
}

void AppWindowLauncherItemController::ExecuteCommand(uint32_t command_id,
                                                     int event_flags) {
  // This delegate does not support showing an application menu.
  NOTIMPLEMENTED();
}

void AppWindowLauncherItemController::Close() {
  // Note: Closing windows may affect the contents of app_windows_.
  WindowList windows_to_close = windows_;
  for (auto* window : windows_)
    window->Close();
}

void AppWindowLauncherItemController::ActivateIndexedApp(size_t index) {
  if (index >= windows_.size())
    return;
  auto it = windows_.begin();
  std::advance(it, index);
  ShowAndActivateOrMinimize(*it);
}

void AppWindowLauncherItemController::OnWindowPropertyChanged(
    aura::Window* window,
    const void* key,
    intptr_t old) {
  if (key == aura::client::kDrawAttentionKey) {
    ash::ShelfItemStatus status;
    if (ash::wm::IsActiveWindow(window)) {
      status = ash::STATUS_ACTIVE;
    } else if (window->GetProperty(aura::client::kDrawAttentionKey)) {
      status = ash::STATUS_ATTENTION;
    } else {
      status = ash::STATUS_RUNNING;
    }
    launcher_controller()->SetItemStatus(shelf_id(), status);
  }
}

ash::ShelfAction AppWindowLauncherItemController::ShowAndActivateOrMinimize(
    ui::BaseWindow* app_window) {
  // Either show or minimize windows when shown from the launcher.
  return launcher_controller()->ActivateWindowOrMinimizeIfActive(
      app_window, GetAppMenuItems(0).size() == 1);
}

ash::ShelfAction
AppWindowLauncherItemController::ActivateOrAdvanceToNextAppWindow(
    ui::BaseWindow* window_to_show) {
  WindowList::iterator i(
      std::find(windows_.begin(), windows_.end(), window_to_show));
  if (i != windows_.end()) {
    if (++i != windows_.end())
      window_to_show = *i;
    else
      window_to_show = windows_.front();
  }
  if (window_to_show->IsActive()) {
    // Coming here, only a single window is active. For keyboard activations
    // the window gets animated.
    AnimateWindow(window_to_show->GetNativeWindow(),
                  wm::WINDOW_ANIMATION_TYPE_BOUNCE);
  } else {
    return ShowAndActivateOrMinimize(window_to_show);
  }
  return ash::SHELF_ACTION_NONE;
}
