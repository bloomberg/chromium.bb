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
    // Note: we enqueue the IPC under the lock to guarantee monotonicity of the
    // release ids as seen by the service.
    host_->EnqueueDeferredMessage(
        GpuChannelMsg_CreateSharedImage(route_id_, params));
  }
  return params.mailbox;
}

void SharedImageInterfaceProxy::DestroySharedImage(const SyncToken& sync_token,
                                                   const Mailbox& mailbox) {
  SyncToken new_token = sync_token;
  if (!new_token.verified_flush()) {
    // Only allow unverified sync tokens for the same channel.
    DCHECK_EQ(sync_token.namespace_id(), gpu::CommandBufferNamespace::GPU_IO);
    int sync_token_channel_id =
        ChannelIdFromCommandBufferId(sync_token.command_buffer_id());
    DCHECK_EQ(sync_token_channel_id, host_->channel_id());
    new_token.SetVerifyFlush();
  }
  host_->EnqueueDeferredMessage(
      GpuChannelMsg_DestroySharedImage(route_id_, mailbox), {new_token});
}

SyncToken SharedImageInterfaceProxy::GenUnverifiedSyncToken() {
  base::AutoLock lock(lock_);
  return SyncToken(
      CommandBufferNamespace::GPU_IO,
      CommandBufferIdFromChannelAndRoute(host_->channel_id(), route_id_),
      next_release_id_);
}

}  // namespace gpu
