// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/surfaces/surfaces_type_converters.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/macros.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_metadata.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/quads/yuv_video_draw_quad.h"
#include "cc/surfaces/surface_id_allocator.h"

using cc::mojom::CompositorFrame;
using cc::mojom::CompositorFramePtr;

namespace mojo {

// static
CompositorFramePtr
TypeConverter<CompositorFramePtr, cc::CompositorFrame>::Convert(
    const cc::CompositorFrame& input) {
  CompositorFramePtr frame = CompositorFrame::New();
  DCHECK(input.delegated_frame_data);
  cc::DelegatedFrameData* frame_data = input.delegated_frame_data.get();
  frame->resources =
      mojo::Array<cc::TransferableResource>(frame_data->resource_list);
  frame->metadata = input.metadata;
  const cc::RenderPassList& pass_list = frame_data->render_pass_list;
  std::vector<std::unique_ptr<cc::RenderPass>> copy;
  cc::RenderPass::CopyAll(pass_list, &copy);
  frame->passes = std::move(copy);
  return frame;
}

// static
std::unique_ptr<cc::CompositorFrame> ConvertToCompositorFrame(
    const CompositorFramePtr& input) {
  std::unique_ptr<cc::DelegatedFrameData> frame_data(
      new cc::DelegatedFrameData);
  frame_data->device_scale_factor = 1.f;
  frame_data->resource_list = input->resources.PassStorage();
  frame_data->render_pass_list.reserve(input->passes.size());
  for (size_t i = 0; i < input->passes.size(); ++i) {
    frame_data->render_pass_list.push_back(std::move(input->passes[i]));
  }
  std::unique_ptr<cc::CompositorFrame> frame(new cc::CompositorFrame);
  cc::CompositorFrameMetadata metadata = input->metadata;
  frame->delegated_frame_data = std::move(frame_data);
  return frame;
}

// static
std::unique_ptr<cc::CompositorFrame>
TypeConverter<std::unique_ptr<cc::CompositorFrame>,
              CompositorFramePtr>::Convert(const CompositorFramePtr& input) {
  return ConvertToCompositorFrame(input);
}

}  // namespace mojo
