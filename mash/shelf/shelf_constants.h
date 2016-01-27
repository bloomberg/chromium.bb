// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_SHELF_SHELF_CONSTANTS_H_
#define MASH_SHELF_SHELF_CONSTANTS_H_


namespace mash {
namespace shelf {

// Max alpha of the shelf background.
extern const int kShelfBackgroundAlpha;

// Invalid image resource id used for ShelfItemDetails.
extern const int kInvalidImageResourceID;

extern const int kInvalidShelfID;

// Size of the shelf when visible (height when the shelf is horizontal).
extern const int kShelfSize;

// Size of the space between buttons on the shelf.
extern const int kShelfButtonSpacing;

// Size allocated for each button on the shelf.
extern const int kShelfButtonSize;

// Animation duration for switching black shelf and dock background on and off.
extern const int kTimeToSwitchBackgroundMs;

// The direction of the focus cycling.
enum CycleDirection {
  CYCLE_FORWARD,
  CYCLE_BACKWARD
};

}  // namespace shelf
}  // namespace mash

#endif  // MASH_SHELF_SHELF_CONSTANTS_H_
