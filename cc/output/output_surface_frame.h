// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_OUTPUT_SURFACE_FRAME_H_
#define CC_OUTPUT_OUTPUT_SURFACE_FRAME_H_

#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "cc/base/cc_export.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

// Metadata given to the OutputSurface for it to swap what was drawn and make
// current frame visible.
class CC_EXPORT OutputSurfaceFrame {
 public:
  OutputSurfaceFrame();
  OutputSurfaceFrame(OutputSurfaceFrame&& other);
  ~OutputSurfaceFrame();

  OutputSurfaceFrame& operator=(OutputSurfaceFrame&& other);

  gfx::Size size;
  // Providing both |sub_buffer_rect| and |content_bounds| is not supported;
  // if neither is present, regular swap is used.
  // Optional rect for partial or empty swap.
  base::Optional<gfx::Rect> sub_buffer_rect;
  // Optional content area for SwapWithBounds. Rectangles may overlap.
  std::vector<gfx::Rect> content_bounds;
  std::vector<ui::LatencyInfo> latency_info;

 private:
  DISALLOW_COPY_AND_ASSIGN(OutputSurfaceFrame);
};

}  // namespace cc

#endif  // CC_OUTPUT_OUTPUT_SURFACE_FRAME_H_
