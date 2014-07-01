// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_VIEWPORT_SELECTION_BOUND_H_
#define CC_OUTPUT_VIEWPORT_SELECTION_BOUND_H_

#include "cc/base/cc_export.h"
#include "cc/input/selection_bound_type.h"
#include "ui/gfx/geometry/rect_f.h"

namespace cc {

// Marker for a selection end-point in (DIP) viewport coordinates.
struct CC_EXPORT ViewportSelectionBound {
  ViewportSelectionBound();
  ~ViewportSelectionBound();

  SelectionBoundType type;
  gfx::RectF viewport_rect;
  bool visible;
};

CC_EXPORT bool operator==(const ViewportSelectionBound& lhs,
                          const ViewportSelectionBound& rhs);
CC_EXPORT bool operator!=(const ViewportSelectionBound& lhs,
                          const ViewportSelectionBound& rhs);

}  // namespace cc

#endif  // CC_OUTPUT_VIEWPORT_SELECTION_BOUND_H_
