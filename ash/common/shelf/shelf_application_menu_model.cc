// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/shelf_application_menu_model.h"

#include <stddef.h>

#include <limits>
#include <utility>

#include "ash/common/shelf/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_application_menu_item.h"
#include "base/metrics/histogram_macros.h"

namespace {

const int kInvalidCommandId = std::numeric_limits<int>::max();

}  // namespace

namespace ash {

ShelfApplicationMenuModel::ShelfApplicationMenuModel(
    const base::string16& title,
    ShelfAppMenuItemList items,
    ShelfItemDelegate* delegate)
    : ui::SimpleMenuModel(this), items_(std::move(items)), delegate_(delegate) {
  AddSeparator(ui::SPACING_SEPARATOR);
  AddItem(kInvalidCommandId, title);
  AddSeparator(ui::SPACING_SEPARATOR);

  for (size_t i = 0; i < items_.size(); i++) {
    ShelfApplicationMenuItem* item = items_[i].get();
    AddItem(i, item->title());
    if (!item->icon().IsEmpty())
      SetIcon(GetIndexOfCommandId(i), item->icon());
  }

  // SimpleMenuModel does not allow two consecutive spacing separator items.
  // This only occurs in tests; users should not see menus with no |items_|.
  if (!items_.empty())
    AddSeparator(ui::SPACING_SEPARATOR);
}

ShelfApplicationMenuModel::~ShelfApplicationMenuModel() {}

bool ShelfApplicationMenuModel::IsCommandIdChecked(int command_id) const {
  return false;
}

bool ShelfApplicationMenuModel::IsCommandIdEnabled(int command_id) const {
  return command_id >= 0 && static_cast<size_t>(command_id) < items_.size();
}

void ShelfApplicationMenuModel::ExecuteCommand(int command_id,
                                               int event_flags) {
  DCHECK(delegate_);
  DCHECK(IsCommandIdEnabled(command_id));
  // Have the delegate execute its own custom command id for the given item.
  delegate_->ExecuteCommand(items_[command_id]->command_id(), event_flags);
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
