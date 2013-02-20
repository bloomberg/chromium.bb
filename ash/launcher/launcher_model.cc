// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_model.h"

#include <algorithm>

#include "ash/launcher/launcher_model_observer.h"

namespace ash {

namespace {

int LauncherItemTypeToWeight(LauncherItemType type) {
  switch (type) {
    case TYPE_BROWSER_SHORTCUT:
      return 0;
    case TYPE_APP_SHORTCUT:
    case TYPE_WINDOWED_APP:
      return 1;
    case TYPE_TABBED:
    case TYPE_PLATFORM_APP:
      return 2;
    case TYPE_APP_LIST:
      return 3;
    case TYPE_APP_PANEL:
      return 4;
  }

  NOTREACHED() << "Invalid type " << type;
  return 2;
}

bool CompareByWeight(const LauncherItem& a, const LauncherItem& b) {
  return LauncherItemTypeToWeight(a.type) < LauncherItemTypeToWeight(b.type);
}

}  // namespace

LauncherModel::LauncherModel() : next_id_(1), status_(STATUS_NORMAL) {
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
  return AddAt(items_.size(), item);
}

int LauncherModel::AddAt(int index, const LauncherItem& item) {
  index = ValidateInsertionIndex(item.type, index);
  items_.insert(items_.begin() + index, item);
  items_[index].id = next_id_++;
  FOR_EACH_OBSERVER(LauncherModelObserver, observers_,
                    LauncherItemAdded(index));
  return index;
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
  int new_index = item.type == items_[index].type ?
      index : ValidateInsertionIndex(item.type, index);

  LauncherItem old_item(items_[index]);
  items_[index] = item;
  items_[index].id = old_item.id;
  FOR_EACH_OBSERVER(LauncherModelObserver, observers_,
                    LauncherItemChanged(index, old_item));

  // If the type changes confirm that the item is still in the right order.
  if (new_index != index)
    Move(index, new_index);
}

int LauncherModel::ItemIndexByID(LauncherID id) const {
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

int LauncherModel::FirstPanelIndex() const {
  LauncherItem weight_dummy;
  weight_dummy.type = TYPE_APP_PANEL;
  return std::lower_bound(items_.begin(), items_.end(), weight_dummy,
                          CompareByWeight) - items_.begin();
}

void LauncherModel::SetStatus(Status status) {
  if (status_ == status)
    return;

  status_ = status;
  FOR_EACH_OBSERVER(LauncherModelObserver, observers_,
                    LauncherStatusChanged());
}

void LauncherModel::AddObserver(LauncherModelObserver* observer) {
  observers_.AddObserver(observer);
}

void LauncherModel::RemoveObserver(LauncherModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

int LauncherModel::ValidateInsertionIndex(LauncherItemType type,
                                          int index) const {
  DCHECK(index >= 0 && index <= item_count());

  // Clamp |index| to the allowed range for the type as determined by |weight|.
  LauncherItem weight_dummy;
  weight_dummy.type = type;
  index = std::max(std::lower_bound(items_.begin(), items_.end(), weight_dummy,
                                    CompareByWeight) - items_.begin(),
                   static_cast<LauncherItems::difference_type>(index));
  index = std::min(std::upper_bound(items_.begin(), items_.end(), weight_dummy,
                                    CompareByWeight) - items_.begin(),
                   static_cast<LauncherItems::difference_type>(index));

  return index;
}

}  // namespace ash
