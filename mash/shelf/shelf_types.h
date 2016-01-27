// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_SHELF_SHELF_TYPES_H_
#define MASH_SHELF_SHELF_TYPES_H_

namespace mash {
namespace shelf {

enum ShelfAlignment {
  SHELF_ALIGNMENT_BOTTOM,
  SHELF_ALIGNMENT_LEFT,
  SHELF_ALIGNMENT_RIGHT,
  SHELF_ALIGNMENT_TOP,
};

enum ShelfAutoHideBehavior {
  // Always auto-hide.
  SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS,

  // Never auto-hide.
  SHELF_AUTO_HIDE_BEHAVIOR_NEVER,

  // Always hide.
  SHELF_AUTO_HIDE_ALWAYS_HIDDEN,
};

enum ShelfVisibilityState {
  // Always visible.
  SHELF_VISIBLE,

  // A couple of pixels are reserved at the bottom for the shelf.
  SHELF_AUTO_HIDE,

  // Nothing is shown. Used for fullscreen windows.
  SHELF_HIDDEN,
};

enum ShelfAutoHideState {
  SHELF_AUTO_HIDE_SHOWN,
  SHELF_AUTO_HIDE_HIDDEN,
};

enum ShelfBackgroundType {
  // The default transparent background.
  SHELF_BACKGROUND_DEFAULT,

  // The background when a window is overlapping.
  SHELF_BACKGROUND_OVERLAP,

  // The background when a window is maximized.
  SHELF_BACKGROUND_MAXIMIZED,
};

}  // namespace shelf
}  // namespace mash

#endif  // MASH_SHELF_SHELF_TYPES_H_
