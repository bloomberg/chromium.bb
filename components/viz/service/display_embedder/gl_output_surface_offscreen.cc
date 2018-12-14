// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/gl_output_surface_offscreen.h"

#include <stdint.h>

#include "base/bind.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gl/gl_utils.h"

namespace viz {
namespace {

constexpr ResourceFormat kFboTextureFormat = RGBA_8888;

}  // namespace

GLOutputSurfaceOffscreen::GLOutputSurfaceOffscreen(
    scoped_refptr<VizProcessContextProvider> context_provider,
    SyntheticBeginFrameSource* synthetic_begin_frame_source)
    : GLOutputSurface(context_provider, synthetic_begin_frame_source),
      weak_ptr_factory_(this) {}

GLOutputSurfaceOffscreen::~GLOutputSurfaceOffscreen() {}

void GLOutputSurfaceOffscreen::EnsureBackbuffer() {
  if (!texture_id_) {
    gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();

    const int max_texture_size =
        context_provider_->ContextCapabilities().max_texture_size;
    int texture_width = std::min(max_texture_size, size_.width());
    int texture_height = std::min(max_texture_size, size_.height());

    // TODO(sgilhuly): Draw to a texture backed by a mailbox.
    gl->GenTextures(1, &texture_id_);
    gl->BindTexture(GL_TEXTURE_2D, texture_id_);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->TexImage2D(GL_TEXTURE_2D, 0, GLInternalFormat(kFboTextureFormat),
                   texture_width, texture_height, 0,
                   GLDataFormat(kFboTextureFormat),
                   GLDataType(kFboTextureFormat), nullptr);
    gl->GenFramebuffers(1, &fbo_);
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, texture_id_, 0);
  }
}

void GLOutputSurfaceOffscreen::DiscardBackbuffer() {
  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();

  if (texture_id_) {
    gl->DeleteTextures(1, &texture_id_);
    texture_id_ = 0;
  }

  if (fbo_) {
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
    gl->DeleteFramebuffers(1, &fbo_);
    fbo_ = 0;
  }
}

void GLOutputSurfaceOffscreen::BindFramebuffer() {
  if (!texture_id_) {
    EnsureBackbuffer();
  } else {
    gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
  }
}

void GLOutputSurfaceOffscreen::Reshape(const gfx::Size& size,
                                       float scale_factor,
                                       const gfx::ColorSpace& color_space,
                                       bool alpha,
                                       bool stencil) {
  size_ = size;
  DiscardBackbuffer();
  EnsureBackbuffer();
}

void GLOutputSurfaceOffscreen::SwapBuffers(OutputSurfaceFrame frame) {
  gfx::Size surface_size = frame.size;
  DCHECK(surface_size == size_);

  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();

  gpu::SyncToken sync_token;
  gl->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  context_provider_->ContextSupport()->SignalSyncToken(
      sync_token,
      base::BindOnce(&GLOutputSurfaceOffscreen::OnSwapBuffersComplete,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(frame.latency_info)));
}

void GLOutputSurfaceOffscreen::OnSwapBuffersComplete(
    std::vector<ui::LatencyInfo> latency_info) {
  latency_tracker()->OnGpuSwapBuffersCompleted(latency_info);
  client()->DidReceiveSwapBuffersAck();
  client()->DidReceivePresentationFeedback(gfx::PresentationFeedback());
}

}  // namespace viz
