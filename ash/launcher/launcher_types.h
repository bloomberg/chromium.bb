// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_TYPES_H_
#define ASH_LAUNCHER_LAUNCHER_TYPES_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/strings/string16.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

typedef int LauncherID;

// Height of the Launcher. Hard coded to avoid resizing as items are
// added/removed.
ASH_EXPORT extern const int kLauncherPreferredSize;

// Max alpha of the launcher background.
ASH_EXPORT extern const int kLauncherBackgroundAlpha;

// Invalid image resource id used for LauncherItemDetails.
extern const int kInvalidImageResourceID;

extern const int kInvalidLauncherID;

// Animation duration for switching black shelf and dock background on and off.
ASH_EXPORT extern const int kTimeToSwitchBackgroundMs;

// Type the LauncherItem represents.
enum LauncherItemType {
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

  // Default value.
  TYPE_UNDEFINED,
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

  // Image to display in the launcher.
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

// LauncherItemDetails may be set on Window (by way of
// SetLauncherItemDetailsForWindow) to make the window appear in the shelf. See
// ShelfWindowWatcher for details.
struct ASH_EXPORT LauncherItemDetails {
  LauncherItemDetails();
  ~LauncherItemDetails();

  LauncherItemType type;

  // Resource id of the image to display on the shelf.
  int image_resource_id;

  // Title of the item.
  base::string16 title;
};

}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_TYPES_H_
