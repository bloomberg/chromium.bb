// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/compositor_frame_helpers.h"

#include "cc/output/compositor_frame.h"

namespace viz {
namespace test {

cc::CompositorFrame MakeCompositorFrame() {
  cc::CompositorFrame frame = MakeEmptyCompositorFrame();
  std::unique_ptr<RenderPass> pass = RenderPass::Create();
  pass->SetNew(1, gfx::Rect(0, 0, 20, 20), gfx::Rect(), gfx::Transform());
  frame.render_pass_list.push_back(std::move(pass));
  return frame;
}

cc::CompositorFrame MakeEmptyCompositorFrame() {
  cc::CompositorFrame frame;
  frame.metadata.begin_frame_ack.source_id = BeginFrameArgs::kManualSourceId;
  frame.metadata.begin_frame_ack.sequence_number =
      BeginFrameArgs::kStartingFrameNumber;
  frame.metadata.begin_frame_ack.has_damage = true;
  frame.metadata.device_scale_factor = 1;
  return frame;
}

cc::CompositorFrame MakeCompositorFrame(
    std::vector<SurfaceId> activation_dependencies,
    std::vector<SurfaceId> referenced_surfaces,
    std::vector<TransferableResource> resource_list) {
  cc::CompositorFrame compositor_frame = test::MakeCompositorFrame();
  compositor_frame.metadata.begin_frame_ack = BeginFrameAck(0, 1, true);
  compositor_frame.metadata.activation_dependencies =
      std::move(activation_dependencies);
  compositor_frame.metadata.referenced_surfaces =
      std::move(referenced_surfaces);
  compositor_frame.resource_list = std::move(resource_list);
  return compositor_frame;
}

}  // namespace test
}  // namespace viz
