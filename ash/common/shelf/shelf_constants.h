// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHELF_SHELF_CONSTANTS_H_
#define ASH_COMMON_SHELF_SHELF_CONSTANTS_H_

#include "ash/ash_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

enum ShelfConstant {
  // Size of the shelf when visible (height when the shelf is horizontal and
  // width when the shelf is vertical).
  SHELF_SIZE,

  // Insets allocated for shelf when it is auto hidden.
  SHELF_INSETS_FOR_AUTO_HIDE
};

// We reserve a small area on the edge of the workspace area to ensure that
// the resize handle at the edge of the window can be hit.
extern const int kWorkspaceAreaVisibleInset;

// When autohidden we extend the touch hit target onto the screen so that the
// user can drag the shelf out.
extern const int kWorkspaceAreaAutoHideInset;

// Size of the shelf when auto-hidden.
ASH_EXPORT extern const int kShelfAutoHideSize;

// Animation duration for switching black shelf and dock background on and off.
ASH_EXPORT extern const int kTimeToSwitchBackgroundMs;

// The default base color of the shelf to which different alpha values are
// applied based on the desired shelf opacity level.
ASH_EXPORT extern const SkColor kShelfDefaultBaseColor;

// Size allocated for each app button on the shelf.
ASH_EXPORT extern const int kShelfButtonSize;

// Size of the space between buttons on the shelf.
ASH_EXPORT extern const int kShelfButtonSpacing;

// Highlight color used for shelf button activated states.
// TODO(bruthig|mohsen): Use of this color is temporary. Draw the active state
// using the material design ripple animation.
ASH_EXPORT extern const SkColor kShelfButtonActivatedHighlightColor;

// Ink drop color for shelf items.
extern const SkColor kShelfInkDropBaseColor;

// Opacity of the ink drop ripple for shelf items when the ripple is visible.
extern const float kShelfInkDropVisibleOpacity;

// The foreground color of the icons used in the shelf (launcher,
// notifications, etc).
ASH_EXPORT extern const SkColor kShelfIconColor;

// The alpha value for the shelf background when a window is overlapping.
ASH_EXPORT extern const int kShelfTranslucentAlpha;

// The width and height of the material design overflow button.
// TODO(tdanderson): Refactor constants which are common between the shelf
// and the tray. See crbug.com/623987.
extern const int kOverflowButtonSize;

// The radius of the rounded corners of the overflow button.
extern const int kOverflowButtonCornerRadius;

// The radius of the circular material design app list button.
extern const int kAppListButtonRadius;

// The direction of the focus cycling.
enum CycleDirection { CYCLE_FORWARD, CYCLE_BACKWARD };

ASH_EXPORT int GetShelfConstant(ShelfConstant shelf_constant);

}  // namespace ash

#endif  // ASH_COMMON_SHELF_SHELF_CONSTANTS_H_
