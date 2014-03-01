// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_model.h"

#include <algorithm>

#include "ash/ash_switches.h"
#include "ash/shelf/shelf_model_observer.h"

namespace ash {

namespace {

int ShelfItemTypeToWeight(ShelfItemType type) {
  switch (type) {
    case TYPE_APP_LIST:
      // TODO(skuhne): If the app list item becomes movable again, this need
      // to be a fallthrough.
      return 0;
    case TYPE_BROWSER_SHORTCUT:
    case TYPE_APP_SHORTCUT:
      return 1;
    case TYPE_WINDOWED_APP:
    case TYPE_PLATFORM_APP:
      return 2;
    case TYPE_DIALOG:
      return 3;
    case TYPE_APP_PANEL:
      return 4;
    case TYPE_UNDEFINED:
      NOTREACHED() << "ShelfItemType must be set";
      return -1;
  }

  NOTREACHED() << "Invalid type " << type;
  return 1;
}

bool CompareByWeight(const ShelfItem& a, const ShelfItem& b) {
  return ShelfItemTypeToWeight(a.type) < ShelfItemTypeToWeight(b.type);
}

}  // namespace

ShelfModel::ShelfModel() : next_id_(1), status_(STATUS_NORMAL) {
}

ShelfModel::~ShelfModel() {
}

int ShelfModel::Add(const ShelfItem& item) {
  return AddAt(items_.size(), item);
}

int ShelfModel::AddAt(int index, const ShelfItem& item) {
  index = ValidateInsertionIndex(item.type, index);
  items_.insert(items_.begin() + index, item);
  items_[index].id = next_id_++;
  FOR_EACH_OBSERVER(ShelfModelObserver, observers_, ShelfItemAdded(index));
  return index;
}

void ShelfModel::RemoveItemAt(int index) {
  DCHECK(index >= 0 && index < item_count());
  // The app list and browser shortcut can't be removed.
  DCHECK(items_[index].type != TYPE_APP_LIST &&
         items_[index].type != TYPE_BROWSER_SHORTCUT);
  ShelfID id = items_[index].id;
  items_.erase(items_.begin() + index);
  FOR_EACH_OBSERVER(ShelfModelObserver, observers_,
                    ShelfItemRemoved(index, id));
}

void ShelfModel::Move(int index, int target_index) {
  if (index == target_index)
    return;
  // TODO: this needs to enforce valid ranges.
  ShelfItem item(items_[index]);
  items_.erase(items_.begin() + index);
  items_.insert(items_.begin() + target_index, item);
  FOR_EACH_OBSERVER(ShelfModelObserver, observers_,
                    ShelfItemMoved(index, target_index));
}

void ShelfModel::Set(int index, const ShelfItem& item) {
  DCHECK(index >= 0 && index < item_count());
  int new_index = item.type == items_[index].type ?
      index : ValidateInsertionIndex(item.type, index);

  ShelfItem old_item(items_[index]);
  items_[index] = item;
  items_[index].id = old_item.id;
  FOR_EACH_OBSERVER(ShelfModelObserver, observers_,
                    ShelfItemChanged(index, old_item));

  // If the type changes confirm that the item is still in the right order.
  if (new_index != index) {
    // The move function works by removing one item and then inserting it at the
    // new location. However - by removing the item first the order will change
    // so that our target index needs to be corrected.
    // TODO(skuhne): Moving this into the Move function breaks lots of unit
    // tests. So several functions were already using this incorrectly.
    // That needs to be cleaned up.
    if (index < new_index)
      new_index--;

    Move(index, new_index);
  }
}

int ShelfModel::ItemIndexByID(ShelfID id) const {
  ShelfItems::const_iterator i = ItemByID(id);
  return i == items_.end() ? -1 : static_cast<int>(i - items_.begin());
}

int ShelfModel::GetItemIndexForType(ShelfItemType type) {
  for (size_t i = 0; i < items_.size(); ++i) {
    if (items_[i].type == type)
      return i;
  }
  return -1;
}

ShelfItems::const_iterator ShelfModel::ItemByID(int id) const {
  for (ShelfItems::const_iterator i = items_.begin();
       i != items_.end(); ++i) {
    if (i->id == id)
      return i;
  }
  return items_.end();
}

int ShelfModel::FirstRunningAppIndex() const {
  // Since lower_bound only checks weights against each other, we do not need
  // to explicitly change different running application types.
  DCHECK_EQ(ShelfItemTypeToWeight(TYPE_WINDOWED_APP),
            ShelfItemTypeToWeight(TYPE_PLATFORM_APP));
  ShelfItem weight_dummy;
  weight_dummy.type = TYPE_WINDOWED_APP;
  return std::lower_bound(items_.begin(), items_.end(), weight_dummy,
                          CompareByWeight) - items_.begin();
}

int ShelfModel::FirstPanelIndex() const {
  ShelfItem weight_dummy;
  weight_dummy.type = TYPE_APP_PANEL;
  return std::lower_bound(items_.begin(), items_.end(), weight_dummy,
                          CompareByWeight) - items_.begin();
}

void ShelfModel::SetStatus(Status status) {
  if (status_ == status)
    return;

  status_ = status;
  FOR_EACH_OBSERVER(ShelfModelObserver, observers_, ShelfStatusChanged());
}

void ShelfModel::AddObserver(ShelfModelObserver* observer) {
  observers_.AddObserver(observer);
}

void ShelfModel::RemoveObserver(ShelfModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

int ShelfModel::ValidateInsertionIndex(ShelfItemType type, int index) const {
  DCHECK(index >= 0 && index <= item_count() + 1);

  // Clamp |index| to the allowed range for the type as determined by |weight|.
  ShelfItem weight_dummy;
  weight_dummy.type = type;
  index = std::max(std::lower_bound(items_.begin(), items_.end(), weight_dummy,
                                    CompareByWeight) - items_.begin(),
                   static_cast<ShelfItems::difference_type>(index));
  index = std::min(std::upper_bound(items_.begin(), items_.end(), weight_dummy,
                                    CompareByWeight) - items_.begin(),
                   static_cast<ShelfItems::difference_type>(index));

  return index;
}

}  // namespace ash
