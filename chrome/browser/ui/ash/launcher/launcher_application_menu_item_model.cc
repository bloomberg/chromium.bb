// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_application_menu_item_model.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item.h"

namespace {

const char kNumItemsEnabledHistogramName[] =
    "Ash.Shelf.Menu.NumItemsEnabledUponSelection";

const char kSelectedMenuItemIndexHistogramName[] =
    "Ash.Shelf.Menu.SelectedMenuItemIndex";

}  // namespace

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
  RecordMenuItemSelectedMetrics(command_id, GetNumMenuItemsEnabled());
}

void LauncherApplicationMenuItemModel::Build() {
  if (launcher_items_.empty())
    return;

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
  AddSeparator(ui::SPACING_SEPARATOR);
}

int LauncherApplicationMenuItemModel::GetNumMenuItemsEnabled() const {
  int num_menu_items_enabled = 0;
  for (const ChromeLauncherAppMenuItem* menu_item : launcher_items_) {
    if (menu_item->IsEnabled())
      ++num_menu_items_enabled;
  }
  return num_menu_items_enabled;
}

void LauncherApplicationMenuItemModel::RecordMenuItemSelectedMetrics(
    int command_id,
    int num_menu_items_enabled) {
  UMA_HISTOGRAM_COUNTS_100(kSelectedMenuItemIndexHistogramName, command_id);
  UMA_HISTOGRAM_COUNTS_100(kNumItemsEnabledHistogramName,
                           num_menu_items_enabled);
}
