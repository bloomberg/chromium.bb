// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_picture_native_pixmap.h"

#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image_native_pixmap.h"

namespace media {

VaapiPictureNativePixmap::VaapiPictureNativePixmap(
    const scoped_refptr<VaapiWrapper>& vaapi_wrapper,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb,
    int32_t picture_buffer_id,
    const gfx::Size& size,
    uint32_t texture_id,
    uint32_t client_texture_id,
    uint32_t texture_target)
    : VaapiPicture(vaapi_wrapper,
                   make_context_current_cb,
                   bind_image_cb,
                   picture_buffer_id,
                   size,
                   texture_id,
                   client_texture_id,
                   texture_target) {}

VaapiPictureNativePixmap::~VaapiPictureNativePixmap() = default;

bool VaapiPictureNativePixmap::DownloadFromSurface(
    const scoped_refptr<VASurface>& va_surface) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return vaapi_wrapper_->BlitSurface(va_surface, va_surface_);
}

bool VaapiPictureNativePixmap::AllowOverlay() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

VASurfaceID VaapiPictureNativePixmap::va_surface_id() const {
  return va_surface_->id();
}

// static
gfx::GpuMemoryBufferHandle
VaapiPictureNativePixmap::CreateGpuMemoryBufferHandleFromVideoFrame(
    const VideoFrame* const video_frame) {
  DCHECK(video_frame->HasDmaBufs());

  const auto& planes = video_frame->layout().planes();
  const auto& fds = video_frame->DmabufFds();
  DCHECK_EQ(fds.size(), planes.size());

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::NATIVE_PIXMAP;
  for (size_t i = 0; i < planes.size(); ++i) {
    int dup_fd = HANDLE_EINTR(dup(fds[i].get()));
    if (dup_fd == -1) {
      PLOG(ERROR) << "Failed duplicating dmabuf fd";
      return gfx::GpuMemoryBufferHandle();
    }

    handle.native_pixmap_handle.planes.emplace_back(
        planes[i].stride, planes[i].offset, 0, base::ScopedFD(dup_fd));
  }
  return handle;
}

}  // namespace media
