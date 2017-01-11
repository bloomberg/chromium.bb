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
#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item_v2app.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_application_menu_item_model.h"
#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"
#include "ui/aura/window.h"
#include "ui/base/base_window.h"

ArcAppWindowLauncherItemController::ArcAppWindowLauncherItemController(
    const std::string& arc_app_id,
    ChromeLauncherController* controller)
    : AppWindowLauncherItemController(arc_app_id, "", controller) {}

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

ash::ShelfItemDelegate::PerformedAction
ArcAppWindowLauncherItemController::ItemSelected(const ui::Event& event) {
  if (window_count()) {
    return AppWindowLauncherItemController::ItemSelected(event);
  } else {
    if (task_ids_.empty()) {
      NOTREACHED();
      return kNoAction;
    }
    arc::SetTaskActive(*task_ids_.begin());
    return kNewWindowCreated;
  }
}

ash::ShelfMenuModel* ArcAppWindowLauncherItemController::CreateApplicationMenu(
    int event_flags) {
  return new LauncherApplicationMenuItemModel(GetApplicationList(event_flags));
}

ChromeLauncherAppMenuItems
ArcAppWindowLauncherItemController::GetApplicationList(int event_flags) {
  ChromeLauncherAppMenuItems items =
      AppWindowLauncherItemController::GetApplicationList(event_flags);
  base::string16 app_title = LauncherControllerHelper::GetAppTitle(
      launcher_controller()->profile(), app_id());
  for (auto it = windows().begin(); it != windows().end(); ++it) {
    // TODO(khmel): resolve correct icon here.
    size_t i = std::distance(windows().begin(), it);
    gfx::Image image;
    aura::Window* window = (*it)->GetNativeWindow();
    items.push_back(base::MakeUnique<ChromeLauncherAppMenuItemV2App>(
        ((window && !window->GetTitle().empty()) ? window->GetTitle()
                                                 : app_title),
        &image, app_id(), launcher_controller(), i,
        i == 0 /* has_leading_separator */));
  }
  return items;
}
