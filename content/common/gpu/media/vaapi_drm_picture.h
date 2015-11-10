// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of picture allocation for the
// Ozone window system used by VaapiVideoDecodeAccelerator to produce
// output pictures.

#ifndef CONTENT_COMMON_GPU_MEDIA_VAAPI_DRM_PICTURE_H_
#define CONTENT_COMMON_GPU_MEDIA_VAAPI_DRM_PICTURE_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/gpu/media/vaapi_picture.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"

namespace gl {
class GLImage;
}

namespace ui {
class NativePixmap;
}

namespace content {

class VaapiWrapper;

// Implementation of VaapiPicture for the ozone/drm backed chromium.
class VaapiDrmPicture : public VaapiPicture {
 public:
  VaapiDrmPicture(VaapiWrapper* vaapi_wrapper,
                  const base::Callback<bool(void)>& make_context_current,
                  int32 picture_buffer_id,
                  uint32 texture_id,
                  const gfx::Size& size);

  ~VaapiDrmPicture() override;

  bool Initialize() override;

  bool DownloadFromSurface(const scoped_refptr<VASurface>& va_surface) override;

  scoped_refptr<gl::GLImage> GetImageToBind() override;

  bool AllowOverlay() const override;

 private:
  // Calls ProcessPixmap() if weak_ptr is not NULL.
  static scoped_refptr<ui::NativePixmap> CallProcessPixmap(
      base::WeakPtr<VaapiDrmPicture> weak_ptr,
      gfx::Size target_size,
      gfx::BufferFormat target_format);
  // Use VPP to process underlying pixmap_, scaling to |target_size| and
  // converting to |target_format|.
  scoped_refptr<ui::NativePixmap> ProcessPixmap(
      gfx::Size target_size,
      gfx::BufferFormat target_format);
  scoped_refptr<VASurface> CreateVASurfaceForPixmap(
      scoped_refptr<ui::NativePixmap> pixmap,
      gfx::Size pixmap_size);
  scoped_refptr<ui::NativePixmap> CreateNativePixmap(gfx::Size size,
                                                     gfx::BufferFormat format);

  VaapiWrapper* vaapi_wrapper_;  // Not owned.
  base::Callback<bool(void)> make_context_current_;

  // Ozone buffer, the storage of the EGLImage and the VASurface.
  scoped_refptr<ui::NativePixmap> pixmap_;

  // Ozone buffer, the storage of the processed buffer for overlay.
  scoped_refptr<ui::NativePixmap> processed_pixmap_;

  // EGLImage bound to the GL textures used by the VDA client.
  scoped_refptr<gl::GLImage> gl_image_;

  // VASurface used to transfer from the decoder's pixel format.
  scoped_refptr<VASurface> va_surface_;

  // VaSurface used to apply processing.
  scoped_refptr<VASurface> processed_va_surface_;

  // The WeakPtrFactory for VaapiDrmPicture.
  base::WeakPtrFactory<VaapiDrmPicture> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(VaapiDrmPicture);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_VAAPI_DRM_PICTURE_H_
