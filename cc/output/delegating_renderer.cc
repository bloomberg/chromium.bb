// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/delegating_renderer.h"

#include <set>
#include <string>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "cc/output/context_provider.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/resources/resource_provider.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"


namespace cc {

DelegatingRenderer::DelegatingRenderer(OutputSurface* output_surface,
                                       ResourceProvider* resource_provider)
    : output_surface_(output_surface), resource_provider_(resource_provider) {
  DCHECK(resource_provider_);
}

DelegatingRenderer::~DelegatingRenderer() = default;

void DelegatingRenderer::DrawFrame(
    RenderPassList* render_passes_in_draw_order) {
  TRACE_EVENT0("cc", "DelegatingRenderer::DrawFrame");

  delegated_frame_data_ = base::WrapUnique(new DelegatedFrameData);
  DelegatedFrameData& out_data = *delegated_frame_data_;
  // Move the render passes and resources into the |out_frame|.
  out_data.render_pass_list.swap(*render_passes_in_draw_order);

  // Collect all resource ids in the render passes into a ResourceIdArray.
  ResourceProvider::ResourceIdArray resources;
  for (const auto& render_pass : out_data.render_pass_list) {
    for (auto* quad : render_pass->quad_list) {
      for (ResourceId resource_id : quad->resources)
        resources.push_back(resource_id);
    }
  }
  resource_provider_->PrepareSendToParent(resources, &out_data.resource_list);
}

void DelegatingRenderer::SwapBuffers(CompositorFrameMetadata metadata) {
  TRACE_EVENT0("cc,benchmark", "DelegatingRenderer::SwapBuffers");
  CompositorFrame compositor_frame;
  compositor_frame.metadata = std::move(metadata);
  compositor_frame.delegated_frame_data = std::move(delegated_frame_data_);
  output_surface_->SwapBuffers(std::move(compositor_frame));
}

}  // namespace cc
