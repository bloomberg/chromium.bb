// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_CONSTANTS_H_
#define ASH_SHELF_SHELF_CONSTANTS_H_

#include "ash/ash_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

// Size of the shelf when visible (height when the shelf is horizontal and
// width when the shelf is vertical).
ASH_EXPORT constexpr int kShelfSize = 48;

// We reserve a small area on the edge of the workspace area to ensure that
// the resize handle at the edge of the window can be hit.
constexpr int kWorkspaceAreaVisibleInset = 2;

// When autohidden we extend the touch hit target onto the screen so that the
// user can drag the shelf out.
constexpr int kWorkspaceAreaAutoHideInset = 5;

// Size of the shelf when auto-hidden.
ASH_EXPORT constexpr int kShelfAutoHideSize = 3;

// Animation duration for switching black shelf and dock background on and off.
ASH_EXPORT constexpr int kTimeToSwitchBackgroundMs = 1000;

// The default base color of the shelf to which different alpha values are
// applied based on the desired shelf opacity level.
ASH_EXPORT constexpr SkColor kShelfDefaultBaseColor = SK_ColorBLACK;

// Size allocated for each app button on the shelf.
ASH_EXPORT constexpr int kShelfButtonSize = 48;

// Size of the space between buttons on the shelf.
ASH_EXPORT constexpr int kShelfButtonSpacing = 16;

// Highlight color used for shelf button activated states.
// TODO(bruthig|mohsen): Use of this color is temporary. Draw the active state
// using the material design ripple animation.
ASH_EXPORT constexpr SkColor kShelfButtonActivatedHighlightColor =
    SkColorSetA(SK_ColorWHITE, 100);

// Ink drop color for shelf items.
constexpr SkColor kShelfInkDropBaseColor = SK_ColorWHITE;

// Opacity of the ink drop ripple for shelf items when the ripple is visible.
constexpr float kShelfInkDropVisibleOpacity = 0.2f;

// The foreground color of the icons used in the shelf (launcher,
// notifications, etc).
ASH_EXPORT constexpr SkColor kShelfIconColor = SK_ColorWHITE;

// The alpha value for the shelf background when a window is overlapping.
ASH_EXPORT constexpr int kShelfTranslucentAlpha = 153;

// The alpha value used to darken a colorized shelf when the shelf is
// translucent.
constexpr int kShelfTranslucentColorDarkenAlpha = 178;

// The alpha vlaue usesd to darken a colorized shelf when the shelf is opaque.
constexpr int kShelfOpaqueColorDarkenAlpha = 178;

// The width and height of the material design overflow button.
constexpr int kOverflowButtonSize = 32;

// The radius of the rounded corners of the overflow button.
constexpr int kOverflowButtonCornerRadius = 2;

// The radius of the circular material design app list button.
constexpr int kAppListButtonRadius = kOverflowButtonSize / 2;

// The direction of the focus cycling.
enum CycleDirection { CYCLE_FORWARD, CYCLE_BACKWARD };

}  // namespace ash

#endif  // ASH_SHELF_SHELF_CONSTANTS_H_
