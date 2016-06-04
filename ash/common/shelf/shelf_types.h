// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHELF_SHELF_TYPES_H_
#define ASH_COMMON_SHELF_SHELF_TYPES_H_

// TODO(sky): make callers explicitly include this.
#include "ash/common/wm/shelf/wm_shelf_types.h"

namespace ash {

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

}  // namespace ash

#endif  // ASH_COMMON_SHELF_SHELF_TYPES_H_
