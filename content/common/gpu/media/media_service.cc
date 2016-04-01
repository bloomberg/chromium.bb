// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/media_service.h"

#include <utility>

#include "ipc/ipc_message_macros.h"
#include "ipc/param_traits_macros.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/media/gpu_video_decode_accelerator.h"
#include "content/common/gpu/media/gpu_video_encode_accelerator.h"
#include "content/common/gpu/media/media_channel.h"

namespace content {

MediaService::MediaService(GpuChannelManager* channel_manager)
    : channel_manager_(channel_manager) {}

MediaService::~MediaService() {}

void MediaService::AddChannel(int32_t client_id) {
  GpuChannel* gpu_channel = channel_manager_->LookupChannel(client_id);
  DCHECK(gpu_channel);
  scoped_ptr<MediaChannel> media_channel(new MediaChannel(gpu_channel));
  gpu_channel->SetUnhandledMessageListener(media_channel.get());
  media_channels_.set(client_id, std::move(media_channel));
}

void MediaService::RemoveChannel(int32_t client_id) {
  media_channels_.erase(client_id);
}

void MediaService::DestroyAllChannels() {
  media_channels_.clear();
}

}  // namespace content
