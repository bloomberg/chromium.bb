// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHELF_SHELF_MODEL_OBSERVER_H_
#define ASH_COMMON_SHELF_SHELF_MODEL_OBSERVER_H_

#include "ash/ash_export.h"
#include "ash/common/shelf/shelf_item_types.h"

namespace ash {

struct ShelfItem;
class ShelfItemDelegate;

class ASH_EXPORT ShelfModelObserver {
 public:
  // Invoked after an item has been added to the model.
  virtual void ShelfItemAdded(int index) = 0;

  // Invoked after an item has been removed. |index| is the index the item was
  // at.
  virtual void ShelfItemRemoved(int index, ShelfID id) = 0;

  // Invoked after an item has been moved. See ShelfModel::Move() for details
  // of the arguments.
  virtual void ShelfItemMoved(int start_index, int target_index) = 0;

  // Invoked when the state of an item changes. |old_item| is the item
  // before the change.
  virtual void ShelfItemChanged(int index, const ShelfItem& old_item) = 0;

  // Gets called when a ShelfItemDelegate gets changed. Note that
  // |item_delegate| can be null.
  // NOTE: This is added a temporary fix for M39 to fix crbug.com/429870.
  // TODO(skuhne): Find the real reason for this problem and remove this fix.
  virtual void OnSetShelfItemDelegate(ShelfID id,
                                      ShelfItemDelegate* item_delegate) = 0;

 protected:
  virtual ~ShelfModelObserver() {}
};

}  // namespace ash

#endif  // ASH_COMMON_SHELF_SHELF_MODEL_OBSERVER_H_
