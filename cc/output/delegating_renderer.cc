// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/delegating_renderer.h"

#include <set>
#include <string>
#include <vector>

#include "base/debug/trace_event.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/quads/checkerboard_draw_quad.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/quads/yuv_video_draw_quad.h"
#include "cc/resources/resource_provider.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/khronos/GLES2/gl2ext.h"

using WebKit::WebGraphicsContext3D;

namespace cc {

scoped_ptr<DelegatingRenderer> DelegatingRenderer::Create(
    RendererClient* client,
    OutputSurface* output_surface,
    ResourceProvider* resource_provider) {
  scoped_ptr<DelegatingRenderer> renderer(
      new DelegatingRenderer(client, output_surface, resource_provider));
  if (!renderer->Initialize())
    return scoped_ptr<DelegatingRenderer>();
  return renderer.Pass();
}

DelegatingRenderer::DelegatingRenderer(
    RendererClient* client,
    OutputSurface* output_surface,
    ResourceProvider* resource_provider)
    : Renderer(client),
      output_surface_(output_surface),
      resource_provider_(resource_provider),
      visible_(true) {
  DCHECK(resource_provider_);
}

bool DelegatingRenderer::Initialize() {
  capabilities_.using_partial_swap = false;
  capabilities_.max_texture_size = resource_provider_->max_texture_size();
  capabilities_.best_texture_format = resource_provider_->best_texture_format();
  capabilities_.allow_partial_texture_updates = false;
  capabilities_.using_offscreen_context3d = false;

  if (!output_surface_->context_provider()) {
    // TODO(danakj): Make software compositing work.
    return true;
  }

  WebGraphicsContext3D* context3d =
      output_surface_->context_provider()->Context3d();

  if (!context3d->makeContextCurrent())
    return false;

  std::string unique_context_name = base::StringPrintf(
      "%s-%p",
      Settings().compositor_name.c_str(),
      context3d);
  context3d->pushGroupMarkerEXT(unique_context_name.c_str());

  std::string extensions_string =
      UTF16ToASCII(context3d->getString(GL_EXTENSIONS));

  std::vector<std::string> extensions;
  base::SplitString(extensions_string, ' ', &extensions);

  // TODO(danakj): We need non-GPU-specific paths for these things. This
  // renderer shouldn't need to use context3d extensions directly.
  bool has_set_visibility = false;
  bool has_io_surface = false;
  bool has_arb_texture_rect = false;
  bool has_egl_image = false;
  bool has_map_image = false;
  for (size_t i = 0; i < extensions.size(); ++i) {
    if (extensions[i] == "GL_CHROMIUM_set_visibility") {
      has_set_visibility = true;
    } else if (extensions[i] == "GL_CHROMIUM_iosurface") {
      has_io_surface = true;
    } else if (extensions[i] == "GL_ARB_texture_rectangle") {
        has_arb_texture_rect = true;
    } else if (extensions[i] == "GL_OES_EGL_image_external") {
        has_egl_image = true;
    } else if (extensions[i] == "GL_CHROMIUM_map_image") {
      has_map_image = true;
    }
  }

  if (has_io_surface)
    DCHECK(has_arb_texture_rect);

  capabilities_.using_set_visibility = has_set_visibility;

  capabilities_.using_egl_image = has_egl_image;

  capabilities_.using_map_image = has_map_image;

  return true;
}

DelegatingRenderer::~DelegatingRenderer() {}

const RendererCapabilities& DelegatingRenderer::Capabilities() const {
  return capabilities_;
}

bool DelegatingRenderer::CanReadPixels() const { return false; }

static ResourceProvider::ResourceId AppendToArray(
    ResourceProvider::ResourceIdArray* array,
    ResourceProvider::ResourceId id) {
  array->push_back(id);
  return id;
}

void DelegatingRenderer::DrawFrame(
    RenderPassList* render_passes_in_draw_order) {
  TRACE_EVENT0("cc", "DelegatingRenderer::DrawFrame");

  DCHECK(!frame_for_swap_buffers_.delegated_frame_data);

  frame_for_swap_buffers_.metadata = client_->MakeCompositorFrameMetadata();

  frame_for_swap_buffers_.delegated_frame_data =
      make_scoped_ptr(new DelegatedFrameData);
  DelegatedFrameData& out_data = *frame_for_swap_buffers_.delegated_frame_data;
  // Move the render passes and resources into the |out_frame|.
  out_data.render_pass_list.swap(*render_passes_in_draw_order);

  // Collect all resource ids in the render passes into a ResourceIdArray.
  ResourceProvider::ResourceIdArray resources;
  DrawQuad::ResourceIteratorCallback append_to_array =
      base::Bind(&AppendToArray, &resources);
  for (size_t i = 0; i < out_data.render_pass_list.size(); ++i) {
    RenderPass* render_pass = out_data.render_pass_list.at(i);
    for (size_t j = 0; j < render_pass->quad_list.size(); ++j)
      render_pass->quad_list[j]->IterateResources(append_to_array);
  }
  resource_provider_->PrepareSendToParent(resources, &out_data.resource_list);
}

void DelegatingRenderer::SwapBuffers() {
  TRACE_EVENT0("cc", "DelegatingRenderer::SwapBuffers");

  output_surface_->SwapBuffers(&frame_for_swap_buffers_);
  frame_for_swap_buffers_.delegated_frame_data.reset();
}

void DelegatingRenderer::GetFramebufferPixels(void* pixels, gfx::Rect rect) {
  NOTREACHED();
}

void DelegatingRenderer::ReceiveSwapBuffersAck(
    const CompositorFrameAck& ack) {
  resource_provider_->ReceiveReturnsFromParent(ack.resources);
}

bool DelegatingRenderer::IsContextLost() {
  ContextProvider* context_provider = output_surface_->context_provider();
  if (!context_provider)
    return false;
  return context_provider->Context3d()->getGraphicsResetStatusARB() !=
         GL_NO_ERROR;
}

void DelegatingRenderer::SetVisible(bool visible) {
  if (visible == visible_)
    return;

  visible_ = visible;
  ContextProvider* context_provider = output_surface_->context_provider();
  if (!visible_) {
    TRACE_EVENT0("cc", "DelegatingRenderer::SetVisible dropping resources");
    resource_provider_->ReleaseCachedData();
    if (context_provider)
      context_provider->Context3d()->flush();
  }
  if (capabilities_.using_set_visibility) {
    // We loop visibility to the GPU process, since that's what manages memory.
    // That will allow it to feed us with memory allocations that we can act
    // upon.
    DCHECK(context_provider);
    context_provider->Context3d()->setVisibilityCHROMIUM(visible);
  }
}

void DelegatingRenderer::SendManagedMemoryStats(size_t bytes_visible,
                                                size_t bytes_visible_and_nearby,
                                                size_t bytes_allocated) {
  ContextProvider* context_provider = output_surface_->context_provider();
  if (!context_provider) {
    // TODO(piman): software path.
    NOTIMPLEMENTED();
    return;
  }
  WebKit::WebGraphicsManagedMemoryStats stats;
  stats.bytesVisible = bytes_visible;
  stats.bytesVisibleAndNearby = bytes_visible_and_nearby;
  stats.bytesAllocated = bytes_allocated;
  stats.backbufferRequested = false;
  context_provider->Context3d()->sendManagedMemoryStatsCHROMIUM(&stats);
}

void DelegatingRenderer::SetDiscardBackBufferWhenNotVisible(bool discard) {
  // Nothing to do, we don't have a back buffer.
}

}  // namespace cc
