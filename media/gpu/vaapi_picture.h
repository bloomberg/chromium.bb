// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an interface of output pictures for the Vaapi
// video decoder. This is implemented by different window system
// (X11/Ozone) and used by VaapiVideoDecodeAccelerator to produce
// output pictures.

#ifndef MEDIA_GPU_VAAPI_PICTURE_H_
#define MEDIA_GPU_VAAPI_PICTURE_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "ui/gfx/geometry/size.h"

namespace gl {
class GLImage;
}

namespace media {

class VASurface;
class VaapiWrapper;

// Picture is native pixmap abstraction (X11/Ozone).
class VaapiPicture : public base::NonThreadSafe {
 public:
  virtual ~VaapiPicture() {}

  // Try to allocate the underlying resources for the picture.
  virtual bool Initialize() = 0;

  int32_t picture_buffer_id() const { return picture_buffer_id_; }
  uint32_t texture_id() const { return texture_id_; }
  const gfx::Size& size() const { return size_; }

  virtual bool AllowOverlay() const;

  // Returns the |GLImage|, if any, to bind to the texture.
  virtual scoped_refptr<gl::GLImage> GetImageToBind() = 0;

  // Downloads the |va_surface| into the picture, potentially scaling
  // it if needed.
  virtual bool DownloadFromSurface(
      const scoped_refptr<VASurface>& va_surface) = 0;

  // Create a VaapiPicture of |size| to be associated with
  // |picture_buffer_id| and bound to |texture_id|.
  // |make_context_current_cb| is provided for the GL operations.
  static linked_ptr<VaapiPicture> CreatePicture(
      const scoped_refptr<VaapiWrapper>& vaapi_wrapper,
      const MakeGLContextCurrentCallback& make_context_current_cb,
      int32_t picture_buffer_id,
      uint32_t texture_id,
      const gfx::Size& size);

  // Get the texture target used to bind EGLImages (either
  // GL_TEXTURE_2D on X11 or GL_TEXTURE_EXTERNAL_OES on DRM).
  static uint32_t GetGLTextureTarget();

 protected:
  VaapiPicture(int32_t picture_buffer_id,
               uint32_t texture_id,
               const gfx::Size& size)
      : picture_buffer_id_(picture_buffer_id),
        texture_id_(texture_id),
        size_(size) {}

 private:
  int32_t picture_buffer_id_;
  uint32_t texture_id_;
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(VaapiPicture);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_PICTURE_H_
