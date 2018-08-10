// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/shared_image_interface_proxy.h"

#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/common/gpu_param_traits_macros.h"

namespace gpu {

SharedImageInterfaceProxy::SharedImageInterfaceProxy(GpuChannelHost* host,
                                                     int32_t route_id)
    : host_(host), route_id_(route_id) {}

SharedImageInterfaceProxy::~SharedImageInterfaceProxy() = default;
Mailbox SharedImageInterfaceProxy::CreateSharedImage(
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage) {
  GpuChannelMsg_CreateSharedImage_Params params;
  params.mailbox = Mailbox::Generate();
  params.format = format;
  params.size = size;
  params.color_space = color_space;
  params.usage = usage;
  {
    base::AutoLock lock(lock_);
    params.release_id = ++next_release_id_;
    // Note: we send the IPC under the lock to guarantee monotonicity of the
    // release ids as seen by the service.
    // TODO(piman): send this via GpuChannelMsg_FlushCommandBuffers
    host_->Send(new GpuChannelMsg_CreateSharedImage(route_id_, params));
  }
  return params.mailbox;
}

void SharedImageInterfaceProxy::DestroySharedImage(const SyncToken& sync_token,
                                                   const Mailbox& mailbox) {
  if (sync_token.verified_flush()) {
    // TODO(piman): send this via GpuChannelMsg_FlushCommandBuffers
    host_->Send(new GpuChannelMsg_DestroySharedImage(0, sync_token, mailbox));
    return;
  }
  // Only allow unverified sync tokens for the same channel.
  DCHECK_EQ(sync_token.namespace_id(), gpu::CommandBufferNamespace::GPU_IO);
  int sync_token_channel_id =
      ChannelIdFromCommandBufferId(sync_token.command_buffer_id());
  DCHECK_EQ(sync_token_channel_id, host_->channel_id());
  host_->EnsureFlush(UINT32_MAX);
  SyncToken new_token = sync_token;
  new_token.SetVerifyFlush();
  host_->Send(
      new GpuChannelMsg_DestroySharedImage(route_id_, new_token, mailbox));
}

SyncToken SharedImageInterfaceProxy::GenUnverifiedSyncToken() {
  base::AutoLock lock(lock_);
  return SyncToken(
      CommandBufferNamespace::GPU_IO,
      CommandBufferIdFromChannelAndRoute(host_->channel_id(), route_id_),
      next_release_id_);
}

}  // namespace gpu
