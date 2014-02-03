// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_item_delegate_manager.h"

#include "ash/shelf/shelf_item_delegate.h"
#include "ash/shelf/shelf_model.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "base/stl_util.h"

namespace ash {

ShelfItemDelegateManager::ShelfItemDelegateManager(ShelfModel* model)
    : model_(model) {
  DCHECK(model_);
  model_->AddObserver(this);
}

ShelfItemDelegateManager::~ShelfItemDelegateManager() {
  model_->RemoveObserver(this);
  STLDeleteContainerPairSecondPointers(id_to_item_delegate_map_.begin(),
                                       id_to_item_delegate_map_.end());
}

void ShelfItemDelegateManager::SetShelfItemDelegate(
    ShelfID id,
    scoped_ptr<ShelfItemDelegate> item_delegate) {
  // If another ShelfItemDelegate is already registered for |id|, we assume
  // that this request is replacing ShelfItemDelegate for |id| with
  // |item_delegate|.
  RemoveShelfItemDelegate(id);
  id_to_item_delegate_map_[id] = item_delegate.release();
}

ShelfItemDelegate* ShelfItemDelegateManager::GetShelfItemDelegate(ShelfID id) {
  if (model_->ItemIndexByID(id) != -1) {
    // Each ShelfItem has to have a ShelfItemDelegate.
    DCHECK(id_to_item_delegate_map_.find(id) != id_to_item_delegate_map_.end());
    return id_to_item_delegate_map_[id];
  }
  return NULL;
}

void ShelfItemDelegateManager::ShelfItemAdded(int index) {
}

void ShelfItemDelegateManager::ShelfItemRemoved(int index, ShelfID id) {
  RemoveShelfItemDelegate(id);
}

void ShelfItemDelegateManager::ShelfItemMoved(int start_index,
                                              int target_index) {
}

void ShelfItemDelegateManager::ShelfItemChanged(int index,
                                                const ShelfItem& old_item) {
}

void ShelfItemDelegateManager::ShelfStatusChanged() {
}

void ShelfItemDelegateManager::RemoveShelfItemDelegate(ShelfID id) {
  if (id_to_item_delegate_map_.find(id) != id_to_item_delegate_map_.end()) {
    delete id_to_item_delegate_map_[id];
    id_to_item_delegate_map_.erase(id);
  }
}

}  // namespace ash
