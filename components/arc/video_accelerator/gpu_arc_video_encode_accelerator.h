// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_VIDEO_ACCELERATOR_GPU_ARC_VIDEO_ENCODE_ACCELERATOR_H_
#define COMPONENTS_ARC_VIDEO_ACCELERATOR_GPU_ARC_VIDEO_ENCODE_ACCELERATOR_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "components/arc/common/video_encode_accelerator.mojom.h"
#include "components/arc/video_accelerator/video_frame_plane.h"
#include "gpu/config/gpu_preferences.h"
#include "media/video/video_encode_accelerator.h"

namespace arc {

// GpuArcVideoEncodeAccelerator manages life-cycle and IPC message translation
// for media::VideoEncodeAccelerator.
class GpuArcVideoEncodeAccelerator
    : public ::arc::mojom::VideoEncodeAccelerator,
      public media::VideoEncodeAccelerator::Client {
 public:
  explicit GpuArcVideoEncodeAccelerator(
      const gpu::GpuPreferences& gpu_preferences);
  ~GpuArcVideoEncodeAccelerator() override;

 private:
  using VideoPixelFormat = media::VideoPixelFormat;
  using VideoCodecProfile = media::VideoCodecProfile;
  using Error = media::VideoEncodeAccelerator::Error;
  using VideoEncodeClientPtr = ::arc::mojom::VideoEncodeClientPtr;

  // VideoEncodeAccelerator::Client implementation.
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override;
  void BitstreamBufferReady(
      int32_t bitstream_buffer_id,
      const media::BitstreamBufferMetadata& metadata) override;
  void NotifyError(Error error) override;

  // ::arc::mojom::VideoEncodeAccelerator implementation.
  void GetSupportedProfiles(GetSupportedProfilesCallback callback) override;
  void Initialize(VideoPixelFormat input_format,
                  const gfx::Size& visible_size,
                  VideoEncodeAccelerator::StorageType input_storage,
                  VideoCodecProfile output_profile,
                  uint32_t initial_bitrate,
                  VideoEncodeClientPtr client,
                  InitializeCallback callback) override;
  void Encode(mojo::ScopedHandle fd,
              std::vector<::arc::VideoFramePlane> planes,
              int64_t timestamp,
              bool force_keyframe,
              EncodeCallback callback) override;
  void UseBitstreamBuffer(mojo::ScopedHandle shmem_fd,
                          uint32_t offset,
                          uint32_t size,
                          UseBitstreamBufferCallback callback) override;
  void RequestEncodingParametersChange(uint32_t bitrate,
                                       uint32_t framerate) override;
  void Flush(FlushCallback callback) override;

  gpu::GpuPreferences gpu_preferences_;
  std::unique_ptr<media::VideoEncodeAccelerator> accelerator_;
  ::arc::mojom::VideoEncodeClientPtr client_;
  gfx::Size coded_size_;
  gfx::Size visible_size_;
  VideoPixelFormat input_pixel_format_;
  int32_t bitstream_buffer_serial_;
  std::unordered_map<uint32_t, UseBitstreamBufferCallback> use_bitstream_cbs_;

  DISALLOW_COPY_AND_ASSIGN(GpuArcVideoEncodeAccelerator);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_VIDEO_ACCELERATOR_GPU_ARC_VIDEO_ENCODE_ACCELERATOR_H_
