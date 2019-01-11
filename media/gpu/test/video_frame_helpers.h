// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_FRAME_HELPERS_H_
#define MEDIA_GPU_TEST_VIDEO_FRAME_HELPERS_H_

#include "base/memory/scoped_refptr.h"
#include "media/base/video_types.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {

struct GpuMemoryBufferHandle;

}  // namespace gfx

namespace media {

class VideoFrame;

namespace test {

// Create a video frame with specified |pixel_format| and |size|.
scoped_refptr<VideoFrame> CreateVideoFrame(
    VideoPixelFormat pixel_format,
    const gfx::Size& size,
    gfx::BufferUsage buffer_usage = gfx::BufferUsage::SCANOUT_VDA_WRITE);

// Create a shared GPU memory handle to the |video_frame|'s data.
gfx::GpuMemoryBufferHandle CreateGpuMemoryBufferHandle(
    scoped_refptr<VideoFrame> video_frame);

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_FRAME_HELPERS_H_
