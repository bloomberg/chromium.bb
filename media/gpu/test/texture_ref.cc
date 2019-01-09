// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/texture_ref.h"

#include <utility>
#include <vector>

#include "media/gpu/platform_video_frame.h"

#if defined(OS_CHROMEOS)
#include <libdrm/drm_fourcc.h>

#include "base/logging.h"
#include "media/base/scopedfd_helper.h"
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

  // We pass coded_size as visible_rect here. The actual visible rect is given
  // in ExportVideoFrame().
  gfx::Rect visible_rect(size.width(), size.height());
  texture_ref->frame_ = media::CreatePlatformVideoFrame(
      pixel_format, size, visible_rect, visible_rect.size(), buffer_usage,
      base::TimeDelta());
  if (!texture_ref->frame_) {
    LOG(ERROR) << "Failed to create Dmabuf-backed VideoFrame";
    return nullptr;
  }
#endif
  return texture_ref;
}

gfx::GpuMemoryBufferHandle TextureRef::ExportGpuMemoryBufferHandle() const {
  gfx::GpuMemoryBufferHandle handle;
#if defined(OS_CHROMEOS)
  LOG_ASSERT(frame_);
  handle.type = gfx::NATIVE_PIXMAP;

  const size_t num_planes = VideoFrame::NumPlanes(frame_->format());
  for (size_t i = 0; i < num_planes; ++i) {
    const auto& plane = frame_->layout().planes()[i];
    handle.native_pixmap_handle.planes.emplace_back(plane.stride, plane.offset,
                                                    i, plane.modifier);
  }

  std::vector<base::ScopedFD> duped_fds = DuplicateFDs(frame_->DmabufFds());
  for (auto& duped_fd : duped_fds) {
    handle.native_pixmap_handle.fds.emplace_back(std::move(duped_fd));
  }
#endif
  return handle;
}

scoped_refptr<VideoFrame> TextureRef::ExportVideoFrame(
    gfx::Rect visible_rect) const {
#if defined(OS_CHROMEOS)
  return VideoFrame::WrapVideoFrame(frame_, frame_->format(), visible_rect,
                                    visible_rect.size());
#else
  return nullptr;
#endif
}

bool TextureRef::IsDirectlyMappable() const {
#if defined(OS_CHROMEOS)
  return frame_->layout().planes()[0].modifier == DRM_FORMAT_MOD_LINEAR;
#else
  return false;
#endif
}

}  // namespace test
}  // namespace media
