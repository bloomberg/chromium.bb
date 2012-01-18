// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_model.h"

#include "ui/aura/window.h"
#include "ash/launcher/launcher_model_observer.h"

namespace ash {

LauncherModel::LauncherModel() {
  Add(0, LauncherItem(TYPE_APP_LIST, NULL));
  Add(1, LauncherItem(TYPE_BROWSER_SHORTCUT, NULL));
}

LauncherModel::~LauncherModel() {
}

void LauncherModel::Add(int index, const LauncherItem& item) {
  DCHECK(index >= 0 && index <= item_count());
  items_.insert(items_.begin() + index, item);
  FOR_EACH_OBSERVER(LauncherModelObserver, observers_,
                    LauncherItemAdded(index));
}

void LauncherModel::RemoveItemAt(int index) {
  DCHECK(index >= 0 && index < item_count());
  // The app list and browser shortcut can't be removed.
  DCHECK(items_[index].type != TYPE_APP_LIST &&
         items_[index].type != TYPE_BROWSER_SHORTCUT);
  items_.erase(items_.begin() + index);
  FOR_EACH_OBSERVER(LauncherModelObserver, observers_,
                    LauncherItemRemoved(index));
}

void LauncherModel::Move(int index, int target_index) {
  if (index == target_index)
    return;
  LauncherItem item(items_[index]);
  items_.erase(items_.begin() + index);
  items_.insert(items_.begin() + target_index, item);
  FOR_EACH_OBSERVER(LauncherModelObserver, observers_,
                    LauncherItemMoved(index, target_index));
}

void LauncherModel::Set(int index, const LauncherItem& item) {
  DCHECK(index >= 0 && index < item_count());
  LauncherItemType type = items_[index].type;
  aura::Window* window = items_[index].window;
  items_[index] = item;
  items_[index].type = type;
  items_[index].window = window;
  FOR_EACH_OBSERVER(LauncherModelObserver, observers_,
                    LauncherItemChanged(index));
}

void LauncherModel::SetPendingUpdate(int index) {
  FOR_EACH_OBSERVER(LauncherModelObserver, observers_,
                    LauncherItemWillChange(index));
}

int LauncherModel::ItemIndexByWindow(aura::Window* window) {
  LauncherItems::const_iterator i = ItemByWindow(window);
  return i == items_.end() ? -1 : static_cast<int>((i - items_.begin()));
}

LauncherItems::const_iterator LauncherModel::ItemByWindow(
    aura::Window* window) const {
  for (LauncherItems::const_iterator i = items_.begin();
       i != items_.end(); ++i) {
    if (i->window == window)
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

}  // namespace ash
