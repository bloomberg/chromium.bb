// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_OVERLAY_CANDIDATE_H_
#define CC_OUTPUT_OVERLAY_CANDIDATE_H_

#include <vector>

#include "base/basictypes.h"
#include "cc/base/cc_export.h"
#include "cc/resources/resource_format.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

struct CC_EXPORT OverlayCandidate {
  enum OverlayTransform {
    NONE,
    FLIP_HORIZONTAL,
    FLIP_VERTICAL,
    ROTATE_90,
    ROTATE_180,
    ROTATE_270,
  };

  OverlayCandidate();
  ~OverlayCandidate();

  // Transformation to apply to layer during composition.
  OverlayTransform transform;
  // Format of the buffer to composite.
  ResourceFormat format;
  // Rect on the display to position the overlay to.
  gfx::Rect display_rect;
  // Crop within the buffer to be placed inside |display_rect|.
  gfx::RectF uv_rect;

  // To be modified by the implementer if this candidate can go into
  // an overlay.
  bool overlay_handled;
};

}  // namespace cc

#endif  // CC_OUTPUT_OVERLAY_CANDIDATE_H_
