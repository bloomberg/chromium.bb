// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_decode_accelerator_stub.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/image_decode_accelerator_worker.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"

namespace gpu {

ImageDecodeAcceleratorStub::ImageDecodeAcceleratorStub(
    ImageDecodeAcceleratorWorker* worker,
    GpuChannel* channel,
    int32_t route_id)
    : worker_(worker),
      channel_(channel),
      sequence_(channel->scheduler()->CreateSequence(SchedulingPriority::kLow)),
      sync_point_client_state_(
          channel->sync_point_manager()->CreateSyncPointClientState(
              CommandBufferNamespace::GPU_IO,
              CommandBufferIdFromChannelAndRoute(channel->client_id(),
                                                 route_id),
              sequence_)),
      main_task_runner_(channel->task_runner()),
      io_task_runner_(channel->io_task_runner()) {
  // We need the sequence to be initially disabled so that when we schedule a
  // task to release the decode sync token, it doesn't run immediately (we want
  // it to run when the decode is done).
  channel_->scheduler()->DisableSequence(sequence_);
}

bool ImageDecodeAcceleratorStub::OnMessageReceived(const IPC::Message& msg) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (!base::FeatureList::IsEnabled(
          features::kVaapiJpegImageDecodeAcceleration)) {
    return false;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ImageDecodeAcceleratorStub, msg)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_ScheduleImageDecode,
                        OnScheduleImageDecode)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ImageDecodeAcceleratorStub::Shutdown() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(lock_);
  sync_point_client_state_->Destroy();
  channel_->scheduler()->DestroySequence(sequence_);
  channel_ = nullptr;
}

ImageDecodeAcceleratorStub::~ImageDecodeAcceleratorStub() {
  DCHECK(!channel_);
}

void ImageDecodeAcceleratorStub::OnScheduleImageDecode(
    const GpuChannelMsg_ScheduleImageDecode_Params& decode_params,
    uint64_t release_count) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(lock_);
  if (!channel_) {
    // The channel is no longer available, so don't schedule a decode.
    return;
  }

  // Make sure the decode sync token is ordered with respect to the last decode
  // request.
  if (release_count <= last_release_count_) {
    DLOG(ERROR) << "Out-of-order decode sync token";
    OnError();
    return;
  }
  last_release_count_ = release_count;

  // Make sure the output dimensions are not too small.
  if (decode_params.output_size.IsEmpty()) {
    DLOG(ERROR) << "Output dimensions are too small";
    OnError();
    return;
  }

  // Start the actual decode.
  worker_->Decode(std::move(decode_params.encoded_data),
                  decode_params.output_size,
                  base::BindOnce(&ImageDecodeAcceleratorStub::OnDecodeCompleted,
                                 base::WrapRefCounted(this)));

  // Schedule a task to eventually release the decode sync token. Note that this
  // task won't run until the sequence is re-enabled when a decode completes.
  channel_->scheduler()->ScheduleTask(Scheduler::Task(
      sequence_,
      base::BindOnce(&ImageDecodeAcceleratorStub::ProcessCompletedDecode,
                     base::WrapRefCounted(this), std::move(decode_params),
                     release_count),
      std::vector<SyncToken>()));
}

void ImageDecodeAcceleratorStub::ProcessCompletedDecode(
    GpuChannelMsg_ScheduleImageDecode_Params params,
    uint64_t decode_release_count) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(lock_);
  if (!channel_) {
    // The channel is no longer available, so don't do anything.
    return;
  }

  // TODO(andrescj): create the transfer cache entry. Doing so will also upload
  // the decoded image to a GPU texture.

  sync_point_client_state_->ReleaseFenceSync(decode_release_count);

  // If there are no more completed decodes to be processed, we can disable the
  // sequence: when the next decode is completed, the sequence will be
  // re-enabled.
  pending_completed_decodes_.pop();
  if (pending_completed_decodes_.empty())
    channel_->scheduler()->DisableSequence(sequence_);
}

void ImageDecodeAcceleratorStub::OnDecodeCompleted(
    std::vector<uint8_t> rgba_output) {
  base::AutoLock lock(lock_);
  if (!channel_) {
    // The channel is no longer available, so don't do anything.
    return;
  }

  if (!accepting_completed_decodes_) {
    // We're still waiting for the channel to be destroyed because of an earlier
    // failure, so don't do anything.
    return;
  }

  if (rgba_output.empty()) {
    DLOG(ERROR) << "The decode failed";
    OnError();
    return;
  }

  pending_completed_decodes_.push(std::move(rgba_output));

  // We only need to enable the sequence when the number of pending completed
  // decodes is 1. If there are more, the sequence should already be enabled.
  if (pending_completed_decodes_.size() == 1u)
    channel_->scheduler()->EnableSequence(sequence_);
}

void ImageDecodeAcceleratorStub::OnError() {
  DCHECK(channel_);

  // Trigger the destruction of the channel and stop processing further
  // completed decodes, even if they're successful. We can't call
  // GpuChannel::OnChannelError() directly because that will end up calling
  // ImageDecodeAcceleratorStub::Shutdown() while |lock_| is still acquired. So,
  // we post a task to the main thread instead.
  accepting_completed_decodes_ = false;
  channel_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuChannel::OnChannelError, channel_->AsWeakPtr()));
}

}  // namespace gpu
