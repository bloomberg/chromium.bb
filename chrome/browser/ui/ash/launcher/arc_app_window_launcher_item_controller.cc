// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_item_controller.h"

#include <utility>

#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"
#include "ui/aura/window.h"
#include "ui/base/base_window.h"

ArcAppWindowLauncherItemController::ArcAppWindowLauncherItemController(
    const std::string& arc_app_id)
    : AppWindowLauncherItemController(ash::ShelfID(arc_app_id)) {}

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

void ArcAppWindowLauncherItemController::ItemSelected(
    std::unique_ptr<ui::Event> event,
    int64_t display_id,
    ash::ShelfLaunchSource source,
    ItemSelectedCallback callback) {
  if (window_count()) {
    AppWindowLauncherItemController::ItemSelected(std::move(event), display_id,
                                                  source, std::move(callback));
    return;
  }

  if (task_ids_.empty()) {
    NOTREACHED();
    std::move(callback).Run(ash::SHELF_ACTION_NONE, base::nullopt);
    return;
  }
  arc::SetTaskActive(*task_ids_.begin());
  std::move(callback).Run(ash::SHELF_ACTION_NEW_WINDOW_CREATED, base::nullopt);
}

ash::MenuItemList ArcAppWindowLauncherItemController::GetAppMenuItems(
    int event_flags) {
  ash::MenuItemList items;
  base::string16 app_title = LauncherControllerHelper::GetAppTitle(
      ChromeLauncherController::instance()->profile(), app_id());
  for (auto it = windows().begin(); it != windows().end(); ++it) {
    // TODO(khmel): resolve correct icon here.
    size_t i = std::distance(windows().begin(), it);
    aura::Window* window = (*it)->GetNativeWindow();
    ash::mojom::MenuItemPtr item = ash::mojom::MenuItem::New();
    item->command_id = base::checked_cast<uint32_t>(i);
    item->label = (window && !window->GetTitle().empty()) ? window->GetTitle()
                                                          : app_title;
    items.push_back(std::move(item));
  }

  return items;
}

void ArcAppWindowLauncherItemController::ExecuteCommand(bool from_context_menu,
                                                        int64_t command_id,
                                                        int32_t event_flags,
                                                        int64_t display_id) {
  if (from_context_menu && ExecuteContextMenuCommand(command_id, event_flags))
    return;

  ActivateIndexedApp(command_id);
}
