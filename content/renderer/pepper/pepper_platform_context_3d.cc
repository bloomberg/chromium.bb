// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_platform_context_3d.h"

#include "base/bind.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "ppapi/c/pp_graphics_3d.h"
#include "ui/gl/gpu_preference.h"
#include "url/gurl.h"

namespace content {

PlatformContext3D::PlatformContext3D()
    : has_alpha_(false), command_buffer_(NULL), weak_ptr_factory_(this) {}

PlatformContext3D::~PlatformContext3D() {
  if (command_buffer_) {
    DCHECK(channel_.get());
    channel_->DestroyCommandBuffer(command_buffer_);
    command_buffer_ = NULL;
  }

  channel_ = NULL;
}

bool PlatformContext3D::Init(const int32* attrib_list,
                             PlatformContext3D* share_context) {
  // Ignore initializing more than once.
  if (command_buffer_)
    return true;

  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  if (!render_thread)
    return false;

  gfx::GpuPreference gpu_preference = gfx::PreferDiscreteGpu;

  channel_ = render_thread->EstablishGpuChannelSync(
      CAUSE_FOR_GPU_LAUNCH_PEPPERPLATFORMCONTEXT3DIMPL_INITIALIZE);
  if (!channel_.get())
    return false;

  gfx::Size surface_size;
  std::vector<int32> attribs;
  // TODO(alokp): Change GpuChannelHost::CreateOffscreenCommandBuffer()
  // interface to accept width and height in the attrib_list so that
  // we do not need to filter for width and height here.
  if (attrib_list) {
    for (const int32_t* attr = attrib_list; attr[0] != PP_GRAPHICS3DATTRIB_NONE;
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
              (attr[1] == PP_GRAPHICS3DATTRIB_GPU_PREFERENCE_LOW_POWER)
                  ? gfx::PreferIntegratedGpu
                  : gfx::PreferDiscreteGpu;
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
    PlatformContext3D* share_impl =
        static_cast<PlatformContext3D*>(share_context);
    share_buffer = share_impl->command_buffer_;
  }

  command_buffer_ = channel_->CreateOffscreenCommandBuffer(
      surface_size, share_buffer, attribs, GURL::EmptyGURL(), gpu_preference);
  if (!command_buffer_)
    return false;
  if (!command_buffer_->Initialize())
    return false;
  gpu::Mailbox mailbox = gpu::Mailbox::Generate();
  if (!command_buffer_->ProduceFrontBuffer(mailbox))
    return false;
  mailbox_ = mailbox;
  sync_point_ = command_buffer_->InsertSyncPoint();

  command_buffer_->SetChannelErrorCallback(base::Bind(
      &PlatformContext3D::OnContextLost, weak_ptr_factory_.GetWeakPtr()));
  command_buffer_->SetOnConsoleMessageCallback(base::Bind(
      &PlatformContext3D::OnConsoleMessage, weak_ptr_factory_.GetWeakPtr()));

  return true;
}

void PlatformContext3D::GetBackingMailbox(gpu::Mailbox* mailbox,
                                          uint32* sync_point) {
  *mailbox = mailbox_;
  *sync_point = sync_point_;
}

void PlatformContext3D::InsertSyncPointForBackingMailbox() {
  DCHECK(command_buffer_);
  sync_point_ = command_buffer_->InsertSyncPoint();
}

bool PlatformContext3D::IsOpaque() {
  DCHECK(command_buffer_);
  return !has_alpha_;
}

gpu::CommandBuffer* PlatformContext3D::GetCommandBuffer() {
  return command_buffer_;
}

gpu::GpuControl* PlatformContext3D::GetGpuControl() { return command_buffer_; }

int PlatformContext3D::GetCommandBufferRouteId() {
  DCHECK(command_buffer_);
  return command_buffer_->GetRouteID();
}

void PlatformContext3D::SetContextLostCallback(const base::Closure& task) {
  context_lost_callback_ = task;
}

void PlatformContext3D::SetOnConsoleMessageCallback(
    const ConsoleMessageCallback& task) {
  console_message_callback_ = task;
}

void PlatformContext3D::Echo(const base::Closure& task) {
  command_buffer_->Echo(task);
}

void PlatformContext3D::OnContextLost() {
  DCHECK(command_buffer_);

  if (!context_lost_callback_.is_null())
    context_lost_callback_.Run();
}

void PlatformContext3D::OnConsoleMessage(const std::string& msg, int id) {
  DCHECK(command_buffer_);

  if (!console_message_callback_.is_null())
    console_message_callback_.Run(msg, id);
}

}  // namespace content
