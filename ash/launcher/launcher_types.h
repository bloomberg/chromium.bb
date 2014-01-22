// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_TYPES_H_
#define ASH_LAUNCHER_LAUNCHER_TYPES_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/shelf/shelf_item_types.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

typedef int LauncherID;

struct ASH_EXPORT LauncherItem {
  LauncherItem();
  ~LauncherItem();

  ShelfItemType type;

  // Image to display in the launcher.
  gfx::ImageSkia image;

  // Assigned by the model when the item is added.
  LauncherID id;

  // Running status.
  ShelfItemStatus status;
};

typedef std::vector<LauncherItem> LauncherItems;

}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_TYPES_H_
