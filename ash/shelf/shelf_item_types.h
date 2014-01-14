// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_ITEM_TYPES_H_
#define ASH_SHELF_SHELF_ITEM_TYPES_H_

#include "ash/ash_export.h"
#include "ash/launcher/launcher_types.h"
#include "base/strings/string16.h"

namespace ash {

// ShelfItemDetails may be set on Window (by way of
// SetShelfItemDetailsForWindow) to make the window appear in the shelf. See
// ShelfWindowWatcher for details.
struct ASH_EXPORT ShelfItemDetails {
  ShelfItemDetails();
  ~ShelfItemDetails();

  LauncherItemType type;

  // Resource id of the image to display on the shelf.
  int image_resource_id;

  // Title of the item.
  base::string16 title;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_ITEM_TYPES_H_
