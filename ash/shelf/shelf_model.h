// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_MODEL_H_
#define ASH_SHELF_SHELF_MODEL_H_

#include "ash/ash_export.h"
#include "ash/launcher/launcher_types.h"
#include "base/basictypes.h"
#include "base/observer_list.h"

namespace ash {

class ShelfModelObserver;

// Model used by ShelfView.
class ASH_EXPORT ShelfModel {
 public:
  enum Status {
    STATUS_NORMAL,
    // A status that indicates apps are syncing/loading.
    STATUS_LOADING,
  };

  ShelfModel();
  ~ShelfModel();

  // Adds a new item to the model. Returns the resulting index.
  int Add(const LauncherItem& item);

  // Adds the item. |index| is the requested insertion index, which may be
  // modified to meet type-based ordering. Returns the actual insertion index.
  int AddAt(int index, const LauncherItem& item);

  // Removes the item at |index|.
  void RemoveItemAt(int index);

  // Moves the item at |index| to |target_index|. |target_index| is in terms
  // of the model *after* the item at |index| is removed.
  void Move(int index, int target_index);

  // Resets the item at the specified index. The item maintains its existing
  // id and type.
  void Set(int index, const LauncherItem& item);

  // Returns the index of the item by id.
  int ItemIndexByID(LauncherID id) const;

  // Returns the |index| of the item matching |type| in |items_|.
  // Returns -1 if the matching item is not found.
  // Note: Requires a linear search.
  int GetItemIndexForType(LauncherItemType type);

  // Returns the index of the first panel or the index where the first panel
  // would go if there are no panels.
  int FirstPanelIndex() const;

  // Returns the id assigned to the next item added.
  LauncherID next_id() const { return next_id_; }

  // Returns a reserved id which will not be used by the |ShelfModel|.
  LauncherID reserve_external_id() { return next_id_++; }

  // Returns an iterator into items() for the item with the specified id, or
  // items().end() if there is no item with the specified id.
  LauncherItems::const_iterator ItemByID(LauncherID id) const;

  const LauncherItems& items() const { return items_; }
  int item_count() const { return static_cast<int>(items_.size()); }

  void SetStatus(Status status);
  Status status() const { return status_; }

  void AddObserver(ShelfModelObserver* observer);
  void RemoveObserver(ShelfModelObserver* observer);

 private:
  // Makes sure |index| is in line with the type-based order of items. If that
  // is not the case, adjusts index by shifting it to the valid range and
  // returns the new value.
  int ValidateInsertionIndex(LauncherItemType type, int index) const;

  // ID assigned to the next item.
  LauncherID next_id_;

  LauncherItems items_;
  Status status_;
  ObserverList<ShelfModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(ShelfModel);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_MODEL_H_
