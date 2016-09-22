// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_BUFFER_HANDLE_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_BUFFER_HANDLE_H_

#include "base/files/file.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace content {

// Abstraction of a pool's buffer data buffer and size for clients.
class VideoCaptureBufferHandle {
 public:
  virtual ~VideoCaptureBufferHandle() {}
  virtual gfx::Size dimensions() const = 0;
  virtual size_t mapped_size() const = 0;
  virtual void* data(int plane) = 0;
  virtual ClientBuffer AsClientBuffer(int plane) = 0;
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  virtual base::FileDescriptor AsPlatformFile() = 0;
#endif
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_BUFFER_HANDLE_H_
