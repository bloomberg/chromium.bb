// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_TYPES_H_
#define ASH_LAUNCHER_LAUNCHER_TYPES_H_
#pragma once

#include <vector>

#include "third_party/skia/include/core/SkBitmap.h"
#include "ash/ash_export.h"

namespace aura {
class Window;
}

namespace ash {

typedef int LauncherID;

// Height of the Launcher. Hard coded to avoid resizing as items are
// added/removed.
ASH_EXPORT extern const int kLauncherPreferredSize;

// Type the LauncherItem represents.
enum ASH_EXPORT LauncherItemType {
  // Represents a tabbed browser.
  TYPE_TABBED,

  // Represents a running app panel.
  TYPE_APP_PANEL,

  // Represents a pinned shortcut to an app.
  TYPE_APP_SHORTCUT,

  // Toggles visiblity of the app list.
  TYPE_APP_LIST,

  // The browser shortcut button.
  TYPE_BROWSER_SHORTCUT,
};

// Represents the status of pinned or running app launcher items.
enum ASH_EXPORT LauncherItemStatus {
  STATUS_CLOSED,
  STATUS_RUNNING,
  STATUS_ACTIVE,
  STATUS_ATTENTION
};

struct ASH_EXPORT LauncherItem {
  LauncherItem();
  ~LauncherItem();

  LauncherItemType type;

  // Whether it is drawn as an incognito icon or not. Only used if this is
  // TYPE_TABBED. Note: This cannot be used for identifying incognito windows.
  bool is_incognito;

  // Image to display in the launcher. If this item is TYPE_TABBED the image is
  // a favicon image.
  SkBitmap image;

  // Assigned by the model when the item is added.
  LauncherID id;

  // Running status.
  LauncherItemStatus status;
};

typedef std::vector<LauncherItem> LauncherItems;

// The direction of the focus cycling.
enum ASH_EXPORT CycleDirection {
  CYCLE_FORWARD,
  CYCLE_BACKWARD
};

}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_TYPES_H_
