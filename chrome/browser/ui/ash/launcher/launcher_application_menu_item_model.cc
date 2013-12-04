// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_application_menu_item_model.h"

#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item.h"

LauncherApplicationMenuItemModel::LauncherApplicationMenuItemModel(
    ChromeLauncherAppMenuItems item_list)
    : ash::ShelfMenuModel(this),
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

void LauncherApplicationMenuItemModel::ExecuteCommand(int command_id,
                                                      int event_flags) {
  DCHECK(command_id < static_cast<int>(launcher_items_.size()));
  launcher_items_[command_id]->Execute(event_flags);
}

void LauncherApplicationMenuItemModel::Build() {
  AddSeparator(ui::SPACING_SEPARATOR);
  for (size_t i = 0; i < launcher_items_.size(); i++) {
    ChromeLauncherAppMenuItem* item = launcher_items_[i];

    // Check for a separator requirement in front of this item.
    if (item->HasLeadingSeparator())
      AddSeparator(ui::SPACING_SEPARATOR);

    // The first item is the context menu, the others are the running apps.
    AddItem(i, item->title());

    if (!item->icon().IsEmpty())
      SetIcon(GetIndexOfCommandId(i), item->icon());
  }
  RemoveTrailingSeparators();

  // Adding final spacing (if the menu is not empty) to conform the menu to our
  // style.
  if (launcher_items_.size())
    AddSeparator(ui::SPACING_SEPARATOR);
}

