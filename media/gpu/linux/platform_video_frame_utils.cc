// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/linux/platform_video_frame_utils.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/scoped_file.h"
#include "media/base/scopedfd_helper.h"
#include "media/base/video_frame_layout.h"
#include "media/gpu/format_utils.h"
#include "ui/gfx/gpu_memory_buffer.h"

#if defined(USE_OZONE)
#include "ui/gfx/native_pixmap.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif

namespace media {

namespace {

#if defined(USE_OZONE)
scoped_refptr<VideoFrame> CreateVideoFrameOzone(VideoPixelFormat pixel_format,
                                                const gfx::Size& coded_size,
                                                const gfx::Rect& visible_rect,
                                                const gfx::Size& natural_size,
                                                gfx::BufferUsage buffer_usage,
                                                base::TimeDelta timestamp) {
  ui::OzonePlatform* platform = ui::OzonePlatform::GetInstance();
  DCHECK(platform);
  ui::SurfaceFactoryOzone* factory = platform->GetSurfaceFactoryOzone();
  DCHECK(factory);

  gfx::BufferFormat buffer_format =
      VideoPixelFormatToGfxBufferFormat(pixel_format);
  auto pixmap = factory->CreateNativePixmap(
      gfx::kNullAcceleratedWidget, coded_size, buffer_format, buffer_usage);

  const size_t num_planes = VideoFrame::NumPlanes(pixel_format);
  std::vector<VideoFrameLayout::Plane> planes(num_planes);
  std::vector<size_t> buffer_sizes(num_planes);
  for (size_t i = 0; i < num_planes; ++i) {
    planes[i].stride = pixmap->GetDmaBufPitch(i);
    planes[i].offset = pixmap->GetDmaBufOffset(i);
    planes[i].modifier = pixmap->GetDmaBufModifier(i);
    buffer_sizes[i] = planes[i].offset +
                      planes[i].stride * VideoFrame::Rows(i, pixel_format,
                                                          coded_size.height());
  }

  auto layout = VideoFrameLayout::CreateWithPlanes(
      pixel_format, coded_size, std::move(planes), std::move(buffer_sizes));
  if (!layout)
    return nullptr;

  std::vector<base::ScopedFD> dmabuf_fds;
  for (size_t i = 0; i < num_planes; ++i) {
    int duped_fd = HANDLE_EINTR(dup(pixmap->GetDmaBufFd(i)));
    if (duped_fd == -1) {
      DLOG(ERROR) << "Failed duplicating dmabuf fd";
      return nullptr;
    }
    dmabuf_fds.emplace_back(duped_fd);
  }

  auto frame = VideoFrame::WrapExternalDmabufs(
      *layout, visible_rect, visible_rect.size(), std::move(dmabuf_fds),
      timestamp);
  if (!frame)
    return nullptr;

  // created |pixmap| must be owned by |frame|.
  frame->AddDestructionObserver(
      base::BindOnce(base::DoNothing::Once<scoped_refptr<gfx::NativePixmap>>(),
                     std::move(pixmap)));
  return frame;
}
#endif  // defined(USE_OZONE)

}  // namespace

scoped_refptr<VideoFrame> CreatePlatformVideoFrame(
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    gfx::BufferUsage buffer_usage,
    base::TimeDelta timestamp) {
#if defined(USE_OZONE)
  return CreateVideoFrameOzone(pixel_format, coded_size, visible_rect,
                               natural_size, buffer_usage, timestamp);
#endif  // defined(USE_OZONE)
  NOTREACHED();
  return nullptr;
}

gfx::GpuMemoryBufferHandle CreateGpuMemoryBufferHandle(
    const VideoFrame* video_frame) {
  DCHECK(video_frame);

  gfx::GpuMemoryBufferHandle handle;
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

  return handle;
}

}  // namespace media
