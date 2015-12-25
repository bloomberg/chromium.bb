// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GPU_JPEG_DECODE_ACCELERATOR_HOST_H_
#define CONTENT_COMMON_GPU_CLIENT_GPU_JPEG_DECODE_ACCELERATOR_HOST_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "media/video/jpeg_decode_accelerator.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace IPC {
class Listener;
class Message;
}

namespace content {
class GpuChannelHost;

// This class is used to talk to JpegDecodeAccelerator in the GPU process
// through IPC messages.
class GpuJpegDecodeAcceleratorHost : public media::JpegDecodeAccelerator,
                                     public base::NonThreadSafe {
 public:
  // VideoCaptureGpuJpegDecoder owns |this| and |channel|. And
  // VideoCaptureGpuJpegDecoder delete |this| before |channel|. So |this| is
  // guaranteed not to outlive |channel|.
  GpuJpegDecodeAcceleratorHost(
      GpuChannelHost* channel,
      int32_t route_id,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);
  ~GpuJpegDecodeAcceleratorHost() override;

  // media::JpegDecodeAccelerator implementation.
  // |client| is called on the IO thread, but is never called into after the
  // GpuJpegDecodeAcceleratorHost is destroyed.
  bool Initialize(media::JpegDecodeAccelerator::Client* client) override;
  void Decode(const media::BitstreamBuffer& bitstream_buffer,
              const scoped_refptr<media::VideoFrame>& video_frame) override;
  bool IsSupported() override;

  base::WeakPtr<IPC::Listener> GetReceiver();

 private:
  class Receiver;

  void Send(IPC::Message* message);

  // Unowned reference to the GpuChannelHost to send IPC messages to the GPU
  // process.
  GpuChannelHost* channel_;

  // Route ID for the associated decoder in the GPU process.
  int32_t decoder_route_id_;

  // GPU IO task runner.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  scoped_ptr<Receiver> receiver_;

  DISALLOW_COPY_AND_ASSIGN(GpuJpegDecodeAcceleratorHost);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_JPEG_DECODE_ACCELERATOR_HOST_H_
