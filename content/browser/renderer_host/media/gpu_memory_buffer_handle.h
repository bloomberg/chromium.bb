// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_GPU_MEMORY_BUFFER_HANDLE_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_GPU_MEMORY_BUFFER_HANDLE_H_

#include "media/capture/video/video_capture_buffer_handle.h"

namespace content {

class GpuMemoryBufferTracker;

// A simple proxy that implements the BufferHandle interface, providing
// accessors to GpuMemoryBufferTracker's memory and properties.
class GpuMemoryBufferBufferHandle : public media::VideoCaptureBufferHandle {
 public:
  // |tracker| must outlive GpuMemoryBufferBufferHandle. This is ensured since
  // a tracker is pinned until ownership of this GpuMemoryBufferBufferHandle
  // is returned to VideoCaptureBufferPool.
  explicit GpuMemoryBufferBufferHandle(GpuMemoryBufferTracker* tracker);
  ~GpuMemoryBufferBufferHandle() override;

  gfx::Size dimensions() const override;
  size_t mapped_size() const override;
  void* data(int plane) override;
  ClientBuffer AsClientBuffer(int plane) override;
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  base::FileDescriptor AsPlatformFile() override;
#endif

 private:
  GpuMemoryBufferTracker* const tracker_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_GPU_MEMORY_BUFFER_HANDLE_H_
