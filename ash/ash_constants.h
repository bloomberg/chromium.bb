// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASH_CONSTANTS_H_
#define ASH_ASH_CONSTANTS_H_

#include "ash/ash_export.h"

#include "third_party/skia/include/core/SkColor.h"

typedef unsigned int SkColor;

namespace ash {

// In the window corners, the resize areas don't actually expand bigger, but the
// 16 px at the end of each edge triggers diagonal resizing.
ASH_EXPORT extern const int kResizeAreaCornerSize;

// Ash windows do not have a traditional visible window frame. Window content
// extends to the edge of the window. We consider a small region outside the
// window bounds and an even smaller region overlapping the window to be the
// "non-client" area and use it for resizing.
ASH_EXPORT extern const int kResizeOutsideBoundsSize;
ASH_EXPORT extern const int kResizeOutsideBoundsScaleForTouch;
ASH_EXPORT extern const int kResizeInsideBoundsSize;

// Background color used for the Chrome OS boot splash screen.
extern const SkColor kChromeOsBootColor;

// The alpha to draw inactive browser frame icons with.
ASH_EXPORT extern const float kInactiveFrameButtonIconAlphaRatio;

// Similar to the above but for touch-optimized UI.
ASH_EXPORT extern const float kInactiveFrameButtonIconAlphaRatioTouch;

// The border color of keyboard focus for launcher items and system tray.
constexpr SkColor kFocusBorderColor = SK_ColorTRANSPARENT;
constexpr int kFocusBorderThickness = 0;

ASH_EXPORT extern const int kDefaultLargeCursorSize;

}  // namespace ash

#endif  // ASH_ASH_CONSTANTS_H_
