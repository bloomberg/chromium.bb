// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_platform_context_3d_impl.h"

#include "base/bind.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/renderer/pepper/pepper_parent_context_provider.h"
#include "content/renderer/render_thread_impl.h"
#include "googleurl/src/gurl.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/ipc/command_buffer_proxy.h"
#include "ppapi/c/pp_graphics_3d.h"
#include "ui/gl/gpu_preference.h"

#ifdef ENABLE_GPU

namespace content {

PlatformContext3DImpl::PlatformContext3DImpl(
    PepperParentContextProvider* parent_context_provider)
      : parent_context_provider_(parent_context_provider),
        parent_texture_id_(0),
        has_alpha_(false),
        command_buffer_(NULL),
        weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
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

bool PlatformContext3DImpl::Init(const int32* attrib_list,
                                 PlatformContext3D* share_context) {
  // Ignore initializing more than once.
  if (command_buffer_)
    return true;

  if (!parent_context_provider_)
    return false;

  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  if (!render_thread)
    return false;

  gfx::GpuPreference gpu_preference = gfx::PreferDiscreteGpu;

  channel_ = render_thread->EstablishGpuChannelSync(
      CAUSE_FOR_GPU_LAUNCH_PEPPERPLATFORMCONTEXT3DIMPL_INITIALIZE);
  if (!channel_.get())
    return false;
  DCHECK(channel_->state() == GpuChannelHost::kConnected);

  gfx::Size surface_size;
  std::vector<int32> attribs;
  // TODO(alokp): Change GpuChannelHost::CreateOffscreenCommandBuffer()
  // interface to accept width and height in the attrib_list so that
  // we do not need to filter for width and height here.
  if (attrib_list) {
    for (const int32_t* attr = attrib_list;
         attr[0] != PP_GRAPHICS3DATTRIB_NONE;
         attr += 2) {
      switch (attr[0]) {
        case PP_GRAPHICS3DATTRIB_WIDTH:
          surface_size.set_width(attr[1]);
          break;
        case PP_GRAPHICS3DATTRIB_HEIGHT:
          surface_size.set_height(attr[1]);
          break;
        case PP_GRAPHICS3DATTRIB_GPU_PREFERENCE:
          gpu_preference =
              (attr[1] == PP_GRAPHICS3DATTRIB_GPU_PREFERENCE_LOW_POWER) ?
                  gfx::PreferIntegratedGpu : gfx::PreferDiscreteGpu;
          break;
        case PP_GRAPHICS3DATTRIB_ALPHA_SIZE:
          has_alpha_ = attr[1] > 0;
        // fall-through
        default:
          attribs.push_back(attr[0]);
          attribs.push_back(attr[1]);
          break;
      }
    }
    attribs.push_back(PP_GRAPHICS3DATTRIB_NONE);
  }

  CommandBufferProxyImpl* share_buffer = NULL;
  if (share_context) {
    PlatformContext3DImpl* share_impl =
        static_cast<PlatformContext3DImpl*>(share_context);
    share_buffer = share_impl->command_buffer_;
  }

  command_buffer_ = channel_->CreateOffscreenCommandBuffer(
      surface_size,
      share_buffer,
      "*",
      attribs,
      GURL::EmptyGURL(),
      gpu_preference);
  if (!command_buffer_)
    return false;

  command_buffer_->SetChannelErrorCallback(
      base::Bind(&PlatformContext3DImpl::OnContextLost,
                 weak_ptr_factory_.GetWeakPtr()));
  command_buffer_->SetOnConsoleMessageCallback(
      base::Bind(&PlatformContext3DImpl::OnConsoleMessage,
                 weak_ptr_factory_.GetWeakPtr()));

  // Fetch the parent context now, after any potential shutdown of the
  // channel due to GPU switching, and creation of the Pepper 3D
  // context with the discrete GPU preference.
  WebGraphicsContext3DCommandBufferImpl* parent_context =
      parent_context_provider_->GetParentContextForPlatformContext3D();
  if (!parent_context)
    return false;

  parent_context_provider_ = NULL;
  parent_context_ = parent_context->AsWeakPtr();

  // Flush any remaining commands in the parent context to make sure the
  // texture id accounting stays consistent.
  gpu::gles2::GLES2Implementation* parent_gles2 =
      parent_context_->GetImplementation();
  parent_gles2->helper()->CommandBufferHelper::Finish();
  parent_texture_id_ = parent_gles2->MakeTextureId();

  CommandBufferProxyImpl* parent_command_buffer =
      parent_context_->GetCommandBufferProxy();
  if (!command_buffer_->SetParent(parent_command_buffer, parent_texture_id_))
    return false;

  return true;
}

void PlatformContext3DImpl::SetParentContext(
    PepperParentContextProvider* parent_context_provider) {
  if (parent_context_.get() && parent_texture_id_ != 0) {
    // Flush any remaining commands in the parent context to make sure the
    // texture id accounting stays consistent.
    gpu::gles2::GLES2Implementation* parent_gles2 =
        parent_context_->GetImplementation();
    parent_gles2->helper()->CommandBufferHelper::Flush();
    parent_gles2->FreeTextureId(parent_texture_id_);
    parent_context_.reset();
    parent_texture_id_ = 0;
  }

  WebGraphicsContext3DCommandBufferImpl* parent_context =
      parent_context_provider->GetParentContextForPlatformContext3D();
  if (!parent_context)
    return;

  parent_context_ = parent_context->AsWeakPtr();
  // Flush any remaining commands in the parent context to make sure the
  // texture id accounting stays consistent.
  gpu::gles2::GLES2Implementation* parent_gles2 =
      parent_context_->GetImplementation();
  parent_gles2->helper()->CommandBufferHelper::Flush();
  parent_texture_id_ = parent_gles2->MakeTextureId();

  CommandBufferProxyImpl* parent_command_buffer =
      parent_context_->GetCommandBufferProxy();
  command_buffer_->SetParent(parent_command_buffer, parent_texture_id_);
}

unsigned PlatformContext3DImpl::GetBackingTextureId() {
  DCHECK(command_buffer_);
  return parent_texture_id_;
}

WebKit::WebGraphicsContext3D* PlatformContext3DImpl::GetParentContext() {
  return parent_context_.get();
}

bool PlatformContext3DImpl::IsOpaque() {
  DCHECK(command_buffer_);
  return !has_alpha_;
}

gpu::CommandBuffer* PlatformContext3DImpl::GetCommandBuffer() {
  return command_buffer_;
}

int PlatformContext3DImpl::GetCommandBufferRouteId() {
  DCHECK(command_buffer_);
  return command_buffer_->GetRouteID();
}

void PlatformContext3DImpl::SetContextLostCallback(const base::Closure& task) {
  context_lost_callback_ = task;
}

void PlatformContext3DImpl::SetOnConsoleMessageCallback(
    const ConsoleMessageCallback& task) {
  console_message_callback_ = task;
}

bool PlatformContext3DImpl::Echo(const base::Closure& task) {
  return command_buffer_->Echo(task);
}

void PlatformContext3DImpl::OnContextLost() {
  DCHECK(command_buffer_);

  if (!context_lost_callback_.is_null())
    context_lost_callback_.Run();
}

void PlatformContext3DImpl::OnConsoleMessage(const std::string& msg, int id) {
  DCHECK(command_buffer_);

  if (!console_message_callback_.is_null())
    console_message_callback_.Run(msg, id);
}

}  // namespace content

#endif  // ENABLE_GPU
