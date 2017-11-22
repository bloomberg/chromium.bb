// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_GPU_JPEG_DECODE_ACCELERATOR_H_
#define MEDIA_MOJO_SERVICES_GPU_JPEG_DECODE_ACCELERATOR_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "media/gpu/gpu_jpeg_decode_accelerator_factory.h"
#include "media/mojo/interfaces/jpeg_decode_accelerator.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "media/video/jpeg_decode_accelerator.h"

namespace media {

// TODO(c.padhi): Rename to MojoJpegDecodeAcceleratorService, see
// http://crbug.com/699255.
// Implementation of a mojom::JpegDecodeAccelerator which runs in the GPU
// process, and wraps a JpegDecodeAccelerator.
class MEDIA_MOJO_EXPORT GpuJpegDecodeAccelerator
    : public mojom::JpegDecodeAccelerator,
      public JpegDecodeAccelerator::Client {
 public:
  static void Create(mojom::JpegDecodeAcceleratorRequest request);

  ~GpuJpegDecodeAccelerator() override;

  // JpegDecodeAccelerator::Client implementation.
  void VideoFrameReady(int32_t buffer_id) override;
  void NotifyError(int32_t buffer_id,
                   ::media::JpegDecodeAccelerator::Error error) override;

 private:
  // This constructor internally calls
  // GpuJpegDecodeAcceleratorFactory::GetAcceleratorFactories() to
  // fill |accelerator_factory_functions_|.
  GpuJpegDecodeAccelerator();

  // mojom::JpegDecodeAccelerator implementation.
  void Initialize(InitializeCallback callback) override;
  void Decode(const BitstreamBuffer& input_buffer,
              const gfx::Size& coded_size,
              mojo::ScopedSharedBufferHandle output_handle,
              uint32_t output_buffer_size,
              DecodeCallback callback) override;
  void Uninitialize() override;

  void NotifyDecodeStatus(int32_t bitstream_buffer_id,
                          ::media::JpegDecodeAccelerator::Error error);

  const std::vector<GpuJpegDecodeAcceleratorFactory::CreateAcceleratorCB>
      accelerator_factory_functions_;

  DecodeCallback decode_cb_;

  std::unique_ptr<::media::JpegDecodeAccelerator> accelerator_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(GpuJpegDecodeAccelerator);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_GPU_JPEG_DECODE_ACCELERATOR_H_
