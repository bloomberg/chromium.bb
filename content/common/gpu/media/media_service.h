// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_MEDIA_SERVICE_H_
#define CONTENT_COMMON_GPU_MEDIA_MEDIA_SERVICE_H_

#include <stdint.h>

#include <memory>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/macros.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "media/video/video_decode_accelerator.h"

namespace gpu {
class GpuChannel;
class GpuChannelManager;
}

namespace content {

class MediaChannel;

class MediaService {
 public:
  MediaService(gpu::GpuChannelManager* channel_manager);
  ~MediaService();

  void AddChannel(int32_t client_id);
  void RemoveChannel(int32_t client_id);
  void DestroyAllChannels();

 private:
  gpu::GpuChannelManager* const channel_manager_;
  base::ScopedPtrHashMap<int32_t, std::unique_ptr<MediaChannel>>
      media_channels_;
  DISALLOW_COPY_AND_ASSIGN(MediaService);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_MEDIA_SERVICE_H_
