// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper_platform_context_3d_impl.h"

#include "content/renderer/render_thread.h"
#include "content/renderer/gpu/renderer_gl_context.h"
#include "content/renderer/gpu/gpu_channel_host.h"
#include "content/renderer/gpu/command_buffer_proxy.h"
#include "googleurl/src/gurl.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"

#ifdef ENABLE_GPU

PlatformContext3DImpl::PlatformContext3DImpl(RendererGLContext* parent_context)
      : parent_context_(parent_context->AsWeakPtr()),
        parent_texture_id_(0),
        command_buffer_(NULL),
        callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PlatformContext3DImpl::~PlatformContext3DImpl() {
  if (parent_context_.get() && parent_texture_id_ != 0) {
    // Flush any remaining commands in the parent context to make sure the
    // texture id accounting stays consistent.
    gpu::gles2::GLES2Implementation* parent_gles2 =
        parent_context_->GetImplementation();
    parent_gles2->helper()->CommandBufferHelper::Finish();
    parent_gles2->FreeTextureId(parent_texture_id_);
  }

  if (command_buffer_) {
    DCHECK(channel_.get());
    channel_->DestroyCommandBuffer(command_buffer_);
    command_buffer_ = NULL;
  }

  channel_ = NULL;
}

bool PlatformContext3DImpl::Init(const int32* attrib_list) {
  // Ignore initializing more than once.
  if (command_buffer_)
    return true;

  // Parent may already have been deleted.
  if (!parent_context_.get())
    return false;

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
      parent_context_->GetImplementation();
  parent_gles2->helper()->CommandBufferHelper::Finish();
  parent_texture_id_ = parent_gles2->MakeTextureId();

  gfx::Size surface_size;
  std::vector<int32> attribs;
  // TODO(alokp): Change GpuChannelHost::CreateOffscreenCommandBuffer()
  // interface to accept width and height in the attrib_list so that
  // we do not need to filter for width and height here.
  if (attrib_list) {
    for (const int32_t* attr = attrib_list;
         attr[0] != RendererGLContext::NONE;
         attr += 2) {
      switch (attr[0]) {
        case RendererGLContext::WIDTH:
          surface_size.set_width(attr[1]);
          break;
        case RendererGLContext::HEIGHT:
          surface_size.set_height(attr[1]);
          break;
        default:
          attribs.push_back(attr[0]);
          attribs.push_back(attr[1]);
          break;
      }
    }
    attribs.push_back(RendererGLContext::NONE);
  }

  CommandBufferProxy* parent_command_buffer =
      parent_context_->GetCommandBufferProxy();
  command_buffer_ = channel_->CreateOffscreenCommandBuffer(
      surface_size,
      NULL,
      "*",
      attribs,
      GURL::EmptyGURL());
  if (!command_buffer_)
    return false;

  if (!command_buffer_->SetParent(parent_command_buffer, parent_texture_id_))
    return false;

  command_buffer_->SetChannelErrorCallback(callback_factory_.NewCallback(
      &PlatformContext3DImpl::OnContextLost));

  return true;
}

unsigned PlatformContext3DImpl::GetBackingTextureId() {
  DCHECK(command_buffer_);
  return parent_texture_id_;
}

gpu::CommandBuffer* PlatformContext3DImpl::GetCommandBuffer() {
  return command_buffer_;
}

int PlatformContext3DImpl::GetCommandBufferRouteId() {
  DCHECK(command_buffer_);
  return command_buffer_->route_id();
}

void PlatformContext3DImpl::SetContextLostCallback(Callback0::Type* callback) {
  context_lost_callback_.reset(callback);
}

bool PlatformContext3DImpl::Echo(Task* task) {
  return command_buffer_->Echo(task);
}

void PlatformContext3DImpl::OnContextLost() {
  DCHECK(command_buffer_);

  if (context_lost_callback_.get())
    context_lost_callback_->Run();
}

#endif  // ENABLE_GPU
