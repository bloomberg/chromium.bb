// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_CONSTANTS_H_
#define ASH_SHELF_SHELF_CONSTANTS_H_

#include "ash/ash_export.h"

namespace ash {

// Max alpha of the shelf background.
ASH_EXPORT extern const int kShelfBackgroundAlpha;

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

// The direction of the focus cycling.
enum CycleDirection {
  CYCLE_FORWARD,
  CYCLE_BACKWARD
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_CONSTANTS_H_
