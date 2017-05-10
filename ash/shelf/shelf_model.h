// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_MODEL_H_
#define ASH_SHELF_SHELF_MODEL_H_

#include <map>
#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_item.h"
#include "ash/public/interfaces/shelf.mojom.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace ash {

class ShelfItemDelegate;
class ShelfModelObserver;

// Model used for shelf items. Owns ShelfItemDelegates but does not create them.
// TODO(msw): Remove id conversion functions; check for item presence as needed.
class ASH_EXPORT ShelfModel {
 public:
  ShelfModel();
  ~ShelfModel();

  // Pins an app with |app_id| to shelf. A running instance will get pinned.
  // In case there is no running instance a new shelf item is created and
  // pinned.
  void PinAppWithID(const std::string& app_id);

  // Check if the app with |app_id_| is pinned to the shelf.
  bool IsAppPinned(const std::string& app_id);

  // Unpins app item with |app_id|.
  void UnpinAppWithID(const std::string& app_id);

  // Cleans up the ShelfItemDelegates.
  void DestroyItemDelegates();

  // Adds a new item to the model. Returns the resulting index.
  int Add(const ShelfItem& item);

  // Adds the item. |index| is the requested insertion index, which may be
  // modified to meet type-based ordering. Returns the actual insertion index.
  int AddAt(int index, const ShelfItem& item);

  // Removes the item at |index|.
  void RemoveItemAt(int index);

  // Moves the item at |index| to |target_index|. |target_index| is in terms
  // of the model *after* the item at |index| is removed.
  void Move(int index, int target_index);

  // Resets the item at the specified index. The item's id should not change.
  void Set(int index, const ShelfItem& item);

  // Returns the index of the item with id |shelf_id|, or -1 if none exists.
  int ItemIndexByID(const ShelfID& shelf_id) const;

  // Returns the |index| of the item matching |type| in |items_|.
  // Returns -1 if the matching item is not found.
  int GetItemIndexForType(ShelfItemType type);

  // Returns the index of the first running application or the index where the
  // first running application would go if there are no running (non pinned)
  // applications yet.
  int FirstRunningAppIndex() const;

  // Returns the index of the first panel or the index where the first panel
  // would go if there are no panels.
  int FirstPanelIndex() const;

  // Returns an iterator into items() for the item with the specified id, or
  // items().end() if there is no item with the specified id.
  ShelfItems::const_iterator ItemByID(const ShelfID& shelf_id) const;

  const ShelfItems& items() const { return items_; }
  int item_count() const { return static_cast<int>(items_.size()); }

  // Sets |item_delegate| for the given |shelf_id| and takes ownership.
  void SetShelfItemDelegate(const ShelfID& shelf_id,
                            std::unique_ptr<ShelfItemDelegate> item_delegate);

  // Returns ShelfItemDelegate for |shelf_id|, or nullptr if none exists.
  ShelfItemDelegate* GetShelfItemDelegate(const ShelfID& shelf_id);

  void AddObserver(ShelfModelObserver* observer);
  void RemoveObserver(ShelfModelObserver* observer);

 private:
  // Makes sure |index| is in line with the type-based order of items. If that
  // is not the case, adjusts index by shifting it to the valid range and
  // returns the new value.
  int ValidateInsertionIndex(ShelfItemType type, int index) const;

  ShelfItems items_;
  base::ObserverList<ShelfModelObserver> observers_;

  std::map<ShelfID, std::unique_ptr<ShelfItemDelegate>>
      id_to_item_delegate_map_;

  DISALLOW_COPY_AND_ASSIGN(ShelfModel);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_MODEL_H_
