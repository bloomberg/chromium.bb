// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_GPU_ARC_VIDEO_SERVICE_H_
#define CHROME_GPU_GPU_ARC_VIDEO_SERVICE_H_

#include <memory>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "chrome/gpu/arc_video_accelerator.h"
#include "components/arc/common/video_accelerator.mojom.h"
#include "components/arc/video_accelerator/video_accelerator.h"
#include "gpu/command_buffer/service/gpu_preferences.h"

namespace chromeos {
namespace arc {

// GpuArcVideoService manages life-cycle and IPC message translation for
// ArcVideoAccelerator.
//
// For each creation request from GpuArcVideoServiceHost, GpuArcVideoService
// will create a new IPC channel.
class GpuArcVideoService : public ::arc::mojom::VideoAcceleratorService,
                           public ArcVideoAccelerator::Client {
 public:
  explicit GpuArcVideoService(const gpu::GpuPreferences& gpu_preferences);
  ~GpuArcVideoService() override;

 private:
  // ArcVideoAccelerator::Client implementation.
  void OnError(ArcVideoAccelerator::Result error) override;
  void OnBufferDone(PortType port,
                    uint32_t index,
                    const BufferMetadata& metadata) override;
  void OnFlushDone() override;
  void OnResetDone() override;
  void OnOutputFormatChanged(const VideoFormat& format) override;

  // ::arc::mojom::VideoAcceleratorService implementation.
  void Initialize(::arc::mojom::ArcVideoAcceleratorConfigPtr config,
                  ::arc::mojom::VideoAcceleratorServiceClientPtr client,
                  const InitializeCallback& callback) override;
  void BindSharedMemory(::arc::mojom::PortType port,
                        uint32_t index,
                        mojo::ScopedHandle ashmem_handle,
                        uint32_t offset,
                        uint32_t length) override;
  void BindDmabuf(::arc::mojom::PortType port,
                  uint32_t index,
                  mojo::ScopedHandle dmabuf_handle,
                  std::vector<::arc::ArcVideoAcceleratorDmabufPlane>
                      dmabuf_planes) override;
  void UseBuffer(::arc::mojom::PortType port,
                 uint32_t index,
                 ::arc::mojom::BufferMetadataPtr metadata) override;
  void SetNumberOfOutputBuffers(uint32_t number) override;
  void Flush() override;
  void Reset() override;

  base::ScopedFD UnwrapFdFromMojoHandle(mojo::ScopedHandle handle);

  base::ThreadChecker thread_checker_;

  gpu::GpuPreferences gpu_preferences_;
  std::unique_ptr<ArcVideoAccelerator> accelerator_;
  ::arc::mojom::VideoAcceleratorServiceClientPtr client_;

  DISALLOW_COPY_AND_ASSIGN(GpuArcVideoService);
};

}  // namespace arc
}  // namespace chromeos

#endif  // CHROME_GPU_GPU_ARC_VIDEO_SERVICE_H_
