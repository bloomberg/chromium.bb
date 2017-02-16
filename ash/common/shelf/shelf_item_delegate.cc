// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/shelf_item_delegate.h"

#include "ui/display/types/display_constants.h"

namespace ash {

ShelfItemDelegate::ShelfItemDelegate() {}
ShelfItemDelegate::~ShelfItemDelegate() {}

ShelfAction ShelfItemDelegate::ItemSelectedBySource(ShelfLaunchSource source) {
  return ItemSelected(ui::ET_UNKNOWN, ui::EF_NONE, display::kInvalidDisplayId,
                      source);
}

}  // namespace ash
