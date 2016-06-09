// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHELF_SHELF_CONSTANTS_H_
#define ASH_COMMON_SHELF_SHELF_CONSTANTS_H_

#include "ash/ash_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

enum ShelfConstant {
  // The alpha value for the shelf background when a window is overlapping.
  SHELF_BACKGROUND_ALPHA
};

// Invalid image resource id used for ShelfItemDetails.
extern const int kInvalidImageResourceID;

const int kInvalidShelfID = 0;

// Size of the shelf when visible (height when the shelf is horizontal).
ASH_EXPORT extern const int kShelfSize;

// Size of the space between buttons on the shelf.
ASH_EXPORT extern const int kShelfButtonSpacing;

// Size allocated for each button on the shelf.
ASH_EXPORT extern const int kShelfButtonSize;

// Animation duration for switching black shelf and dock background on and off.
ASH_EXPORT extern const int kTimeToSwitchBackgroundMs;

// The base color of the shelf to which different alpha values are applied
// based on the desired shelf opacity level.
ASH_EXPORT extern const SkColor kShelfBaseColor;

// The foreground color of the icons used in the shelf (launcher,
// notifications, etc).
ASH_EXPORT extern const SkColor kShelfIconColor;

// The direction of the focus cycling.
enum CycleDirection { CYCLE_FORWARD, CYCLE_BACKWARD };

ASH_EXPORT int GetShelfConstant(ShelfConstant shelf_constant);

}  // namespace ash

#endif  // ASH_COMMON_SHELF_SHELF_CONSTANTS_H_
