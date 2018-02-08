// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_COMPOSITOR_FRAME_H_
#define COMPONENTS_VIZ_COMMON_QUADS_COMPOSITOR_FRAME_H_

#include <memory>

#include "base/macros.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/viz_common_export.h"

namespace viz {

// A CompositorFrame struct contains the complete output of a compositor meant
// for display. A CompositorFrame consists of a series of DrawQuads that
// describe placement of textures, solid colors, overlays and other
// CompositorFrames within an area specified by the parent compositor. DrawQuads
// may share common data referred to as SharedQuadState. A CompositorFrame also
// has |metadata| that refers to global graphical state associated with this
// frame.
class VIZ_COMMON_EXPORT CompositorFrame {
 public:
  CompositorFrame();
  CompositorFrame(CompositorFrame&& other);
  ~CompositorFrame();

  CompositorFrame& operator=(CompositorFrame&& other);

  float device_scale_factor() const { return metadata.device_scale_factor; }

  const gfx::Size& size_in_pixels() const {
    DCHECK(!render_pass_list.empty());
    return render_pass_list.back()->output_rect.size();
  }

  bool HasCopyOutputRequests() const;

  CompositorFrameMetadata metadata;
  std::vector<TransferableResource> resource_list;
  // This list is in the order that each RenderPass will be drawn. The last one
  // is the "root" RenderPass that all others are directly or indirectly drawn
  // into.
  RenderPassList render_pass_list;

 private:
  DISALLOW_COPY_AND_ASSIGN(CompositorFrame);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_COMPOSITOR_FRAME_H_
