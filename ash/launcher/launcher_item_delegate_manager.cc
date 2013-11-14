// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_item_delegate_manager.h"

#include "ash/launcher/launcher_item_delegate.h"
#include "ash/shelf/shelf_model.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "base/stl_util.h"

namespace ash {

LauncherItemDelegateManager::LauncherItemDelegateManager(ShelfModel* model)
    : model_(model) {
  DCHECK(model_);
  model_->AddObserver(this);
}

LauncherItemDelegateManager::~LauncherItemDelegateManager() {
  model_->RemoveObserver(this);
  STLDeleteContainerPairSecondPointers(id_to_item_delegate_map_.begin(),
                                       id_to_item_delegate_map_.end());
}

void LauncherItemDelegateManager::SetLauncherItemDelegate(
    ash::LauncherID id,
    scoped_ptr<LauncherItemDelegate> item_delegate) {
  // If another LauncherItemDelegate is already registered for |id|, we assume
  // that this request is replacing LauncherItemDelegate for |id| with
  // |item_delegate|.
  RemoveLauncherItemDelegate(id);
  id_to_item_delegate_map_[id] = item_delegate.release();
}

LauncherItemDelegate* LauncherItemDelegateManager::GetLauncherItemDelegate(
    ash::LauncherID id) {
  if (model_->ItemIndexByID(id) != -1) {
    // Each LauncherItem has to have a LauncherItemDelegate.
    DCHECK(id_to_item_delegate_map_.find(id) != id_to_item_delegate_map_.end());
    return id_to_item_delegate_map_[id];
  }
  return NULL;
}

void LauncherItemDelegateManager::ShelfItemAdded(int index) {
}

void LauncherItemDelegateManager::ShelfItemRemoved(int index, LauncherID id) {
  RemoveLauncherItemDelegate(id);
}

void LauncherItemDelegateManager::ShelfItemMoved(int start_index,
                                                 int target_index) {
}

void LauncherItemDelegateManager::ShelfItemChanged(
    int index,
    const LauncherItem& old_item) {
}

void LauncherItemDelegateManager::ShelfStatusChanged() {
}

void LauncherItemDelegateManager::RemoveLauncherItemDelegate(
    ash::LauncherID id) {
  if (id_to_item_delegate_map_.find(id) != id_to_item_delegate_map_.end()) {
    delete id_to_item_delegate_map_[id];
    id_to_item_delegate_map_.erase(id);
  }
}

}  // namespace ash
