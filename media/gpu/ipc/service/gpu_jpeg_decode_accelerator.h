// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_IPC_SERVICE_GPU_JPEG_DECODE_ACCELERATOR_H_
#define MEDIA_GPU_IPC_SERVICE_GPU_JPEG_DECODE_ACCELERATOR_H_

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
class FilteredSender;
}

namespace media {

class GpuJpegDecodeAcceleratorFactoryProvider {
 public:
  using CreateAcceleratorCB =
      base::Callback<std::unique_ptr<JpegDecodeAccelerator>(
          scoped_refptr<base::SingleThreadTaskRunner>)>;

  // Static query for JPEG supported. This query calls the appropriate
  // platform-specific version.
  static bool IsAcceleratedJpegDecodeSupported();

  static std::vector<CreateAcceleratorCB> GetAcceleratorFactories();
};

class GpuJpegDecodeAccelerator
    : public IPC::Sender,
      public base::NonThreadSafe,
      public base::SupportsWeakPtr<GpuJpegDecodeAccelerator> {
 public:
  // |channel| must outlive this object.
  // This convenience constructor internally calls
  // GpuJpegDecodeAcceleratorFactoryProvider::GetAcceleratorFactories() to
  // fill |accelerator_factory_functions_|.
  GpuJpegDecodeAccelerator(
      gpu::FilteredSender* channel,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  // |channel| must outlive this object.
  GpuJpegDecodeAccelerator(
      gpu::FilteredSender* channel,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      std::vector<GpuJpegDecodeAcceleratorFactoryProvider::CreateAcceleratorCB>
          accelerator_factory_functions);
  ~GpuJpegDecodeAccelerator() override;

  void AddClient(int32_t route_id, base::Callback<void(bool)> response);

  void NotifyDecodeStatus(int32_t route_id,
                          int32_t bitstream_buffer_id,
                          JpegDecodeAccelerator::Error error);

  // Function to delegate sending to actual sender.
  bool Send(IPC::Message* message) override;

 private:
  class Client;
  class MessageFilter;

  void ClientRemoved();

  std::vector<GpuJpegDecodeAcceleratorFactoryProvider::CreateAcceleratorCB>
      accelerator_factory_functions_;

  gpu::FilteredSender* channel_;

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

}  // namespace media

#endif  // MEDIA_GPU_IPC_SERVICE_GPU_JPEG_DECODE_ACCELERATOR_H_
