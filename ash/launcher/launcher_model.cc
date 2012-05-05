// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_model.h"

#include "ash/launcher/launcher_model_observer.h"
#include "ui/aura/window.h"

namespace ash {

LauncherModel::LauncherModel() : next_id_(1) {
  LauncherItem app_list;
  app_list.type = TYPE_APP_LIST;
  app_list.is_incognito = false;

  LauncherItem browser_shortcut;
  browser_shortcut.type = TYPE_BROWSER_SHORTCUT;
  browser_shortcut.is_incognito = false;

  AddAt(0, browser_shortcut);
  AddAt(1, app_list);
}

LauncherModel::~LauncherModel() {
}

int LauncherModel::Add(const LauncherItem& item) {
  return AddAt(GetIndexToAddItemAt(item.type), item);
}

void LauncherModel::RemoveItemAt(int index) {
  DCHECK(index >= 0 && index < item_count());
  // The app list and browser shortcut can't be removed.
  DCHECK(items_[index].type != TYPE_APP_LIST &&
         items_[index].type != TYPE_BROWSER_SHORTCUT);
  LauncherID id = items_[index].id;
  items_.erase(items_.begin() + index);
  FOR_EACH_OBSERVER(LauncherModelObserver, observers_,
                    LauncherItemRemoved(index, id));
}

void LauncherModel::Move(int index, int target_index) {
  if (index == target_index)
    return;
  // TODO: this needs to enforce valid ranges.
  LauncherItem item(items_[index]);
  items_.erase(items_.begin() + index);
  items_.insert(items_.begin() + target_index, item);
  FOR_EACH_OBSERVER(LauncherModelObserver, observers_,
                    LauncherItemMoved(index, target_index));
}

void LauncherModel::Set(int index, const LauncherItem& item) {
  DCHECK(index >= 0 && index < item_count());
  LauncherItem old_item(items_[index]);
  items_[index] = item;
  items_[index].id = old_item.id;
  items_[index].type = old_item.type;
  FOR_EACH_OBSERVER(LauncherModelObserver, observers_,
                    LauncherItemChanged(index, old_item));
}

int LauncherModel::ItemIndexByID(LauncherID id) {
  LauncherItems::const_iterator i = ItemByID(id);
  return i == items_.end() ? -1 : static_cast<int>(i - items_.begin());
}

LauncherItems::const_iterator LauncherModel::ItemByID(int id) const {
  for (LauncherItems::const_iterator i = items_.begin();
       i != items_.end(); ++i) {
    if (i->id == id)
      return i;
  }
  return items_.end();
}

void LauncherModel::AddObserver(LauncherModelObserver* observer) {
  observers_.AddObserver(observer);
}

void LauncherModel::RemoveObserver(LauncherModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

int LauncherModel::AddAt(int index, const LauncherItem& item) {
  DCHECK(index >= 0 && index <= item_count());
  items_.insert(items_.begin() + index, item);
  items_[index].id = next_id_++;
  FOR_EACH_OBSERVER(LauncherModelObserver, observers_,
                    LauncherItemAdded(index));
  return index;
}

int LauncherModel::GetIndexToAddItemAt(LauncherItemType type) const {
  DCHECK_GE(item_count(), 2);  // APP_LIST and BROWSER_SHORTCUT.
  if (type == TYPE_APP_SHORTCUT) {
    // APP_SHORTCUTS go after TYPE_BROWSER_SHORTCUT, but before everything else.
    for (int i = 1; i < item_count() - 1; ++i) {
      if (items_[i].type != TYPE_APP_SHORTCUT)
        return i;
    }
  }

  // The rest go before TYPE_APP_LIST.
  return item_count() - 1;
}

}  // namespace ash
