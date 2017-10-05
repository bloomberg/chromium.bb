// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shelf_model.h"

#include <algorithm>

#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model_observer.h"

namespace ash {

namespace {

int ShelfItemTypeToWeight(ShelfItemType type) {
  switch (type) {
    case TYPE_APP_LIST:
      // TODO(skuhne): If the app list item becomes movable again, this need
      // to be a fallthrough.
      return 0;
    case TYPE_BROWSER_SHORTCUT:
    case TYPE_PINNED_APP:
      return 1;
    case TYPE_APP:
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

const char kAppListId[] = "jlfapfmkapbjlfbpjedlinehodkccjee";

ShelfModel::ShelfModel() {
  // Add the app list item; its title and delegate are set in ShelfController.
  // This avoids an ash/public dep on ash/strings, and a Chrome-side delegate.
  ShelfItem item;
  item.type = TYPE_APP_LIST;
  item.id = ShelfID(kAppListId);
  const int index = Add(item);
  DCHECK_EQ(0, index);
}

ShelfModel::~ShelfModel() = default;

void ShelfModel::PinAppWithID(const std::string& app_id) {
  const ShelfID shelf_id(app_id);

  // If the app is already pinned, do nothing and return.
  if (IsAppPinned(shelf_id.app_id))
    return;

  // Convert an existing item to be pinned, or create a new pinned item.
  const int index = ItemIndexByID(shelf_id);
  if (index >= 0) {
    ShelfItem item = items_[index];
    DCHECK_EQ(item.type, TYPE_APP);
    DCHECK(!item.pinned_by_policy);
    item.type = TYPE_PINNED_APP;
    Set(index, item);
  } else if (!shelf_id.IsNull()) {
    ShelfItem item;
    item.type = TYPE_PINNED_APP;
    item.id = shelf_id;
    Add(item);
  }
}

bool ShelfModel::IsAppPinned(const std::string& app_id) {
  const int index = ItemIndexByID(ShelfID(app_id));
  return index >= 0 && (items_[index].type == TYPE_PINNED_APP ||
                        items_[index].type == TYPE_BROWSER_SHORTCUT);
}

void ShelfModel::UnpinAppWithID(const std::string& app_id) {
  // If the app is already not pinned, do nothing and return.
  if (!IsAppPinned(app_id))
    return;

  // Remove the item if it is closed, or mark it as unpinned.
  const int index = ItemIndexByID(ShelfID(app_id));
  ShelfItem item = items_[index];
  DCHECK_EQ(item.type, TYPE_PINNED_APP);
  DCHECK(!item.pinned_by_policy);
  if (item.status == STATUS_CLOSED) {
    RemoveItemAt(index);
  } else {
    item.type = TYPE_APP;
    Set(index, item);
  }
}

void ShelfModel::DestroyItemDelegates() {
  // Some ShelfItemDelegates access this model in their destructors and hence
  // need early cleanup.
  id_to_item_delegate_map_.clear();
}

int ShelfModel::Add(const ShelfItem& item) {
  return AddAt(items_.size(), item);
}

int ShelfModel::AddAt(int index, const ShelfItem& item) {
  // Items should have unique non-empty ids to avoid undefined model behavior.
  DCHECK(!item.id.IsNull()) << " The id is null.";
  DCHECK_EQ(ItemIndexByID(item.id), -1) << " The id is not unique: " << item.id;
  index = ValidateInsertionIndex(item.type, index);
  items_.insert(items_.begin() + index, item);
  for (auto& observer : observers_)
    observer.ShelfItemAdded(index);
  return index;
}

void ShelfModel::RemoveItemAt(int index) {
  DCHECK(index >= 0 && index < item_count());
  ShelfItem old_item(items_[index]);
  items_.erase(items_.begin() + index);
  id_to_item_delegate_map_.erase(old_item.id);
  for (auto& observer : observers_)
    observer.ShelfItemRemoved(index, old_item);
}

void ShelfModel::Move(int index, int target_index) {
  if (index == target_index)
    return;
  // TODO: this needs to enforce valid ranges.
  ShelfItem item(items_[index]);
  items_.erase(items_.begin() + index);
  items_.insert(items_.begin() + target_index, item);
  for (auto& observer : observers_)
    observer.ShelfItemMoved(index, target_index);
}

void ShelfModel::Set(int index, const ShelfItem& item) {
  if (index < 0 || index >= item_count()) {
    NOTREACHED();
    return;
  }

  int new_index = item.type == items_[index].type
                      ? index
                      : ValidateInsertionIndex(item.type, index);

  ShelfItem old_item(items_[index]);
  items_[index] = item;
  DCHECK(old_item.id == item.id);
  for (auto& observer : observers_)
    observer.ShelfItemChanged(index, old_item);

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

int ShelfModel::ItemIndexByID(const ShelfID& shelf_id) const {
  ShelfItems::const_iterator i = ItemByID(shelf_id);
  return i == items_.end() ? -1 : static_cast<int>(i - items_.begin());
}

int ShelfModel::GetItemIndexForType(ShelfItemType type) {
  for (size_t i = 0; i < items_.size(); ++i) {
    if (items_[i].type == type)
      return i;
  }
  return -1;
}

ShelfItems::const_iterator ShelfModel::ItemByID(const ShelfID& shelf_id) const {
  for (ShelfItems::const_iterator i = items_.begin(); i != items_.end(); ++i) {
    if (i->id == shelf_id)
      return i;
  }
  return items_.end();
}

int ShelfModel::FirstRunningAppIndex() const {
  ShelfItem weight_dummy;
  weight_dummy.type = TYPE_APP;
  return std::lower_bound(items_.begin(), items_.end(), weight_dummy,
                          CompareByWeight) -
         items_.begin();
}

int ShelfModel::FirstPanelIndex() const {
  ShelfItem weight_dummy;
  weight_dummy.type = TYPE_APP_PANEL;
  return std::lower_bound(items_.begin(), items_.end(), weight_dummy,
                          CompareByWeight) -
         items_.begin();
}

void ShelfModel::SetShelfItemDelegate(
    const ShelfID& shelf_id,
    std::unique_ptr<ShelfItemDelegate> item_delegate) {
  // Create a copy of the id that can be safely accessed if |shelf_id| is backed
  // by a controller that will be deleted in the assignment below.
  const ShelfID safe_shelf_id = shelf_id;
  if (item_delegate)
    item_delegate->set_shelf_id(safe_shelf_id);
  // This assignment replaces any ShelfItemDelegate already registered for
  // |shelf_id|.
  std::unique_ptr<ShelfItemDelegate> old_item_delegate =
      std::move(id_to_item_delegate_map_[safe_shelf_id]);
  id_to_item_delegate_map_[safe_shelf_id] = std::move(item_delegate);
  for (auto& observer : observers_) {
    observer.ShelfItemDelegateChanged(
        safe_shelf_id, old_item_delegate.get(),
        id_to_item_delegate_map_[safe_shelf_id].get());
  }
}

ShelfItemDelegate* ShelfModel::GetShelfItemDelegate(const ShelfID& shelf_id) {
  auto it = id_to_item_delegate_map_.find(shelf_id);
  if (it != id_to_item_delegate_map_.end())
    return it->second.get();
  return nullptr;
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
                                    CompareByWeight) -
                       items_.begin(),
                   static_cast<ShelfItems::difference_type>(index));
  index = std::min(std::upper_bound(items_.begin(), items_.end(), weight_dummy,
                                    CompareByWeight) -
                       items_.begin(),
                   static_cast<ShelfItems::difference_type>(index));

  return index;
}

}  // namespace ash
