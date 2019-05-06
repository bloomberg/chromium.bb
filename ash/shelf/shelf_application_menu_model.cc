// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_application_menu_model.h"

#include <stddef.h>

#include <limits>
#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/shell.h"
#include "base/metrics/histogram_macros.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/image/image.h"

namespace {

const int kInvalidCommandId = std::numeric_limits<int>::max();

}  // namespace

namespace ash {

ShelfApplicationMenuModel::ShelfApplicationMenuModel(
    const base::string16& title,
    std::vector<mojom::MenuItemPtr> items,
    ShelfItemDelegate* delegate)
    : ui::SimpleMenuModel(this), items_(std::move(items)), delegate_(delegate) {
  AddItem(kInvalidCommandId, title);

  for (size_t i = 0; i < items_.size(); i++) {
    mojom::MenuItem* item = items_[i].get();
    AddItem(i, item->label);
    if (!item->image.isNull())
      SetIcon(GetIndexOfCommandId(i), gfx::Image(item->image));
  }

  // SimpleMenuModel does not allow two consecutive spacing separator items.
  // This only occurs in tests; users should not see menus with no |items_|.
  if (!items_.empty())
    AddSeparator(ui::SPACING_SEPARATOR);
}

ShelfApplicationMenuModel::~ShelfApplicationMenuModel() = default;

bool ShelfApplicationMenuModel::IsCommandIdChecked(int command_id) const {
  return false;
}

bool ShelfApplicationMenuModel::IsCommandIdEnabled(int command_id) const {
  return command_id >= 0 && static_cast<size_t>(command_id) < items_.size();
}

void ShelfApplicationMenuModel::ExecuteCommand(int command_id,
                                               int event_flags) {
  DCHECK(IsCommandIdEnabled(command_id));
  // Have the delegate execute its own custom command id for the given item.
  if (delegate_) {
    if (Shell::Get()->app_list_controller()) {
      // Record app launch when selecting window to open from disambiguation
      // menu.
      Shell::Get()->app_list_controller()->RecordShelfAppLaunched(
          base::nullopt /* recorded_app_list_view_state */,
          base::nullopt /* recorded_home_launcher_shown */);
    }

    // The display hosting the menu is irrelevant, windows activate in-place.
    delegate_->ExecuteCommand(false, items_[command_id]->command_id,
                              event_flags, display::kInvalidDisplayId);
  }
  RecordMenuItemSelectedMetrics(command_id, items_.size());
}

void ShelfApplicationMenuModel::RecordMenuItemSelectedMetrics(
    int command_id,
    int num_menu_items_enabled) {
  UMA_HISTOGRAM_COUNTS_100("Ash.Shelf.Menu.SelectedMenuItemIndex", command_id);
  UMA_HISTOGRAM_COUNTS_100("Ash.Shelf.Menu.NumItemsEnabledUponSelection",
                           num_menu_items_enabled);
}

}  // namespace ash
