// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_RENDER_FRAME_METADATA_H_
#define CC_TREES_RENDER_FRAME_METADATA_H_

#include "cc/cc_export.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

class CC_EXPORT RenderFrameMetadata {
 public:
  RenderFrameMetadata();
  RenderFrameMetadata(const RenderFrameMetadata& other);
  RenderFrameMetadata(RenderFrameMetadata&& other);
  ~RenderFrameMetadata();

  RenderFrameMetadata& operator=(const RenderFrameMetadata&);
  RenderFrameMetadata& operator=(RenderFrameMetadata&& other);

  // Scroll offset and scale of the root layer. This can be used for tasks
  // like positioning windowed plugins.
  gfx::Vector2dF root_scroll_offset;
};

}  // namespace cc

#endif  // CC_TREES_RENDER_FRAME_METADATA_H_
