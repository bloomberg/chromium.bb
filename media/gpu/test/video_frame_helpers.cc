// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_frame_helpers.h"

#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "media/gpu/platform_video_frame.h"
#include "ui/gfx/gpu_memory_buffer.h"

#if defined(OS_CHROMEOS)
#include "media/base/scopedfd_helper.h"
#endif

namespace media {
namespace test {

scoped_refptr<VideoFrame> CreateVideoFrame(VideoPixelFormat pixel_format,
                                           const gfx::Size& size,
                                           gfx::BufferUsage buffer_usage) {
  scoped_refptr<VideoFrame> video_frame;
#if defined(OS_CHROMEOS)
  gfx::Rect visible_rect(size.width(), size.height());
  video_frame = media::CreatePlatformVideoFrame(
      pixel_format, size, visible_rect, visible_rect.size(), buffer_usage,
      base::TimeDelta());
  LOG_ASSERT(video_frame) << "Failed to create Dmabuf-backed VideoFrame";
#endif
  return video_frame;
}

gfx::GpuMemoryBufferHandle CreateGpuMemoryBufferHandle(
    scoped_refptr<VideoFrame> video_frame) {
  gfx::GpuMemoryBufferHandle handle;
#if defined(OS_CHROMEOS)
  LOG_ASSERT(video_frame);
  handle.type = gfx::NATIVE_PIXMAP;

  const size_t num_planes = VideoFrame::NumPlanes(video_frame->format());
  for (size_t i = 0; i < num_planes; ++i) {
    const auto& plane = video_frame->layout().planes()[i];
    handle.native_pixmap_handle.planes.emplace_back(plane.stride, plane.offset,
                                                    i, plane.modifier);
  }

  std::vector<base::ScopedFD> duped_fds =
      DuplicateFDs(video_frame->DmabufFds());
  for (auto& duped_fd : duped_fds)
    handle.native_pixmap_handle.fds.emplace_back(std::move(duped_fd));

#endif
  return handle;
}

}  // namespace test
}  // namespace media
