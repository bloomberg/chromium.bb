// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_model.h"

#include <algorithm>

#include "ash/public/cpp/app_launch_id.h"
#include "ash/public/cpp/shelf_item_delegate.h"
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

// Returns shelf app id. Play Store app is mapped to ARC platform host app.
// TODO(khmel): Fix this Arc application id mapping. See http://b/31703859
std::string GetShelfAppIdFromArcAppId(const std::string& arc_app_id) {
  static const char kPlayStoreAppId[] = "gpkmicpkkebkmabiaedjognfppcchdfa";
  static const char kArcHostAppId[] = "cnbgggchhmkkdmeppjobngjoejnihlei";
  return arc_app_id == kPlayStoreAppId ? kArcHostAppId : arc_app_id;
}

}  // namespace

ShelfModel::ShelfModel() : next_id_(1) {}

ShelfModel::~ShelfModel() {}

ShelfID ShelfModel::GetShelfIDForAppID(const std::string& app_id) {
  // TODO(khmel): Fix this Arc application id mapping. See http://b/31703859
  const std::string shelf_app_id = GetShelfAppIdFromArcAppId(app_id);

  if (shelf_app_id.empty())
    return ash::kInvalidShelfID;

  for (const ShelfItem& item : items_) {
    // ShelfWindowWatcher handles app panel windows separately.
    if (item.type != TYPE_APP_PANEL &&
        item.app_launch_id.app_id() == shelf_app_id) {
      return item.id;
    }
  }
  return kInvalidShelfID;
}

ShelfID ShelfModel::GetShelfIDForAppIDAndLaunchID(
    const std::string& app_id,
    const std::string& launch_id) {
  // TODO(khmel): Fix this Arc application id mapping. See http://b/31703859
  const std::string shelf_app_id = GetShelfAppIdFromArcAppId(app_id);

  if (shelf_app_id.empty())
    return ash::kInvalidShelfID;

  for (const ShelfItem& item : items_) {
    // ShelfWindowWatcher handles app panel windows separately.
    if (item.type != TYPE_APP_PANEL &&
        item.app_launch_id.app_id() == shelf_app_id &&
        item.app_launch_id.launch_id() == launch_id) {
      return item.id;
    }
  }
  return kInvalidShelfID;
}

const std::string& ShelfModel::GetAppIDForShelfID(ShelfID id) {
  ShelfItems::const_iterator item = ItemByID(id);
  return item != items().end() ? item->app_launch_id.app_id()
                               : base::EmptyString();
}

void ShelfModel::PinAppWithID(const std::string& app_id) {
  // TODO(khmel): Fix this Arc application id mapping. See http://b/31703859
  const std::string shelf_app_id = GetShelfAppIdFromArcAppId(app_id);

  // If the app is already pinned, do nothing and return.
  if (IsAppPinned(shelf_app_id))
    return;

  // Convert an existing item to be pinned, or create a new pinned item.
  const int index = ItemIndexByID(GetShelfIDForAppID(shelf_app_id));
  if (index >= 0) {
    ShelfItem item = items_[index];
    DCHECK_EQ(item.type, TYPE_APP);
    DCHECK(!item.pinned_by_policy);
    item.type = TYPE_PINNED_APP;
    Set(index, item);
  } else if (!shelf_app_id.empty()) {
    ash::ShelfItem item;
    item.type = ash::TYPE_PINNED_APP;
    item.app_launch_id = AppLaunchId(shelf_app_id);
    Add(item);
  }
}

bool ShelfModel::IsAppPinned(const std::string& app_id) {
  // TODO(khmel): Fix this Arc application id mapping. See http://b/31703859
  const std::string shelf_app_id = GetShelfAppIdFromArcAppId(app_id);

  const int index = ItemIndexByID(GetShelfIDForAppID(shelf_app_id));
  return index >= 0 && (items_[index].type == TYPE_PINNED_APP ||
                        items_[index].type == TYPE_BROWSER_SHORTCUT);
}

void ShelfModel::UnpinAppWithID(const std::string& app_id) {
  // TODO(khmel): Fix this Arc application id mapping. See http://b/31703859
  const std::string shelf_app_id = GetShelfAppIdFromArcAppId(app_id);

  // If the app is already not pinned, do nothing and return.
  if (!IsAppPinned(shelf_app_id))
    return;

  // Remove the item if it is closed, or mark it as unpinned.
  const int index = ItemIndexByID(GetShelfIDForAppID(shelf_app_id));
  ShelfItem item = items_[index];
  DCHECK_EQ(item.type, TYPE_PINNED_APP);
  DCHECK(!item.pinned_by_policy);
  if (item.status == ash::STATUS_CLOSED) {
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
  index = ValidateInsertionIndex(item.type, index);
  items_.insert(items_.begin() + index, item);
  items_[index].id = next_id_++;
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
  items_[index].id = old_item.id;
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

ShelfItems::const_iterator ShelfModel::ItemByID(ShelfID id) const {
  for (ShelfItems::const_iterator i = items_.begin(); i != items_.end(); ++i) {
    if (i->id == id)
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
    ShelfID id,
    std::unique_ptr<ShelfItemDelegate> item_delegate) {
  if (item_delegate)
    item_delegate->set_shelf_id(id);
  // This assignment replaces any ShelfItemDelegate already registered for |id|.
  id_to_item_delegate_map_[id] = std::move(item_delegate);
}

ShelfItemDelegate* ShelfModel::GetShelfItemDelegate(ShelfID id) {
  if (id_to_item_delegate_map_.find(id) != id_to_item_delegate_map_.end())
    return id_to_item_delegate_map_[id].get();
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
