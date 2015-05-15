// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_GPU_MEMORY_BUFFER_VIDEO_FRAME_POOL_H_
#define MEDIA_VIDEO_GPU_MEMORY_BUFFER_VIDEO_FRAME_POOL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/base/video_frame.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {
class GpuVideoAcceleratorFactories;

// Interface to a pool of GpuMemoryBuffers/textues/images that can be used to
// transform software VideoFrames to VideoFrames backed by native textures.
// The resources used by the VideoFrame created by the pool will be
// automatically put back into the pool once the frame is destroyed.
// The pool recycles resources to a void unnecessarily allocating and
// destroying textures, images and GpuMemoryBuffer that could result
// in a round trip to the browser/GPU process.
class MEDIA_EXPORT GpuMemoryBufferVideoFramePool {
 public:
  GpuMemoryBufferVideoFramePool(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      const scoped_refptr<GpuVideoAcceleratorFactories>& gpu_factories);
  ~GpuMemoryBufferVideoFramePool();

  // Returns a new VideoFrame containing only mailboxes to native resources.
  // The content of the returned object is copied from the software-allocated
  // |video_frame|.
  // If it's not possible to create a new hardware VideoFrame, |video_frame|
  // itself will be returned.
  scoped_refptr<VideoFrame> MaybeCreateHardwareFrame(
      const scoped_refptr<VideoFrame>& video_frame);

 private:
  class PoolImpl;
  scoped_refptr<PoolImpl> pool_impl_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferVideoFramePool);
};

}  // namespace media

#endif  // MEDIA_VIDEO_GPU_MEMORY_BUFFER_VIDEO_FRAME_POOL_H_
