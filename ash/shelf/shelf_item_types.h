// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_ITEM_TYPES_H_
#define ASH_SHELF_SHELF_ITEM_TYPES_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/strings/string16.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

typedef int ShelfID;

// The type of a shelf item.
enum ShelfItemType {
  // Represents a running app panel.
  TYPE_APP_PANEL,

  // Represents a pinned shortcut to an app.
  TYPE_APP_SHORTCUT,

  // Toggles visiblity of the app list.
  TYPE_APP_LIST,

  // The browser shortcut button.
  TYPE_BROWSER_SHORTCUT,

  // Represents a platform app.
  TYPE_PLATFORM_APP,

  // Represents a windowed V1 browser app.
  TYPE_WINDOWED_APP,

  // Represents a dialog.
  TYPE_DIALOG,

  // Default value.
  TYPE_UNDEFINED,
};

// Represents the status of applications in the shelf.
enum ShelfItemStatus {
  // A closed shelf item, i.e. has no live instance.
  STATUS_CLOSED,
  // A shelf item that has live instance.
  STATUS_RUNNING,
  // An active shelf item that has focus.
  STATUS_ACTIVE,
  // A shelf item that needs user's attention.
  STATUS_ATTENTION,
};

struct ASH_EXPORT ShelfItem {
  ShelfItem();
  ~ShelfItem();

  ShelfItemType type;

  // Image to display in the shelf.
  gfx::ImageSkia image;

  // Assigned by the model when the item is added.
  ShelfID id;

  // Running status.
  ShelfItemStatus status;
};

typedef std::vector<ShelfItem> ShelfItems;

// ShelfItemDetails may be set on Window (by way of
// SetShelfItemDetailsForWindow) to make the window appear in the shelf. See
// ShelfWindowWatcher for details.
struct ASH_EXPORT ShelfItemDetails {
  ShelfItemDetails();
  ~ShelfItemDetails();

  ShelfItemType type;

  // Resource id of the image to display on the shelf.
  int image_resource_id;

  // Title of the item.
  base::string16 title;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_ITEM_TYPES_H_
