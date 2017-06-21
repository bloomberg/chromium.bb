// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_VIDEO_FRAME_FACTORY_
#define MEDIA_GPU_ANDROID_VIDEO_FRAME_FACTORY_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {
class GpuCommandBufferStub;
}

namespace media {

class CodecOutputBuffer;
class SurfaceTextureGLOwner;
class VideoFrame;

// VideoFrameFactory creates CodecOutputBuffer backed VideoFrames. Not thread
// safe. Virtual for testing; see VideoFrameFactoryImpl.
class MEDIA_GPU_EXPORT VideoFrameFactory {
 public:
  using GetStubCb = base::Callback<gpu::GpuCommandBufferStub*()>;
  using InitCb = base::Callback<void(scoped_refptr<SurfaceTextureGLOwner>)>;
  using FrameCreatedCb = base::Callback<void(const scoped_refptr<VideoFrame>&)>;

  VideoFrameFactory() = default;
  virtual ~VideoFrameFactory() = default;

  // Initializes the factory and runs |init_cb| on the current thread when it's
  // complete. If initialization fails, the returned surface texture will be
  // null.
  virtual void Initialize(
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      GetStubCb get_stub_cb,
      InitCb init_cb) = 0;

  // Creates a new VideoFrame backed by |output_buffer| and |surface_texture|.
  // |surface_texture| may be null if the buffer is backed by an overlay
  // instead. Runs |frame_created_cb| on the current thread to return the frame.
  virtual void CreateVideoFrame(
      std::unique_ptr<CodecOutputBuffer> output_buffer,
      scoped_refptr<SurfaceTextureGLOwner> surface_texture,
      base::TimeDelta timestamp,
      gfx::Size natural_size,
      FrameCreatedCb frame_created_cb) = 0;
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_VIDEO_FRAME_FACTORY_
