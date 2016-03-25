// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of picture allocation for the
// X11 window system used by VaapiVideoDecodeAccelerator to produce
// output pictures.

#ifndef CONTENT_COMMON_GPU_MEDIA_VAAPI_TFP_PICTURE_H_
#define CONTENT_COMMON_GPU_MEDIA_VAAPI_TFP_PICTURE_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/gpu/media/vaapi_picture.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"

namespace gfx {
class GLContextGLX;
}

namespace gl {
class GLImageGLX;
}

namespace content {

class VaapiWrapper;

// Implementation of VaapiPicture for the X11 backed chromium.
class VaapiTFPPicture : public VaapiPicture {
 public:
  VaapiTFPPicture(const scoped_refptr<VaapiWrapper>& vaapi_wrapper,
                  const base::Callback<bool(void)> make_context_current,
                  int32_t picture_buffer_id,
                  uint32_t texture_id,
                  const gfx::Size& size);

  ~VaapiTFPPicture() override;

  bool Initialize() override;

  bool DownloadFromSurface(const scoped_refptr<VASurface>& va_surface) override;

  scoped_refptr<gl::GLImage> GetImageToBind() override;

 private:
  scoped_refptr<VaapiWrapper> vaapi_wrapper_;

  base::Callback<bool(void)> make_context_current_;
  Display* x_display_;

  Pixmap x_pixmap_;
  scoped_refptr<gl::GLImageGLX> glx_image_;

  DISALLOW_COPY_AND_ASSIGN(VaapiTFPPicture);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_VAAPI_TFP_PICTURE_H_
