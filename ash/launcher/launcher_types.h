// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_TYPES_H_
#define ASH_LAUNCHER_LAUNCHER_TYPES_H_

#include <vector>

#include "ash/ash_export.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

typedef int LauncherID;

// Height of the Launcher. Hard coded to avoid resizing as items are
// added/removed.
ASH_EXPORT extern const int kLauncherPreferredSize;

// Max alpha of the launcher background.
ASH_EXPORT extern const int kLauncherBackgroundAlpha;

// Type the LauncherItem represents.
enum LauncherItemType {
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

  // Represents a platform app.
  TYPE_PLATFORM_APP,

  // Represents a windowed V1 browser app.
  TYPE_WINDOWED_APP,
};

// Represents the status of pinned or running app launcher items.
enum LauncherItemStatus {
  // A closed LauncherItem, i.e. has no live instance.
  STATUS_CLOSED,
  // A LauncherItem that has live instance.
  STATUS_RUNNING,
  // An active LauncherItem that has focus.
  STATUS_ACTIVE,
  // A LauncherItem that needs user's attention.
  STATUS_ATTENTION,
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
  gfx::ImageSkia image;

  // Assigned by the model when the item is added.
  LauncherID id;

  // Running status.
  LauncherItemStatus status;
};

typedef std::vector<LauncherItem> LauncherItems;

// The direction of the focus cycling.
enum CycleDirection {
  CYCLE_FORWARD,
  CYCLE_BACKWARD
};

}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_TYPES_H_
