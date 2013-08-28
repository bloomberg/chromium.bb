// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_item_delegate_manager.h"

namespace ash {

LauncherItemDelegateManager::LauncherItemDelegateManager() {
}

LauncherItemDelegateManager::~LauncherItemDelegateManager() {
}

void LauncherItemDelegateManager::RegisterLauncherItemDelegate(
    ash::LauncherItemType type, LauncherItemDelegate* item_delegate) {
  // When a new |item_delegate| is registered with an exsiting |type|, it will
  // get overwritten.
  item_type_to_item_delegate_map_[type] = item_delegate;
}

LauncherItemDelegate* LauncherItemDelegateManager::GetLauncherItemDelegate(
    ash::LauncherItemType item_type) {
  DCHECK(item_type_to_item_delegate_map_.find(item_type) !=
      item_type_to_item_delegate_map_.end());
  return item_type_to_item_delegate_map_[item_type];
}

}  // namespace ash
