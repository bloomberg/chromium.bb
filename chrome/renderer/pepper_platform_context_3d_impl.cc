// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper_platform_context_3d_impl.h"

#include "chrome/renderer/ggl/ggl.h"
#include "chrome/renderer/gpu_channel_host.h"
#include "chrome/renderer/render_thread.h"

#ifdef ENABLE_GPU
PlatformContext3DImpl::PlatformContext3DImpl(ggl::Context* parent_context)
      : parent_context_(parent_context),
  context_(NULL) {
}

PlatformContext3DImpl::~PlatformContext3DImpl() {
  if (context_) {
    ggl::DestroyContext(context_);
    context_ = NULL;
  }
}

bool PlatformContext3DImpl::Init() {
  // Ignore initializing more than once.
  if (context_)
    return true;

  RenderThread* render_thread = RenderThread::current();
  if (!render_thread)
    return false;

  GpuChannelHost* host = render_thread->GetGpuChannel();
  if (!host)
    return false;

  DCHECK(host->state() == GpuChannelHost::kConnected);

  // TODO(apatrick): Let Pepper plugins configure their back buffer surface.
  static const int32 attribs[] = {
    ggl::GGL_ALPHA_SIZE, 8,
    ggl::GGL_DEPTH_SIZE, 24,
    ggl::GGL_STENCIL_SIZE, 8,
    ggl::GGL_SAMPLES, 0,
    ggl::GGL_SAMPLE_BUFFERS, 0,
    ggl::GGL_NONE,
  };

  // TODO(apatrick): Decide which extensions to expose to Pepper plugins.
  // Currently they get only core GLES2.
  context_ = ggl::CreateOffscreenContext(host,
                                         parent_context_,
                                         gfx::Size(1, 1),
                                         "",
                                         attribs);
  if (!context_)
    return false;

  return true;
}

bool PlatformContext3DImpl::SwapBuffers() {
  DCHECK(context_);
  return ggl::SwapBuffers(context_);
}

unsigned PlatformContext3DImpl::GetError() {
  DCHECK(context_);
  return ggl::GetError(context_);
}

void PlatformContext3DImpl::SetSwapBuffersCallback(Callback0::Type* callback) {
  DCHECK(context_);
  ggl::SetSwapBuffersCallback(context_, callback);
}

unsigned PlatformContext3DImpl::GetBackingTextureId() {
  DCHECK(context_);
  return ggl::GetParentTextureId(context_);
}

gpu::gles2::GLES2Implementation*
    PlatformContext3DImpl::GetGLES2Implementation() {
  DCHECK(context_);
  return ggl::GetImplementation(context_);
}

#endif  // ENABLE_GPU

