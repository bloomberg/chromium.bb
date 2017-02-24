// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SHELF_TYPES_H_
#define ASH_PUBLIC_CPP_SHELF_TYPES_H_

#include <cstdint>

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

enum ShelfAlignment {
  SHELF_ALIGNMENT_BOTTOM,
  SHELF_ALIGNMENT_LEFT,
  SHELF_ALIGNMENT_RIGHT,
  // Top has never been supported.

  // The locked alignment is set temporarily and not saved to preferences.
  SHELF_ALIGNMENT_BOTTOM_LOCKED,
};

enum ShelfAutoHideBehavior {
  // Always auto-hide.
  SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS,

  // Never auto-hide.
  SHELF_AUTO_HIDE_BEHAVIOR_NEVER,

  // Always hide.
  SHELF_AUTO_HIDE_ALWAYS_HIDDEN,
};

enum ShelfAutoHideState {
  SHELF_AUTO_HIDE_SHOWN,
  SHELF_AUTO_HIDE_HIDDEN,
};

enum ShelfVisibilityState {
  // Always visible.
  SHELF_VISIBLE,

  // A couple of pixels are reserved at the bottom for the shelf.
  SHELF_AUTO_HIDE,

  // Nothing is shown. Used for fullscreen windows.
  SHELF_HIDDEN,
};

enum ShelfBackgroundType {
  // The default transparent background.
  SHELF_BACKGROUND_DEFAULT,

  // The background when a window is overlapping.
  SHELF_BACKGROUND_OVERLAP,

  // The background when a window is maximized.
  SHELF_BACKGROUND_MAXIMIZED,
};

// Source of the launch or activation request, for tracking.
enum ShelfLaunchSource {
  // The item was launched from an unknown source (ie. not the app list).
  LAUNCH_FROM_UNKNOWN,

  // The item was launched from a generic app list view.
  LAUNCH_FROM_APP_LIST,

  // The item was launched from an app list search view.
  LAUNCH_FROM_APP_LIST_SEARCH,
};

// The actions that may be performed when a shelf item is selected.
enum ShelfAction {
  // No action was taken.
  SHELF_ACTION_NONE,

  // A new window was created.
  SHELF_ACTION_NEW_WINDOW_CREATED,

  // An existing inactive window was activated.
  SHELF_ACTION_WINDOW_ACTIVATED,

  // The currently active window was minimized.
  SHELF_ACTION_WINDOW_MINIMIZED,

  // The app list launcher menu was shown.
  SHELF_ACTION_APP_LIST_SHOWN,
};

typedef int ShelfID;
const int kInvalidShelfID = 0;

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
  //                    ARC (App Runtime for Chrome - Android Play Store) apps.
  TYPE_APP,

  // Represents a dialog.
  TYPE_DIALOG,

  // Default value.
  TYPE_UNDEFINED,
};

// Returns true if |type| is a valid ShelfItemType.
ASH_PUBLIC_EXPORT bool IsValidShelfItemType(int64_t type);

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

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SHELF_TYPES_H_
