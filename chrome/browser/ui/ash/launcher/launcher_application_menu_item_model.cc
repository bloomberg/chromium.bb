// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_application_menu_item_model.h"

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_per_app.h"

LauncherApplicationMenuItemModel::LauncherApplicationMenuItemModel(
    ChromeLauncherAppMenuItems item_list)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ash::LauncherMenuModel(this)),
      launcher_items_(item_list.Pass()) {
  Build();
}

LauncherApplicationMenuItemModel::~LauncherApplicationMenuItemModel() {
}

bool LauncherApplicationMenuItemModel::IsCommandActive(int command_id) const {
  DCHECK(command_id >= 0);
  DCHECK(static_cast<size_t>(command_id) < launcher_items_.size());
  return launcher_items_[command_id]->IsActive();
}

bool LauncherApplicationMenuItemModel::IsCommandIdChecked(
    int command_id) const {
  return false;
}

bool LauncherApplicationMenuItemModel::IsCommandIdEnabled(
    int command_id) const {
  DCHECK(command_id < static_cast<int>(launcher_items_.size()));
  return launcher_items_[command_id]->IsEnabled();
}

bool LauncherApplicationMenuItemModel::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void LauncherApplicationMenuItemModel::ExecuteCommand(int command_id) {
  DCHECK(command_id < static_cast<int>(launcher_items_.size()));
  launcher_items_[command_id]->Execute();
}

void LauncherApplicationMenuItemModel::Build() {
  AddSeparator(ui::SPACING_SEPARATOR);
  for (size_t i = 0; i < launcher_items_.size(); i++) {
    ChromeLauncherAppMenuItem* item = launcher_items_[i];
    // The first item is the context menu, the others are the running apps.
    AddItem(i, item->title());

    if (!item->icon().IsEmpty())
      SetIcon(GetIndexOfCommandId(i), item->icon());
    // The first item is most likely the application name which should get
    // separated from the rest.
    if (i == 0)
      AddSeparator(ui::NORMAL_SEPARATOR);
  }
  RemoveTrailingSeparators();
}

