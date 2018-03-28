// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_RENDER_FRAME_METADATA_H_
#define CC_TREES_RENDER_FRAME_METADATA_H_

#include "base/optional.h"
#include "cc/cc_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

class CC_EXPORT RenderFrameMetadata {
 public:
  RenderFrameMetadata();
  RenderFrameMetadata(const RenderFrameMetadata& other);
  RenderFrameMetadata(RenderFrameMetadata&& other);
  ~RenderFrameMetadata();

  // Certain fields should always have their changes reported. This will return
  // true when there is a difference between |rfm1| and |rfm2| for those fields.
  // These fields have a low frequency rate of change.
  static bool HasAlwaysUpdateMetadataChanged(const RenderFrameMetadata& rfm1,
                                             const RenderFrameMetadata& rfm2);

  RenderFrameMetadata& operator=(const RenderFrameMetadata&);
  RenderFrameMetadata& operator=(RenderFrameMetadata&& other);
  bool operator==(const RenderFrameMetadata& other);
  bool operator!=(const RenderFrameMetadata& other);

  // Indicates whether the scroll offset of the root layer is at top, i.e.,
  // whether scroll_offset.y() == 0.
  bool is_scroll_offset_at_top = true;

  // The background color of a CompositorFrame. It can be used for filling the
  // content area if the primary surface is unavailable and fallback is not
  // specified.
  SkColor root_background_color = SK_ColorWHITE;

  // Scroll offset of the root layer. This optional parameter is only valid
  // during tests.
  base::Optional<gfx::Vector2dF> root_scroll_offset;
};

}  // namespace cc

#endif  // CC_TREES_RENDER_FRAME_METADATA_H_
