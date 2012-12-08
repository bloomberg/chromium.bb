/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "cc/delegating_renderer.h"

#include <set>
#include <string>
#include <vector>

#include "base/debug/trace_event.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "cc/checkerboard_draw_quad.h"
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
    RendererClient* client, ResourceProvider* resource_provider) {
  scoped_ptr<DelegatingRenderer> renderer(
      new DelegatingRenderer(client, resource_provider));
  if (!renderer->Initialize())
    return scoped_ptr<DelegatingRenderer>();
  return renderer.Pass();
}

DelegatingRenderer::DelegatingRenderer(
    RendererClient* client, ResourceProvider* resource_provider)
    : Renderer(client),
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

void DelegatingRenderer::drawFrame(RenderPassList& render_passes_in_draw_order,
                                   RenderPassIdHashMap& render_passes_by_id) {
  TRACE_EVENT0("cc", "DelegatingRenderer::drawFrame");
  NOTIMPLEMENTED();
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
