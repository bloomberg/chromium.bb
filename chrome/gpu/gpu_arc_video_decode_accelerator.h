// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_GPU_ARC_VIDEO_DECODE_ACCELERATOR_H_
#define CHROME_GPU_GPU_ARC_VIDEO_DECODE_ACCELERATOR_H_

#include <memory>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "chrome/gpu/arc_video_decode_accelerator.h"
#include "components/arc/common/video_decode_accelerator.mojom.h"
#include "components/arc/video_accelerator/video_accelerator.h"
#include "gpu/command_buffer/service/gpu_preferences.h"

namespace chromeos {
namespace arc {

// GpuArcVideoDecodeAccelerator manages life-cycle and IPC message translation
// for ArcVideoDecodeAccelerator.
//
// For each creation request from GpuArcVideoDecodeAcceleratorHost,
// GpuArcVideoDecodeAccelerator will create a new IPC channel.
class GpuArcVideoDecodeAccelerator
    : public ::arc::mojom::VideoDecodeAccelerator,
      public ArcVideoDecodeAccelerator::Client {
 public:
  explicit GpuArcVideoDecodeAccelerator(
      const gpu::GpuPreferences& gpu_preferences);
  ~GpuArcVideoDecodeAccelerator() override;

 private:
  // ArcVideoDecodeAccelerator::Client implementation.
  void OnError(ArcVideoDecodeAccelerator::Result error) override;
  void OnBufferDone(PortType port,
                    uint32_t index,
                    const BufferMetadata& metadata) override;
  void OnFlushDone() override;
  void OnResetDone() override;
  void OnOutputFormatChanged(const VideoFormat& format) override;

  // ::arc::mojom::VideoDecodeAccelerator implementation.
  void Initialize(::arc::mojom::VideoDecodeAcceleratorConfigPtr config,
                  ::arc::mojom::VideoDecodeClientPtr client,
                  const InitializeCallback& callback) override;
  void BindSharedMemory(::arc::mojom::PortType port,
                        uint32_t index,
                        mojo::ScopedHandle ashmem_handle,
                        uint32_t offset,
                        uint32_t length) override;
  void BindDmabuf(::arc::mojom::PortType port,
                  uint32_t index,
                  mojo::ScopedHandle dmabuf_handle,
                  std::vector<::arc::VideoFramePlane> planes) override;
  void UseBuffer(::arc::mojom::PortType port,
                 uint32_t index,
                 ::arc::mojom::BufferMetadataPtr metadata) override;
  void SetNumberOfOutputBuffers(uint32_t number) override;
  void Flush() override;
  void Reset() override;

  base::ScopedFD UnwrapFdFromMojoHandle(mojo::ScopedHandle handle);

  THREAD_CHECKER(thread_checker_);

  gpu::GpuPreferences gpu_preferences_;
  std::unique_ptr<ArcVideoDecodeAccelerator> accelerator_;
  ::arc::mojom::VideoDecodeClientPtr client_;

  DISALLOW_COPY_AND_ASSIGN(GpuArcVideoDecodeAccelerator);
};

}  // namespace arc
}  // namespace chromeos

#endif  // CHROME_GPU_GPU_ARC_VIDEO_DECODE_ACCELERATOR_H_
