// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_VIDEO_ACCELERATOR_GPU_ARC_VIDEO_DECODE_ACCELERATOR_DEPRECATED_H_
#define COMPONENTS_ARC_VIDEO_ACCELERATOR_GPU_ARC_VIDEO_DECODE_ACCELERATOR_DEPRECATED_H_

#include <memory>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/arc/common/video_decode_accelerator_deprecated.mojom.h"
#include "components/arc/video_accelerator/arc_video_decode_accelerator_deprecated.h"
#include "components/arc/video_accelerator/video_frame_plane.h"
#include "gpu/command_buffer/service/gpu_preferences.h"

namespace arc {

class ProtectedBufferManager;

// GpuArcVideoDecodeAcceleratorDeprecated manages life-cycle
// and IPC message translation for ArcVideoDecodeAcceleratorDeprecated.
//
// For each creation request from GpuArcVideoDecodeAcceleratorHost,
// GpuArcVideoDecodeAcceleratorDeprecated will create a new IPC channel.
class GpuArcVideoDecodeAcceleratorDeprecated
    : public ::arc::mojom::VideoDecodeAcceleratorDeprecated,
      public ArcVideoDecodeAcceleratorDeprecated::Client {
 public:
  GpuArcVideoDecodeAcceleratorDeprecated(
      const gpu::GpuPreferences& gpu_preferences,
      ProtectedBufferManager* protected_buffer_manager);
  ~GpuArcVideoDecodeAcceleratorDeprecated() override;

 private:
  // ArcVideoDecodeAcceleratorDeprecated::Client implementation.
  void OnError(ArcVideoDecodeAcceleratorDeprecated::Result error) override;
  void OnBufferDone(PortType port,
                    uint32_t index,
                    const BufferMetadata& metadata) override;
  void OnFlushDone() override;
  void OnResetDone() override;
  void OnOutputFormatChanged(const VideoFormat& format) override;

  // ::arc::mojom::VideoDecodeAcceleratorDeprecated implementation.
  void Initialize(
      ::arc::mojom::VideoDecodeAcceleratorConfigDeprecatedPtr config,
      ::arc::mojom::VideoDecodeClientDeprecatedPtr client,
      InitializeCallback callback) override;
  void AllocateProtectedBuffer(
      ::arc::mojom::PortType port,
      uint32_t index,
      mojo::ScopedHandle handle,
      uint64_t size,
      AllocateProtectedBufferCallback callback) override;
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

  std::unique_ptr<ArcVideoDecodeAcceleratorDeprecated> accelerator_;
  ::arc::mojom::VideoDecodeClientDeprecatedPtr client_;

  DISALLOW_COPY_AND_ASSIGN(GpuArcVideoDecodeAcceleratorDeprecated);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_VIDEO_ACCELERATOR_GPU_ARC_VIDEO_DECODE_ACCELERATOR_DEPRECATED_H_
