// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SHELF_MODEL_OBSERVER_H_
#define ASH_PUBLIC_CPP_SHELF_MODEL_OBSERVER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/shelf_types.h"

namespace ash {

struct ShelfItem;
class ShelfItemDelegate;

// An observer interface for shelf item and delegate changes in ShelfModel.
class ASH_PUBLIC_EXPORT ShelfModelObserver {
 public:
  // Invoked after an item has been added to the model.
  virtual void ShelfItemAdded(int index) = 0;

  // Invoked after an item has been removed from the model. |index| is the index
  // the item was at before removal, |old_item| is the item before removal.
  virtual void ShelfItemRemoved(int index, const ShelfItem& old_item) = 0;

  // Invoked after an item has been moved. See ShelfModel::Move() for details
  // of the arguments.
  virtual void ShelfItemMoved(int start_index, int target_index) = 0;

  // Invoked after an item changes. |old_item| is the item before the change.
  virtual void ShelfItemChanged(int index, const ShelfItem& old_item) = 0;

  // Invoked after a delegate changes. |delegate| is the new value.
  virtual void ShelfItemDelegateChanged(const ShelfID& id,
                                        ShelfItemDelegate* delegate) = 0;

 protected:
  virtual ~ShelfModelObserver() {}
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SHELF_MODEL_OBSERVER_H_
