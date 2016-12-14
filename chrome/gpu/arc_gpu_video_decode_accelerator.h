// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_ARC_GPU_VIDEO_DECODE_ACCELERATOR_H_
#define CHROME_GPU_ARC_GPU_VIDEO_DECODE_ACCELERATOR_H_

#include <list>
#include <memory>
#include <queue>
#include <vector>

#include "base/callback.h"
#include "base/threading/thread_checker.h"
#include "chrome/gpu/arc_video_accelerator.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "media/video/video_decode_accelerator.h"

namespace chromeos {
namespace arc {

// This class is executed in the GPU process. It takes decoding requests from
// ARC via IPC channels and translates and sends those requests to an
// implementation of media::VideoDecodeAccelerator. It also returns the decoded
// frames back to the ARC side.
class ArcGpuVideoDecodeAccelerator
    : public ArcVideoAccelerator,
      public media::VideoDecodeAccelerator::Client,
      public base::SupportsWeakPtr<ArcGpuVideoDecodeAccelerator> {
 public:
  explicit ArcGpuVideoDecodeAccelerator(
      const gpu::GpuPreferences& gpu_preferences);
  ~ArcGpuVideoDecodeAccelerator() override;

  // Implementation of the ArcVideoAccelerator interface.
  ArcVideoAccelerator::Result Initialize(
      const Config& config,
      ArcVideoAccelerator::Client* client) override;
  void SetNumberOfOutputBuffers(size_t number) override;
  void BindSharedMemory(PortType port,
                        uint32_t index,
                        base::ScopedFD ashmem_fd,
                        off_t offset,
                        size_t length) override;
  void BindDmabuf(PortType port,
                  uint32_t index,
                  base::ScopedFD dmabuf_fd,
                  const std::vector<::arc::ArcVideoAcceleratorDmabufPlane>&
                      dmabuf_planes) override;
  void UseBuffer(PortType port,
                 uint32_t index,
                 const BufferMetadata& metadata) override;
  void Reset() override;
  void Flush() override;

  // Implementation of the VideoDecodeAccelerator::Client interface.
  void ProvidePictureBuffers(uint32_t requested_num_of_buffers,
                             media::VideoPixelFormat output_format,
                             uint32_t textures_per_buffer,
                             const gfx::Size& dimensions,
                             uint32_t texture_target) override;
  void DismissPictureBuffer(int32_t picture_buffer) override;
  void PictureReady(const media::Picture& picture) override;
  void NotifyEndOfBitstreamBuffer(int32_t bitstream_buffer_id) override;
  void NotifyFlushDone() override;
  void NotifyResetDone() override;
  void NotifyError(media::VideoDecodeAccelerator::Error error) override;

 private:
  // Some information related to a bitstream buffer. The information is required
  // when input or output buffers are returned back to the client.
  struct InputRecord {
    int32_t bitstream_buffer_id;
    uint32_t buffer_index;
    int64_t timestamp;

    InputRecord(int32_t bitstream_buffer_id,
                uint32_t buffer_index,
                int64_t timestamp);
  };

  // The information about the shared memory used as an input buffer.
  struct InputBufferInfo {
    // The file handle to access the buffer. It is owned by this class and
    // should be closed after use.
    base::ScopedFD handle;

    // The offset of the payload to the beginning of the shared memory.
    off_t offset = 0;

    // The size of the payload in bytes.
    size_t length = 0;

    InputBufferInfo();
    InputBufferInfo(InputBufferInfo&& other);
    ~InputBufferInfo();
  };

  // The information about the dmabuf used as an output buffer.
  struct OutputBufferInfo {
    base::ScopedFD handle;
    std::vector<::arc::ArcVideoAcceleratorDmabufPlane> planes;

    OutputBufferInfo();
    OutputBufferInfo(OutputBufferInfo&& other);
    ~OutputBufferInfo();
  };

  // The helper method to simplify reporting of the status returned to UMA.
  ArcVideoAccelerator::Result InitializeTask(
      const Config& config,
      ArcVideoAccelerator::Client* client);

  // Helper function to validate |port| and |index|.
  bool ValidatePortAndIndex(PortType port, uint32_t index) const;

  // Return true if |dmabuf_planes| is valid for a dmabuf |fd|.
  bool VerifyDmabuf(const base::ScopedFD& fd,
                    const std::vector<::arc::ArcVideoAcceleratorDmabufPlane>&
                        dmabuf_planes) const;

  // Creates an InputRecord for the given |bitstream_buffer_id|. The
  // |buffer_index| is the index of the associated input buffer. The |timestamp|
  // is the time the video frame should be displayed.
  void CreateInputRecord(int32_t bitstream_buffer_id,
                         uint32_t buffer_index,
                         int64_t timestamp);

  // Finds the InputRecord which matches to given |bitstream_buffer_id|.
  // Returns |nullptr| if it cannot be found.
  InputRecord* FindInputRecord(int32_t bitstream_buffer_id);

  // Notify the client when output format changes.
  void NotifyOutputFormatChanged();

  // Global counter that keeps track the number of active clients (i.e., how
  // many VDAs in use by this class).
  // Since this class only works on the same thread, it's safe to access
  // |client_count_| without lock.
  static int client_count_;

  std::unique_ptr<media::VideoDecodeAccelerator> vda_;

  // It's safe to use the pointer here, the life cycle of the |arc_client_|
  // is longer than this ArcGpuVideoDecodeAccelerator.
  ArcVideoAccelerator::Client* arc_client_;

  // The next ID for the bitstream buffer, started from 0.
  int32_t next_bitstream_buffer_id_;

  gfx::Size coded_size_;
  gfx::Rect visible_rect_;
  media::VideoPixelFormat output_pixel_format_;

  // A list of most recent |kMaxNumberOfInputRecord| InputRecords.
  // |kMaxNumberOfInputRecord| is defined in the cc file.
  std::list<InputRecord> input_records_;

  // The details of the shared memory of each input buffers.
  std::vector<InputBufferInfo> input_buffer_info_;

  // To keep those output buffers which have been bound by bindDmabuf() but
  // haven't been passed to VDA yet. Will call VDA::ImportBufferForPicture()
  // when those buffers are used for the first time.
  std::vector<OutputBufferInfo> buffers_pending_import_;

  base::ThreadChecker thread_checker_;
  size_t output_buffer_size_;

  // The minimal number of requested output buffers.
  uint32_t requested_num_of_output_buffers_;

  gpu::GpuPreferences gpu_preferences_;

  DISALLOW_COPY_AND_ASSIGN(ArcGpuVideoDecodeAccelerator);
};

}  // namespace arc
}  // namespace chromeos

#endif  // CHROME_GPU_ARC_GPU_VIDEO_DECODE_ACCELERATOR_H_
