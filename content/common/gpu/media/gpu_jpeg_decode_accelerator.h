// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_GPU_JPEG_DECODE_ACCELERATOR_H_
#define CONTENT_COMMON_GPU_MEDIA_GPU_JPEG_DECODE_ACCELERATOR_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/non_thread_safe.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "media/video/jpeg_decode_accelerator.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace gpu {
class GpuChannel;
}

namespace content {
class GpuJpegDecodeAccelerator
    : public IPC::Sender,
      public base::NonThreadSafe,
      public base::SupportsWeakPtr<GpuJpegDecodeAccelerator> {
 public:
  // |channel| must outlive this object.
  GpuJpegDecodeAccelerator(
      gpu::GpuChannel* channel,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);
  ~GpuJpegDecodeAccelerator() override;

  void AddClient(int32_t route_id, base::Callback<void(bool)> response);

  void NotifyDecodeStatus(int32_t route_id,
                          int32_t bitstream_buffer_id,
                          media::JpegDecodeAccelerator::Error error);

  // Function to delegate sending to actual sender.
  bool Send(IPC::Message* message) override;

  // Static query for JPEG supported. This query calls the appropriate
  // platform-specific version.
  static bool IsSupported();

 private:
  using CreateJDAFp = std::unique_ptr<media::JpegDecodeAccelerator> (*)(
      const scoped_refptr<base::SingleThreadTaskRunner>&);

  class Client;
  class MessageFilter;

  void ClientRemoved();

  static std::unique_ptr<media::JpegDecodeAccelerator> CreateV4L2JDA(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);
  static std::unique_ptr<media::JpegDecodeAccelerator> CreateVaapiJDA(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);

  // The lifetime of objects of this class is managed by a gpu::GpuChannel. The
  // GpuChannels destroy all the GpuJpegDecodeAccelerator that they own when
  // they are destroyed. So a raw pointer is safe.
  gpu::GpuChannel* channel_;

  // The message filter to run JpegDecodeAccelerator::Decode on IO thread.
  scoped_refptr<MessageFilter> filter_;

  // GPU child task runner.
  scoped_refptr<base::SingleThreadTaskRunner> child_task_runner_;

  // GPU IO task runner.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Number of clients added to |filter_|.
  int client_number_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(GpuJpegDecodeAccelerator);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_GPU_JPEG_DECODE_ACCELERATOR_H_
