// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_SHARED_MEMORY_BUFFER_TRACKER_H_
#define MEDIA_CAPTURE_VIDEO_SHARED_MEMORY_BUFFER_TRACKER_H_

#include "media/capture/video/shared_memory_buffer_handle.h"
#include "media/capture/video/video_capture_buffer_tracker.h"

namespace media {

// Tracker specifics for SharedMemory.
class SharedMemoryBufferTracker final : public VideoCaptureBufferTracker {
 public:
  SharedMemoryBufferTracker();

  bool Init(const gfx::Size& dimensions,
            VideoPixelFormat format,
            VideoPixelStorage storage_type,
            base::Lock* lock) override;

  std::unique_ptr<VideoCaptureBufferHandle> GetMemoryMappedAccess() override;
  mojo::ScopedSharedBufferHandle GetHandleForTransit() override;
  base::SharedMemoryHandle GetNonOwnedSharedMemoryHandleForLegacyIPC() override;

 private:
  friend class SharedMemoryBufferHandle;

  // The memory created to be shared with renderer processes.
  base::SharedMemory shared_memory_;
  size_t mapped_size_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_SHARED_MEMORY_BUFFER_TRACKER_H_
