// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_CONSTANTS_H_
#define ASH_SHELF_SHELF_CONSTANTS_H_

#include "ash/ash_export.h"

namespace ash {

// Height of the shelf. Hard coded to avoid resizing as items are added/removed.
ASH_EXPORT extern const int kShelfPreferredSize;

// Max alpha of the shelf background.
ASH_EXPORT extern const int kShelfBackgroundAlpha;

// Invalid image resource id used for ShelfItemDetails.
extern const int kInvalidImageResourceID;

extern const int kInvalidShelfID;

// Animation duration for switching black shelf and dock background on and off.
ASH_EXPORT extern const int kTimeToSwitchBackgroundMs;

// The direction of the focus cycling.
enum CycleDirection {
  CYCLE_FORWARD,
  CYCLE_BACKWARD
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_CONSTANTS_H_
