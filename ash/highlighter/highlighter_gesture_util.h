// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HIGHLIGHTER_HIGHLIGHTER_GESTURE_UTIL_H_
#define ASH_HIGHLIGHTER_HIGHLIGHTER_GESTURE_UTIL_H_

#include <vector>

#include "ash/ash_export.h"
#include "ui/gfx/geometry/rect_f.h"

namespace ash {

class FastInkPoints;

// Returns true if |box| is sufficiently long horizontally and short vertically.
bool ASH_EXPORT DetectHorizontalStroke(const gfx::Rect& box,
                                       const gfx::SizeF& pen_tip_size);

// Returns true if |points| is forming a "closed shape" which is defined as a
// sequence of points sweeping at least of 80% of a full circle around the
// center of |box|, and going in one direction (clockwise or counterclockwise),
// with a little noise tolerated.
bool ASH_EXPORT DetectClosedShape(const gfx::Rect& box,
                                  const FastInkPoints& points);

}  // namespace ash

#endif  // ASH_HIGHLIGHTER_HIGHLIGHTER_GESTURE_UTIL_H_
