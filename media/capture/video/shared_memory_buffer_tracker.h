// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_SHARED_MEMORY_BUFFER_TRACKER_H_
#define MEDIA_CAPTURE_VIDEO_SHARED_MEMORY_BUFFER_TRACKER_H_

#include "media/capture/video/video_capture_buffer_tracker.h"

namespace media {

class SharedMemoryBufferHandle;

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

// A simple proxy that implements the BufferHandle interface, providing
// accessors to SharedMemTracker's memory and properties.
class SharedMemoryBufferHandle : public VideoCaptureBufferHandle {
 public:
  // |tracker| must outlive SimpleBufferHandle. This is ensured since a
  // tracker is pinned until ownership of this SimpleBufferHandle is returned
  // to VideoCaptureBufferPool.
  explicit SharedMemoryBufferHandle(SharedMemoryBufferTracker* tracker);
  ~SharedMemoryBufferHandle() override;

  size_t mapped_size() const override;
  uint8_t* data() override;
  const uint8_t* data() const override;

 private:
  SharedMemoryBufferTracker* const tracker_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_SHARED_MEMORY_BUFFER_TRACKER_H_
