// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_PLATFORM_VIDEO_FRAME_H_
#define MEDIA_GPU_PLATFORM_VIDEO_FRAME_H_

#include "media/base/video_frame.h"
#include "ui/gfx/buffer_types.h"

namespace media {
namespace gpu {

// Create platform dependent media::VideoFrame. |buffer_usage| is passed to
// CreateNativePixmap(). See //media/base/video_frame.h for other parameters.
scoped_refptr<VideoFrame> CreatePlatformVideoFrame(
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    gfx::BufferUsage buffer_usage,
    base::TimeDelta timestamp);

}  // namespace gpu
}  // namespace media

#endif  // MEDIA_GPU_PLATFORM_VIDEO_FRAME_H_
