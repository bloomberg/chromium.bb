// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_CHROME_ARC_VIDEO_DECODE_ACCELERATOR_H_
#define CHROME_GPU_CHROME_ARC_VIDEO_DECODE_ACCELERATOR_H_

#include <list>
#include <memory>
#include <queue>
#include <vector>

#include "base/callback.h"
#include "base/threading/thread_checker.h"
#include "chrome/gpu/arc_video_decode_accelerator.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "media/video/video_decode_accelerator.h"

namespace chromeos {
namespace arc {

class ProtectedBufferManager;
class ProtectedBufferHandle;

// This class is executed in the GPU process. It takes decoding requests from
// ARC via IPC channels and translates and sends those requests to an
// implementation of media::VideoDecodeAccelerator. It also returns the decoded
// frames back to the ARC side.
class ChromeArcVideoDecodeAccelerator
    : public ArcVideoDecodeAccelerator,
      public media::VideoDecodeAccelerator::Client,
      public base::SupportsWeakPtr<ChromeArcVideoDecodeAccelerator> {
 public:
  ChromeArcVideoDecodeAccelerator(
      const gpu::GpuPreferences& gpu_preferences,
      ProtectedBufferManager* protected_buffer_manager);
  ~ChromeArcVideoDecodeAccelerator() override;

  // Implementation of the ArcVideoDecodeAccelerator interface.
  ArcVideoDecodeAccelerator::Result Initialize(
      const Config& config,
      ArcVideoDecodeAccelerator::Client* client) override;
  void SetNumberOfOutputBuffers(size_t number) override;
  bool AllocateProtectedBuffer(PortType port,
                               uint32_t index,
                               base::ScopedFD handle_fd,
                               size_t size) override;
  void BindSharedMemory(PortType port,
                        uint32_t index,
                        base::ScopedFD ashmem_fd,
                        off_t offset,
                        size_t length) override;
  void BindDmabuf(PortType port,
                  uint32_t index,
                  base::ScopedFD dmabuf_fd,
                  const std::vector<::arc::VideoFramePlane>& planes) override;
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
    // SharedMemoryHandle for this buffer to be passed to accelerator.
    // In non-secure mode, received via BindSharedMemory from the client,
    // in secure mode, a handle for the SharedMemory in protected_shmem.
    base::SharedMemoryHandle shm_handle;

    // Used only in secure mode; handle to the protected buffer backing
    // this input buffer.
    std::unique_ptr<ProtectedBufferHandle> protected_buffer_handle;

    // Offset to the payload from the beginning of the shared memory buffer.
    off_t offset = 0;

    InputBufferInfo();
    ~InputBufferInfo();
  };

  // The information about the native pixmap used as an output buffer.
  struct OutputBufferInfo {
    // GpuMemoryBufferHandle for this buffer to be passed to accelerator.
    // In non-secure mode, received via BindDmabuf from the client,
    // in secure mode, a handle to the NativePixmap in protected_pixmap.
    gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle;

    // Used only in secure mode; handle to the protected buffer backing
    // this output buffer.
    std::unique_ptr<ProtectedBufferHandle> protected_buffer_handle;

    OutputBufferInfo();
    ~OutputBufferInfo();
  };

  // The helper method to simplify reporting of the status returned to UMA.
  ArcVideoDecodeAccelerator::Result InitializeTask(
      const Config& config,
      ArcVideoDecodeAccelerator::Client* client);

  // Helper function to validate |port| and |index|.
  bool ValidatePortAndIndex(PortType port, uint32_t index) const;

  // Return true if |planes| is valid for a dmabuf |fd|.
  bool VerifyDmabuf(const base::ScopedFD& fd,
                    const std::vector<::arc::VideoFramePlane>& planes) const;

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
  // is longer than this ChromeArcVideoDecodeAccelerator.
  ArcVideoDecodeAccelerator::Client* arc_client_;

  // The next ID for the bitstream buffer, started from 0.
  int32_t next_bitstream_buffer_id_;

  gfx::Size coded_size_;
  gfx::Rect visible_rect_;
  media::VideoPixelFormat output_pixel_format_;

  // A list of most recent |kMaxNumberOfInputRecord| InputRecords.
  // |kMaxNumberOfInputRecord| is defined in the cc file.
  std::list<InputRecord> input_records_;

  // The details of the shared memory of each input buffers.
  std::vector<std::unique_ptr<InputBufferInfo>> input_buffer_info_;

  // To keep those output buffers which have been bound by bindDmabuf() but
  // haven't been passed to VDA yet. Will call VDA::ImportBufferForPicture()
  // when those buffers are used for the first time.
  std::vector<std::unique_ptr<OutputBufferInfo>> buffers_pending_import_;

  THREAD_CHECKER(thread_checker_);
  size_t output_buffer_size_;

  // The minimal number of requested output buffers.
  uint32_t requested_num_of_output_buffers_;

  bool secure_mode_ = false;

  gpu::GpuPreferences gpu_preferences_;
  ProtectedBufferManager* protected_buffer_manager_;

  DISALLOW_COPY_AND_ASSIGN(ChromeArcVideoDecodeAccelerator);
};

}  // namespace arc
}  // namespace chromeos

#endif  // CHROME_GPU_CHROME_ARC_VIDEO_DECODE_ACCELERATOR_H_
