// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_MEDIA_CHANNEL_H_
#define CONTENT_COMMON_GPU_MEDIA_MEDIA_CHANNEL_H_

#include <memory>

#include "content/common/gpu/media/gpu_jpeg_decode_accelerator.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "media/video/video_decode_accelerator.h"

namespace media {
struct CreateVideoEncoderParams;
}

namespace gpu {
class GpuChannel;
class GpuCommandBufferStub;
}

namespace content {

class MediaChannelDispatchHelper;

class MediaChannel : public IPC::Listener, public IPC::Sender {
 public:
  explicit MediaChannel(gpu::GpuChannel* channel);
  ~MediaChannel() override;

  // IPC::Sender implementation:
  bool Send(IPC::Message* msg) override;

 private:
  friend class MediaChannelDispatchHelper;

  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& message) override;

  // Message handlers.
  void OnCreateJpegDecoder(int32_t route_id, IPC::Message* reply_msg);
  void OnCreateVideoDecoder(int32_t command_buffer_route_id,
                            const media::VideoDecodeAccelerator::Config& config,
                            int32_t route_id,
                            IPC::Message* reply_message);
  void OnCreateVideoEncoder(int32_t command_buffer_route_id,
                            const media::CreateVideoEncoderParams& params,
                            IPC::Message* reply_message);

  gpu::GpuChannel* const channel_;
  std::unique_ptr<GpuJpegDecodeAccelerator> jpeg_decoder_;
  DISALLOW_COPY_AND_ASSIGN(MediaChannel);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_MEDIA_CHANNEL_H_
