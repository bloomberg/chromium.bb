// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shelf_item_delegate.h"

namespace ash {

ShelfItemDelegate::ShelfItemDelegate(const ash::ShelfID& shelf_id)
    : shelf_id_(shelf_id), image_set_by_controller_(false) {}

ShelfItemDelegate::~ShelfItemDelegate() {}

MenuItemList ShelfItemDelegate::GetAppMenuItems(int event_flags) {
  return MenuItemList();
}

AppWindowLauncherItemController*
ShelfItemDelegate::AsAppWindowLauncherItemController() {
  return nullptr;
}

}  // namespace ash
