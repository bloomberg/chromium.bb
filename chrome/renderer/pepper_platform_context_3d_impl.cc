// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper_platform_context_3d_impl.h"

#include "chrome/renderer/command_buffer_proxy.h"
#include "chrome/renderer/ggl/ggl.h"
#include "chrome/renderer/gpu_channel_host.h"
#include "chrome/renderer/render_thread.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"

#ifdef ENABLE_GPU
PlatformContext3DImpl::PlatformContext3DImpl(ggl::Context* parent_context)
      : parent_context_(parent_context),
        command_buffer_(NULL),
        callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PlatformContext3DImpl::~PlatformContext3DImpl() {
  if (command_buffer_) {
    DCHECK(channel_.get());
    channel_->DestroyCommandBuffer(command_buffer_);
    command_buffer_ = NULL;
  }

  channel_ = NULL;

  if (parent_context_ && parent_texture_id_ != 0) {
    ggl::GetImplementation(parent_context_)->FreeTextureId(parent_texture_id_);
  }

}

bool PlatformContext3DImpl::Init() {
  // Ignore initializing more than once.
  if (command_buffer_)
    return true;

  RenderThread* render_thread = RenderThread::current();
  if (!render_thread)
    return false;

  channel_ = render_thread->GetGpuChannel();
  if (!channel_.get())
    return false;

  DCHECK(channel_->state() == GpuChannelHost::kConnected);

  // Flush any remaining commands in the parent context to make sure the
  // texture id accounting stays consistent.
  gpu::gles2::GLES2Implementation* parent_gles2 =
      ggl::GetImplementation(parent_context_);
  parent_gles2->helper()->CommandBufferHelper::Finish();
  parent_texture_id_ = parent_gles2->MakeTextureId();

  // TODO(apatrick): Let Pepper plugins configure their back buffer surface.
  static const int32 kAttribs[] = {
    ggl::GGL_ALPHA_SIZE, 8,
    ggl::GGL_DEPTH_SIZE, 24,
    ggl::GGL_STENCIL_SIZE, 8,
    ggl::GGL_SAMPLES, 0,
    ggl::GGL_SAMPLE_BUFFERS, 0,
    ggl::GGL_NONE,
  };
  std::vector<int32> attribs(kAttribs, kAttribs + ARRAYSIZE_UNSAFE(kAttribs));
  CommandBufferProxy* parent_command_buffer =
      ggl::GetCommandBufferProxy(parent_context_);
  command_buffer_ = channel_->CreateOffscreenCommandBuffer(
      parent_command_buffer,
      gfx::Size(1, 1),
      "*",
      attribs,
      parent_texture_id_);
  command_buffer_->SetChannelErrorCallback(callback_factory_.NewCallback(
      &PlatformContext3DImpl::OnContextLost));

  if (!command_buffer_)
    return false;

  return true;
}

void PlatformContext3DImpl::SetSwapBuffersCallback(Callback0::Type* callback) {
  DCHECK(command_buffer_);
  command_buffer_->SetSwapBuffersCallback(callback);
}

unsigned PlatformContext3DImpl::GetBackingTextureId() {
  DCHECK(command_buffer_);
  return parent_texture_id_;
}

gpu::CommandBuffer* PlatformContext3DImpl::GetCommandBuffer() {
  return command_buffer_;
}

void PlatformContext3DImpl::SetContextLostCallback(Callback0::Type* callback) {
    context_lost_callback_.reset(callback);
}

void PlatformContext3DImpl::OnContextLost() {
  DCHECK(command_buffer_);

  // We will lose the parent context soon (it will be reallocated by the main
  // page).
  parent_context_ = NULL;
  parent_texture_id_ = 0;
  if (context_lost_callback_.get())
    context_lost_callback_->Run();
}

#endif  // ENABLE_GPU

