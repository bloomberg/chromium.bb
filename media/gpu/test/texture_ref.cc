// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/texture_ref.h"

#include <utility>
#include <vector>

#if defined(OS_CHROMEOS)
#include <libdrm/drm_fourcc.h>

#include "base/logging.h"
#include "media/gpu/format_utils.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif

namespace media {
namespace test {

TextureRef::TextureRef(uint32_t texture_id,
                       base::OnceClosure no_longer_needed_cb)
    : texture_id_(texture_id),
      no_longer_needed_cb_(std::move(no_longer_needed_cb)) {}

TextureRef::~TextureRef() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::move(no_longer_needed_cb_).Run();
}

// static
scoped_refptr<TextureRef> TextureRef::Create(
    uint32_t texture_id,
    base::OnceClosure no_longer_needed_cb) {
  return base::WrapRefCounted(
      new TextureRef(texture_id, std::move(no_longer_needed_cb)));
}

// static
scoped_refptr<TextureRef> TextureRef::CreatePreallocated(
    uint32_t texture_id,
    base::OnceClosure no_longer_needed_cb,
    VideoPixelFormat pixel_format,
    const gfx::Size& size,
    gfx::BufferUsage buffer_usage) {
  scoped_refptr<TextureRef> texture_ref;
#if defined(OS_CHROMEOS)
  texture_ref = TextureRef::Create(texture_id, std::move(no_longer_needed_cb));
  LOG_ASSERT(texture_ref);

  ui::OzonePlatform* platform = ui::OzonePlatform::GetInstance();
  LOG_ASSERT(platform);
  ui::SurfaceFactoryOzone* factory = platform->GetSurfaceFactoryOzone();
  LOG_ASSERT(factory);
  gfx::BufferFormat buffer_format =
      VideoPixelFormatToGfxBufferFormat(pixel_format);
  texture_ref->pixmap_ = factory->CreateNativePixmap(
      gfx::kNullAcceleratedWidget, size, buffer_format, buffer_usage);
  LOG_ASSERT(texture_ref->pixmap_);
  texture_ref->coded_size_ = size;
#endif

  return texture_ref;
}

gfx::GpuMemoryBufferHandle TextureRef::ExportGpuMemoryBufferHandle() const {
  gfx::GpuMemoryBufferHandle handle;
#if defined(OS_CHROMEOS)
  CHECK(pixmap_);
  handle.type = gfx::NATIVE_PIXMAP;

  size_t num_planes =
      gfx::NumberOfPlanesForBufferFormat(pixmap_->GetBufferFormat());
  for (size_t i = 0; i < num_planes; ++i) {
    handle.native_pixmap_handle.planes.emplace_back(
        pixmap_->GetDmaBufPitch(i), pixmap_->GetDmaBufOffset(i), i,
        pixmap_->GetDmaBufModifier(i));
  }

  size_t num_fds = pixmap_->GetDmaBufFdCount();
  LOG_ASSERT(num_fds == num_planes || num_fds == 1);
  for (size_t i = 0; i < num_fds; ++i) {
    int duped_fd = HANDLE_EINTR(dup(pixmap_->GetDmaBufFd(i)));
    LOG_ASSERT(duped_fd != -1) << "Failed duplicating dmabuf fd";
    handle.native_pixmap_handle.fds.emplace_back(
        base::FileDescriptor(duped_fd, true));
  }
#endif
  return handle;
}

scoped_refptr<VideoFrame> TextureRef::CreateVideoFrame(
    const gfx::Rect& visible_rect) const {
  scoped_refptr<VideoFrame> video_frame;
#if defined(OS_CHROMEOS)
  VideoPixelFormat pixel_format =
      GfxBufferFormatToVideoPixelFormat(pixmap_->GetBufferFormat());
  CHECK_NE(pixel_format, PIXEL_FORMAT_UNKNOWN);
  size_t num_planes = VideoFrame::NumPlanes(pixel_format);
  std::vector<VideoFrameLayout::Plane> planes(num_planes);
  std::vector<int> plane_height(num_planes, 0u);
  for (size_t i = 0; i < num_planes; ++i) {
    planes[i].stride = pixmap_->GetDmaBufPitch(i);
    planes[i].offset = pixmap_->GetDmaBufOffset(i);
    plane_height[i] = VideoFrame::Rows(i, pixel_format, coded_size_.height());
  }

  std::vector<base::ScopedFD> dmabuf_fds;
  size_t num_fds = pixmap_->GetDmaBufFdCount();
  LOG_ASSERT(num_fds <= num_planes);
  for (size_t i = 0; i < num_fds; ++i) {
    int duped_fd = HANDLE_EINTR(dup(pixmap_->GetDmaBufFd(i)));
    LOG_ASSERT(duped_fd != -1) << "Failed duplicating dmabuf fd";
    dmabuf_fds.emplace_back(duped_fd);
  }

  std::vector<size_t> buffer_sizes(num_fds, 0u);
  for (size_t plane = 0, i = 0; plane < num_planes; ++plane) {
    if (plane + 1 < buffer_sizes.size()) {
      buffer_sizes[i] =
          planes[plane].offset + planes[plane].stride * plane_height[plane];
      ++i;
    } else {
      buffer_sizes[i] = std::max(
          buffer_sizes[i],
          planes[plane].offset + planes[plane].stride * plane_height[plane]);
    }
  }
  auto layout = VideoFrameLayout::CreateWithPlanes(
      pixel_format, coded_size_, std::move(planes), std::move(buffer_sizes));
  LOG_ASSERT(layout.has_value() == true);
  video_frame = VideoFrame::WrapExternalDmabufs(
      *layout, visible_rect, visible_rect.size(), std::move(dmabuf_fds),
      base::TimeDelta());
#endif
  return video_frame;
}

bool TextureRef::IsDirectlyMappable() const {
#if defined(OS_CHROMEOS)
  return pixmap_->GetDmaBufModifier(0) == DRM_FORMAT_MOD_LINEAR;
#else
  return false;
#endif
}

}  // namespace test
}  // namespace media
