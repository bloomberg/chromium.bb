// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/delegating_renderer.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "cc/output/context_provider.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/resources/resource_provider.h"
#include "gpu/command_buffer/client/context_support.h"


namespace cc {

DelegatingRenderer::DelegatingRenderer(OutputSurface* output_surface,
                                       ResourceProvider* resource_provider)
    : output_surface_(output_surface), resource_provider_(resource_provider) {
  DCHECK(resource_provider_);
}

DelegatingRenderer::~DelegatingRenderer() = default;

void DelegatingRenderer::DrawFrame(CompositorFrameMetadata metadata,
                                   RenderPassList render_passes_in_draw_order) {
  TRACE_EVENT0("cc", "DelegatingRenderer::DrawFrame");

  // Collect all resource ids in the render passes into a single array.
  ResourceProvider::ResourceIdArray resources;
  for (const auto& render_pass : render_passes_in_draw_order) {
    for (auto* quad : render_pass->quad_list) {
      for (ResourceId resource_id : quad->resources)
        resources.push_back(resource_id);
    }
  }

  auto data = base::MakeUnique<DelegatedFrameData>();
  resource_provider_->PrepareSendToParent(resources, &data->resource_list);
  data->render_pass_list = std::move(render_passes_in_draw_order);

  CompositorFrame compositor_frame;
  compositor_frame.metadata = std::move(metadata);
  compositor_frame.delegated_frame_data = std::move(data);
  output_surface_->SwapBuffers(std::move(compositor_frame));
}

}  // namespace cc
