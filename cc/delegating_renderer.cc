// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/delegating_renderer.h"

#include <set>
#include <string>
#include <vector>

#include "base/debug/trace_event.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "cc/checkerboard_draw_quad.h"
#include "cc/compositor_frame.h"
#include "cc/compositor_frame_ack.h"
#include "cc/debug_border_draw_quad.h"
#include "cc/render_pass.h"
#include "cc/render_pass_draw_quad.h"
#include "cc/resource_provider.h"
#include "cc/solid_color_draw_quad.h"
#include "cc/texture_draw_quad.h"
#include "cc/tile_draw_quad.h"
#include "cc/yuv_video_draw_quad.h"
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
  capabilities_.usingPartialSwap = false;
  // TODO(danakj): Throttling - we may want to only allow 1 outstanding frame,
  // but the parent compositor may pipeline for us.
  // TODO(danakj): Can we use this in single-thread mode?
  capabilities_.usingSwapCompleteCallback = true;
  capabilities_.maxTextureSize = resource_provider_->maxTextureSize();
  capabilities_.bestTextureFormat = resource_provider_->bestTextureFormat();
  capabilities_.allowPartialTextureUpdates = false;

  WebGraphicsContext3D* context3d = resource_provider_->graphicsContext3D();

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

  capabilities_.usingAcceleratedPainting =
      settings().acceleratePainting &&
      capabilities_.bestTextureFormat == GL_BGRA_EXT &&
      hasReadBGRA;

  // TODO(piman): loop visibility to GPU process?
  capabilities_.usingSetVisibility = hasSetVisibility;

  // TODO(danakj): Support GpuMemoryManager.
  capabilities_.usingGpuMemoryManager = false;

  capabilities_.usingEglImage = hasEGLImage;

  return true;
}

DelegatingRenderer::~DelegatingRenderer() {
  WebGraphicsContext3D* context3d = resource_provider_->graphicsContext3D();
  if (context3d)
    context3d->setContextLostCallback(NULL);
}

const RendererCapabilities& DelegatingRenderer::capabilities() const {
  return capabilities_;
}

void DelegatingRenderer::drawFrame(
    RenderPassList& render_passes_in_draw_order) {
  TRACE_EVENT0("cc", "DelegatingRenderer::drawFrame");

  CompositorFrame out_frame;
  out_frame.metadata = m_client->makeCompositorFrameMetadata();

  out_frame.delegated_frame_data = make_scoped_ptr(new DelegatedFrameData);
  DelegatedFrameData& out_data = *out_frame.delegated_frame_data;

  out_data.size = viewportSize();
  out_data.render_pass_list.swap(render_passes_in_draw_order);

  ResourceProvider::ResourceIdArray resources;
  for (size_t i = 0; i < out_data.render_pass_list.size(); ++i) {
    for (size_t j = 0; j < out_data.render_pass_list[i]->quad_list.size(); ++j)
      out_data.render_pass_list[i]->quad_list[j]->AppendResources(&resources);
  }
  resource_provider_->prepareSendToParent(resources, &out_data.resource_list);

  output_surface_->SendFrameToParentCompositor(&out_frame);
}

bool DelegatingRenderer::swapBuffers() {
  return true;
}

void DelegatingRenderer::getFramebufferPixels(void *pixels,
                                              const gfx::Rect& rect) {
  NOTIMPLEMENTED();
}

void DelegatingRenderer::receiveCompositorFrameAck(
    const CompositorFrameAck& ack) {
  resource_provider_->receiveFromParent(ack.resources);
  m_client->onSwapBuffersComplete();
}


bool DelegatingRenderer::isContextLost() {
  WebGraphicsContext3D* context3d = resource_provider_->graphicsContext3D();
  if (!context3d)
    return false;
  return context3d->getGraphicsResetStatusARB() != GL_NO_ERROR;
}

void DelegatingRenderer::setVisible(bool visible) {
  visible_ = visible;
}

void DelegatingRenderer::onContextLost() {
  m_client->didLoseOutputSurface();
}

}  // namespace cc
