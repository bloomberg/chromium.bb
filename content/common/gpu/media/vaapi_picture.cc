// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/vaapi_picture.h"
#include "content/common/gpu/media/vaapi_wrapper.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"

#if defined(USE_X11)
#include "content/common/gpu/media/vaapi_tfp_picture.h"
#elif defined(USE_OZONE)
#include "content/common/gpu/media/vaapi_drm_picture.h"
#endif

namespace content {

// static
linked_ptr<VaapiPicture> VaapiPicture::CreatePicture(
    const scoped_refptr<VaapiWrapper>& vaapi_wrapper,
    const base::Callback<bool(void)> make_context_current,
    int32_t picture_buffer_id,
    uint32_t texture_id,
    const gfx::Size& size) {
  linked_ptr<VaapiPicture> picture;
#if defined(USE_X11)
  picture.reset(new VaapiTFPPicture(vaapi_wrapper, make_context_current,
                                    picture_buffer_id, texture_id, size));
#elif defined(USE_OZONE)
  picture.reset(new VaapiDrmPicture(vaapi_wrapper, make_context_current,
                                    picture_buffer_id, texture_id, size));
#endif  // USE_X11

  if (picture.get() && !picture->Initialize())
    picture.reset();

  return picture;
}

bool VaapiPicture::AllowOverlay() const {
  return false;
}

// static
uint32_t VaapiPicture::GetGLTextureTarget() {
#if defined(USE_OZONE)
  return GL_TEXTURE_EXTERNAL_OES;
#else
  return GL_TEXTURE_2D;
#endif
}

}  // namespace content
