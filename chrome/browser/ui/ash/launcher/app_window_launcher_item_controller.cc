// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"

#include <algorithm>

#include "ash/wm/window_util.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/base_window.h"
#include "ui/wm/core/window_animations.h"

AppWindowLauncherItemController::AppWindowLauncherItemController(
    Type type,
    const std::string& app_shelf_id,
    const std::string& app_id,
    ChromeLauncherController* controller)
    : LauncherItemController(type, app_id, controller),
      app_shelf_id_(app_shelf_id),
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

bool AppWindowLauncherItemController::IsOpen() const {
  return !windows_.empty();
}

bool AppWindowLauncherItemController::IsVisible() const {
  // Return true if any windows are visible.
  for (const auto& window : windows_) {
    if (window->GetNativeWindow()->IsVisible())
      return true;
  }
  return false;
}

void AppWindowLauncherItemController::Launch(ash::LaunchSource source,
                                             int event_flags) {
  launcher_controller()->LaunchApp(app_id(), source, ui::EF_NONE);
}

ash::ShelfItemDelegate::PerformedAction
AppWindowLauncherItemController::Activate(ash::LaunchSource source) {
  DCHECK(!windows_.empty());
  ui::BaseWindow* window_to_activate =
      last_active_window_ ? last_active_window_ : windows_.back();
  window_to_activate->Activate();
  return kExistingWindowActivated;
}

void AppWindowLauncherItemController::Close() {
  // Note: Closing windows may affect the contents of app_windows_.
  WindowList windows_to_close = windows_;
  for (const auto& window : windows_)
    window->Close();
}

void AppWindowLauncherItemController::ActivateIndexedApp(size_t index) {
  if (index >= windows_.size())
    return;
  auto it = windows_.begin();
  std::advance(it, index);
  ShowAndActivateOrMinimize(*it);
}

ChromeLauncherAppMenuItems AppWindowLauncherItemController::GetApplicationList(
    int event_flags) {
  ChromeLauncherAppMenuItems items;
  items.push_back(new ChromeLauncherAppMenuItem(GetTitle(), NULL, false));
  return items;
}

ash::ShelfItemDelegate::PerformedAction
AppWindowLauncherItemController::ItemSelected(const ui::Event& event) {
  if (windows_.empty())
    return kNoAction;

  DCHECK_EQ(TYPE_APP, type());
  ui::BaseWindow* window_to_show =
      last_active_window_ ? last_active_window_ : windows_.front();
  // If the event was triggered by a keystroke, we try to advance to the next
  // item if the window we are trying to activate is already active.
  if (windows_.size() >= 1 && window_to_show->IsActive() &&
      event.type() == ui::ET_KEY_RELEASED) {
    return ActivateOrAdvanceToNextAppWindow(window_to_show);
  } else {
    return ShowAndActivateOrMinimize(window_to_show);
  }
}

base::string16 AppWindowLauncherItemController::GetTitle() {
  return GetAppTitle();
}

bool AppWindowLauncherItemController::IsDraggable() {
  DCHECK_EQ(TYPE_APP, type());
  return CanPin();
}

bool AppWindowLauncherItemController::CanPin() const {
  return launcher_controller()->CanPin(app_id());
}

bool AppWindowLauncherItemController::ShouldShowTooltip() {
  DCHECK_EQ(TYPE_APP, type());
  return true;
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

ash::ShelfItemDelegate::PerformedAction
AppWindowLauncherItemController::ShowAndActivateOrMinimize(
    ui::BaseWindow* app_window) {
  // Either show or minimize windows when shown from the launcher.
  return launcher_controller()->ActivateWindowOrMinimizeIfActive(
      app_window, GetApplicationList(0).size() == 2);
}

ash::ShelfItemDelegate::PerformedAction
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
  return kNoAction;
}
