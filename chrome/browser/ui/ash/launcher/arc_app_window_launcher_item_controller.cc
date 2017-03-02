// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_item_controller.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"
#include "ui/aura/window.h"
#include "ui/base/base_window.h"

ArcAppWindowLauncherItemController::ArcAppWindowLauncherItemController(
    const std::string& arc_app_id,
    ChromeLauncherController* controller)
    : AppWindowLauncherItemController(arc_app_id, std::string(), controller) {}

ArcAppWindowLauncherItemController::~ArcAppWindowLauncherItemController() {}

void ArcAppWindowLauncherItemController::AddTaskId(int task_id) {
  task_ids_.insert(task_id);
}

void ArcAppWindowLauncherItemController::RemoveTaskId(int task_id) {
  task_ids_.erase(task_id);
}

bool ArcAppWindowLauncherItemController::HasAnyTasks() const {
  return !task_ids_.empty();
}

ash::ShelfAction ArcAppWindowLauncherItemController::ItemSelected(
    ui::EventType event_type,
    int event_flags,
    int64_t display_id,
    ash::ShelfLaunchSource source) {
  if (window_count()) {
    return AppWindowLauncherItemController::ItemSelected(
        event_type, event_flags, display_id, source);
  }

  if (task_ids_.empty()) {
    NOTREACHED();
    return ash::SHELF_ACTION_NONE;
  }
  arc::SetTaskActive(*task_ids_.begin());
  return ash::SHELF_ACTION_NEW_WINDOW_CREATED;
}

ash::ShelfAppMenuItemList ArcAppWindowLauncherItemController::GetAppMenuItems(
    int event_flags) {
  ash::ShelfAppMenuItemList items;
  base::string16 app_title = LauncherControllerHelper::GetAppTitle(
      launcher_controller()->profile(), app_id());
  for (auto it = windows().begin(); it != windows().end(); ++it) {
    // TODO(khmel): resolve correct icon here.
    size_t i = std::distance(windows().begin(), it);
    gfx::Image image;
    aura::Window* window = (*it)->GetNativeWindow();
    items.push_back(base::MakeUnique<ash::ShelfApplicationMenuItem>(
        base::checked_cast<uint32_t>(i),
        ((window && !window->GetTitle().empty()) ? window->GetTitle()
                                                 : app_title),
        &image));
  }
  return items;
}
