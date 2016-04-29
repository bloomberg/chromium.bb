// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_TYPES_H_
#define ASH_SHELF_SHELF_TYPES_H_

// TODO(sky): make callers explicitly include this.
#include "ash/wm/common/shelf/wm_shelf_types.h"

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

}  // namespace ash

#endif  // ASH_SHELF_SHELF_TYPES_H_
