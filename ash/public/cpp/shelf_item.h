// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SHELF_ITEM_H_
#define ASH_PUBLIC_CPP_SHELF_ITEM_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/strings/string16.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

struct ASH_PUBLIC_EXPORT ShelfItem {
  ShelfItem();
  ShelfItem(const ShelfItem& shelf_item);
  ~ShelfItem();

  ShelfItemType type = TYPE_UNDEFINED;

  // Image to display in the shelf.
  gfx::ImageSkia image;

  // Running status.
  ShelfItemStatus status = STATUS_CLOSED;

  // The application id and launch id for this shelf item.
  ShelfID id;

  // The title to display for tooltips, etc.
  base::string16 title;

  // Whether the tooltip should be shown on hover; generally true.
  bool shows_tooltip = true;

  // Whether the item is pinned by a policy preference (ie. user cannot un-pin).
  bool pinned_by_policy = false;

  // Whether the item has a notification.
  bool has_notification = false;
};

typedef std::vector<ShelfItem> ShelfItems;

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SHELF_ITEM_H_
