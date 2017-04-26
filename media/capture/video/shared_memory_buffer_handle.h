// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_SHARED_MEMORY_BUFFER_HANDLE_H_
#define MEDIA_CAPTURE_VIDEO_SHARED_MEMORY_BUFFER_HANDLE_H_

#include "media/capture/capture_export.h"
#include "media/capture/video/video_capture_buffer_handle.h"

namespace media {

// Provides access to memory-mapped shared memory without participating in the
// lifetime management of the memory. Instances are typically handed out by
// an instance of VideoCaptureDevice::Client as part of a
// VideoCaptureDevice::Client::Buffer, which contains a separate
// |access_permission| that guarantees that the memory stays alive. The buffers
// are typically managed by an instance of VideoCaptureBufferPool.
class CAPTURE_EXPORT SharedMemoryBufferHandle
    : public VideoCaptureBufferHandle {
 public:
  explicit SharedMemoryBufferHandle(base::SharedMemory* shared_memory,
                                    size_t mapped_size);
  ~SharedMemoryBufferHandle() override;

  size_t mapped_size() const override;
  uint8_t* data() const override;
  const uint8_t* const_data() const override;

 private:
  base::SharedMemory* const shared_memory_;
  const size_t mapped_size_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_SHARED_MEMORY_BUFFER_HANDLE_H_
