// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_IPC_SERVICE_GPU_JPEG_DECODE_ACCELERATOR_H_
#define MEDIA_GPU_IPC_SERVICE_GPU_JPEG_DECODE_ACCELERATOR_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "media/gpu/ipc/service/gpu_jpeg_decode_accelerator_factory_provider.h"
#include "media/gpu/mojo/jpeg_decoder.mojom.h"
#include "media/video/jpeg_decode_accelerator.h"

namespace media {

// TODO(c.padhi): Move GpuJpegDecodeAccelerator to media/gpu/mojo, see
// http://crbug.com/699255.
// Implementation of a mojom::GpuJpegDecodeAccelerator which runs in the GPU
// process, and wraps a JpegDecodeAccelerator.
class GpuJpegDecodeAccelerator : public mojom::GpuJpegDecodeAccelerator,
                                 public JpegDecodeAccelerator::Client {
 public:
  static void Create(mojom::GpuJpegDecodeAcceleratorRequest request);

  ~GpuJpegDecodeAccelerator() override;

  // JpegDecodeAccelerator::Client implementation.
  void VideoFrameReady(int32_t buffer_id) override;
  void NotifyError(int32_t buffer_id,
                   JpegDecodeAccelerator::Error error) override;

 private:
  // This constructor internally calls
  // GpuJpegDecodeAcceleratorFactoryProvider::GetAcceleratorFactories() to
  // fill |accelerator_factory_functions_|.
  GpuJpegDecodeAccelerator();

  // mojom::GpuJpegDecodeAccelerator implementation.
  void Initialize(InitializeCallback callback) override;
  void Decode(const BitstreamBuffer& input_buffer,
              const gfx::Size& coded_size,
              mojo::ScopedSharedBufferHandle output_handle,
              uint32_t output_buffer_size,
              DecodeCallback callback) override;
  void Uninitialize() override;

  void NotifyDecodeStatus(int32_t bitstream_buffer_id,
                          JpegDecodeAccelerator::Error error);

  const std::vector<
      GpuJpegDecodeAcceleratorFactoryProvider::CreateAcceleratorCB>
      accelerator_factory_functions_;

  DecodeCallback decode_cb_;

  std::unique_ptr<JpegDecodeAccelerator> accelerator_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(GpuJpegDecodeAccelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_IPC_SERVICE_GPU_JPEG_DECODE_ACCELERATOR_H_
