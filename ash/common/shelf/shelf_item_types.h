// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHELF_SHELF_ITEM_TYPES_H_
#define ASH_COMMON_SHELF_SHELF_ITEM_TYPES_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/common/shelf/shelf_constants.h"
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

  // Represents an app: Extension "V1" (legacy packaged and hosted) apps,
  //                    Extension "V2" (platform) apps,
  //                    Arc (App Runtime for Chrome - Android Play Store) apps.
  TYPE_APP,

  // Represents a dialog.
  TYPE_DIALOG,

  // The expanded IME menu in the shelf.
  TYPE_IME_MENU,

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

  ShelfItemType type = TYPE_UNDEFINED;

  // Image to display in the shelf.
  gfx::ImageSkia image;

  // Assigned by the model when the item is added.
  ShelfID id = kInvalidShelfID;

  // Running status.
  ShelfItemStatus status = STATUS_CLOSED;

  // The application id for this shelf item; only populated for some items.
  std::string app_id;
};

typedef std::vector<ShelfItem> ShelfItems;

}  // namespace ash

#endif  // ASH_COMMON_SHELF_SHELF_ITEM_TYPES_H_
