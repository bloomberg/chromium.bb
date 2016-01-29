// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_SHELF_SHELF_ITEM_TYPES_H_
#define MASH_SHELF_SHELF_ITEM_TYPES_H_

#include <vector>

#include "base/strings/string16.h"
#include "ui/gfx/image/image_skia.h"

namespace mash {
namespace shelf {

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

  // TODO(msw): Integrate mojo apps with another enum type.
  TYPE_MOJO_APP,

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

struct ShelfItem {
  ShelfItem();
  ~ShelfItem();

  ShelfItemType type;

  // Image to display in the shelf.
  gfx::ImageSkia image;

  // Assigned by the model when the item is added.
  ShelfID id;

  // Running status.
  ShelfItemStatus status;

  // The id of an associated open window.
  // TODO(msw): Support multiple open windows per button.
  uint32_t window_id;

  // Title of the item.
  base::string16 title;
};

typedef std::vector<ShelfItem> ShelfItems;

}  // namespace shelf
}  // namespace mash

#endif  // MASH_SHELF_SHELF_ITEM_TYPES_H_
