// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_SHELF_WM_SHELF_TYPES_H_
#define ASH_WM_COMMON_SHELF_WM_SHELF_TYPES_H_

namespace ash {
namespace wm {

enum ShelfAlignment {
  SHELF_ALIGNMENT_BOTTOM,
  SHELF_ALIGNMENT_LEFT,
  SHELF_ALIGNMENT_RIGHT,
  // Top has never been supported.

  // The locked alignment is set temporarily and not saved to preferences.
  SHELF_ALIGNMENT_BOTTOM_LOCKED,
};

enum ShelfBackgroundType {
  // The default transparent background.
  SHELF_BACKGROUND_DEFAULT,

  // The background when a window is overlapping.
  SHELF_BACKGROUND_OVERLAP,

  // The background when a window is maximized.
  SHELF_BACKGROUND_MAXIMIZED,
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_COMMON_SHELF_WM_SHELF_TYPES_H_
