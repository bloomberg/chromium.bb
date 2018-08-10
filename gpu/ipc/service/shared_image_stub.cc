// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/shared_image_stub.h"

#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "ui/gl/gl_context.h"

namespace gpu {

SharedImageStub::SharedImageStub(GpuChannel* channel, int32_t route_id)
    : channel_(channel),
      sequence_(channel->scheduler()->CreateSequence(SchedulingPriority::kLow)),
      sync_point_client_state_(
          channel->sync_point_manager()->CreateSyncPointClientState(
              CommandBufferNamespace::GPU_IO,
              CommandBufferIdFromChannelAndRoute(channel->client_id(),
                                                 route_id),
              sequence_)) {}

SharedImageStub::~SharedImageStub() {
  channel_->scheduler()->DestroySequence(sequence_);
  sync_point_client_state_->Destroy();
  if (factory_ && factory_->HasImages()) {
    bool have_context = MakeContextCurrentAndCreateFactory();
    factory_->DestroyAllSharedImages(have_context);
  }
}

bool SharedImageStub::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SharedImageStub, msg)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_CreateSharedImage, OnCreateSharedImage)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_DestroySharedImage, OnDestroySharedImage)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SharedImageStub::OnCreateSharedImage(
    const GpuChannelMsg_CreateSharedImage_Params& params) {
  if (!MakeContextCurrentAndCreateFactory())
    return;

  if (!factory_->CreateSharedImage(params.mailbox, params.format, params.size,
                                   params.color_space, params.usage)) {
    LOG(ERROR) << "SharedImageStub: Unable to create shared image";
    OnError();
    return;
  }
  sync_point_client_state_->ReleaseFenceSync(params.release_id);
}

void SharedImageStub::OnDestroySharedImage(const gpu::SyncToken& sync_token,
                                           const Mailbox& mailbox) {
  // The Scheduler guaranteed this only executes after the sync_token was
  // released.
  DCHECK(channel_->sync_point_manager()->IsSyncTokenReleased(sync_token));
  if (!MakeContextCurrentAndCreateFactory())
    return;

  if (!factory_->DestroySharedImage(mailbox)) {
    LOG(ERROR) << "SharedImageStub: Unable to destroy shared image";
    OnError();
    return;
  }
}

bool SharedImageStub::MakeContextCurrentAndCreateFactory() {
  if (!factory_) {
    auto* channel_manager = channel_->gpu_channel_manager();
    DCHECK(!context_state_);
    ContextResult result;
    context_state_ = channel_manager->GetRasterDecoderContextState(&result);
    if (result != ContextResult::kSuccess) {
      LOG(ERROR) << "SharedImageStub: unable to create context";
      OnError();
      return false;
    }
    DCHECK(context_state_);
    DCHECK(!context_state_->context_lost);
    DCHECK(context_state_->context->IsCurrent(nullptr));
    gpu::GpuMemoryBufferFactory* gmb_factory =
        channel_manager->gpu_memory_buffer_factory();
    factory_ = std::make_unique<SharedImageFactory>(
        channel_manager->gpu_preferences(),
        channel_manager->gpu_driver_bug_workarounds(),
        channel_manager->gpu_feature_info(), channel_manager->mailbox_manager(),
        gmb_factory ? gmb_factory->AsImageFactory() : nullptr, this);
    return true;
  } else {
    DCHECK(context_state_);
    if (context_state_->context_lost) {
      LOG(ERROR) << "SharedImageStub: context already lost";
      OnError();
      return false;
    } else {
      if (context_state_->context->MakeCurrent(context_state_->surface.get()))
        return true;
      context_state_->context_lost = true;
      LOG(ERROR) << "SharedImageStub: MakeCurrent failed";
      OnError();
      return false;
    }
  }
}

void SharedImageStub::OnError() {
  channel_->OnChannelError();
}

void SharedImageStub::TrackMemoryAllocatedChange(uint64_t delta) {
  size_ += delta;
}

uint64_t SharedImageStub::GetSize() const {
  return size_;
}

uint64_t SharedImageStub::ClientTracingId() const {
  return channel_->client_tracing_id();
}

int SharedImageStub::ClientId() const {
  return channel_->client_id();
}

uint64_t SharedImageStub::ShareGroupTracingGUID() const {
  return sync_point_client_state_->command_buffer_id().GetUnsafeValue();
}

}  // namespace gpu
