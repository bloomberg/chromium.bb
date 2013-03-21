// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/delegating_renderer.h"

#include <set>
#include <string>
#include <vector>

#include "base/debug/trace_event.h"
#include "base/string_util.h"
#include "base/strings/string_split.h"
#include "cc/output/compositor_frame.h"
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
  // TODO(danakj): Throttling - we may want to only allow 1 outstanding frame,
  // but the parent compositor may pipeline for us.
  // TODO(danakj): Can we use this in single-thread mode?
  capabilities_.using_swap_complete_callback = true;
  capabilities_.max_texture_size = resource_provider_->max_texture_size();
  capabilities_.best_texture_format = resource_provider_->best_texture_format();
  capabilities_.allow_partial_texture_updates = false;

  WebGraphicsContext3D* context3d = resource_provider_->GraphicsContext3D();

  if (!context3d) {
    // Software compositing.
    return true;
  }

  if (!context3d->makeContextCurrent())
    return false;

  context3d->setContextLostCallback(this);
  context3d->pushGroupMarkerEXT("CompositorContext");

  std::string extensionsString =
      UTF16ToASCII(context3d->getString(GL_EXTENSIONS));

  std::vector<std::string> extensions;
  base::SplitString(extensionsString, ' ', &extensions);

  // TODO(danakj): We need non-GPU-specific paths for these things. This
  // renderer shouldn't need to use context3d extensions directly.
  bool hasReadBGRA = true;
  bool hasSetVisibility = true;
  bool hasIOSurface = true;
  bool hasARBTextureRect = true;
  bool hasGpuMemoryManager = true;
  bool hasEGLImage = true;
  for (size_t i = 0; i < extensions.size(); ++i) {
    if (extensions[i] == "GL_EXT_read_format_bgra")
      hasReadBGRA = true;
    else if (extensions[i] == "GL_CHROMIUM_set_visibility")
      hasSetVisibility = true;
    else if (extensions[i] == "GL_CHROMIUM_iosurface")
      hasIOSurface = true;
    else if (extensions[i] == "GL_ARB_texture_rectangle")
      hasARBTextureRect = true;
    else if (extensions[i] == "GL_CHROMIUM_gpu_memory_manager")
      hasGpuMemoryManager = true;
    else if (extensions[i] == "GL_OES_EGL_image_external")
      hasEGLImage = true;
  }

  if (hasIOSurface)
    DCHECK(hasARBTextureRect);

  capabilities_.using_accelerated_painting =
      Settings().accelerate_painting &&
      capabilities_.best_texture_format == GL_BGRA_EXT &&
      hasReadBGRA;

  // TODO(piman): loop visibility to GPU process?
  capabilities_.using_set_visibility = hasSetVisibility;

  // TODO(danakj): Support GpuMemoryManager.
  capabilities_.using_gpu_memory_manager = false;

  capabilities_.using_egl_image = hasEGLImage;

  return true;
}

DelegatingRenderer::~DelegatingRenderer() {
  WebGraphicsContext3D* context3d = resource_provider_->GraphicsContext3D();
  if (context3d)
    context3d->setContextLostCallback(NULL);
}

const RendererCapabilities& DelegatingRenderer::Capabilities() const {
  return capabilities_;
}

static ResourceProvider::ResourceId AppendToArray(
    ResourceProvider::ResourceIdArray* array,
    ResourceProvider::ResourceId id) {
  array->push_back(id);
  return id;
}

void DelegatingRenderer::DrawFrame(
    RenderPassList& render_passes_in_draw_order) {
  TRACE_EVENT0("cc", "DelegatingRenderer::drawFrame");

  CompositorFrame out_frame;
  out_frame.metadata = client_->MakeCompositorFrameMetadata();

  out_frame.delegated_frame_data = make_scoped_ptr(new DelegatedFrameData);

  // Collect all resource ids in the render passes into a ResourceIdArray.
  ResourceProvider::ResourceIdArray resources;
  DrawQuad::ResourceIteratorCallback append_to_array =
      base::Bind(&AppendToArray, &resources);
  for (size_t i = 0; i < render_passes_in_draw_order.size(); ++i) {
    RenderPass* render_pass = render_passes_in_draw_order[i];
    for (size_t j = 0; j < render_pass->quad_list.size(); ++j)
      render_pass->quad_list[j]->IterateResources(append_to_array);
  }

  // Move the render passes and resources into the |out_frame|.
  DelegatedFrameData& out_data = *out_frame.delegated_frame_data;
  out_data.render_pass_list.swap(render_passes_in_draw_order);
  resource_provider_->PrepareSendToParent(resources, &out_data.resource_list);

  output_surface_->SendFrameToParentCompositor(&out_frame);
}

bool DelegatingRenderer::SwapBuffers() {
  return true;
}

void DelegatingRenderer::GetFramebufferPixels(void* pixels, gfx::Rect rect) {
  NOTIMPLEMENTED();
}

void DelegatingRenderer::ReceiveCompositorFrameAck(
    const CompositorFrameAck& ack) {
  resource_provider_->ReceiveFromParent(ack.resources);
  if (client_->HasImplThread())
    client_->OnSwapBuffersComplete();
}


bool DelegatingRenderer::IsContextLost() {
  WebGraphicsContext3D* context3d = resource_provider_->GraphicsContext3D();
  if (!context3d)
    return false;
  return context3d->getGraphicsResetStatusARB() != GL_NO_ERROR;
}

void DelegatingRenderer::SetVisible(bool visible) {
  visible_ = visible;
}

void DelegatingRenderer::onContextLost() {
  client_->DidLoseOutputSurface();
}

}  // namespace cc
