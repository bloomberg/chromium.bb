// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_picture.h"

#include "media/gpu/vaapi/vaapi_drm_picture.h"
#include "media/gpu/vaapi_wrapper.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"

#if defined(USE_X11)
#include "media/gpu/vaapi/vaapi_tfp_picture.h"
#endif

namespace media {

VaapiPicture::VaapiPicture(
    const scoped_refptr<VaapiWrapper>& vaapi_wrapper,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb,
    int32_t picture_buffer_id,
    const gfx::Size& size,
    uint32_t texture_id,
    uint32_t client_texture_id)
    : vaapi_wrapper_(vaapi_wrapper),
      make_context_current_cb_(make_context_current_cb),
      bind_image_cb_(bind_image_cb),
      size_(size),
      texture_id_(texture_id),
      client_texture_id_(client_texture_id),
      picture_buffer_id_(picture_buffer_id) {}

VaapiPicture::~VaapiPicture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool VaapiPicture::AllowOverlay() const {
  return false;
}

// static
std::unique_ptr<VaapiPicture> VaapiPicture::CreatePicture(
    const scoped_refptr<VaapiWrapper>& vaapi_wrapper,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb,
    int32_t picture_buffer_id,
    const gfx::Size& size,
    uint32_t texture_id,
    uint32_t client_texture_id) {
  std::unique_ptr<VaapiPicture> picture;

  switch (gl::GetGLImplementation()) {
    case gl::kGLImplementationEGLGLES2:
      picture.reset(new VaapiDrmPicture(vaapi_wrapper, make_context_current_cb,
                                        bind_image_cb, picture_buffer_id, size,
                                        texture_id, client_texture_id));
      break;
    case gl::kGLImplementationDesktopGL:
#if defined(USE_X11)
      picture.reset(new VaapiTFPPicture(vaapi_wrapper, make_context_current_cb,
                                        bind_image_cb, picture_buffer_id, size,
                                        texture_id, client_texture_id));
      break;
#endif  // USE_X11
    default:
      NOTIMPLEMENTED();
  }

  return picture;
}

// static
uint32_t VaapiPicture::GetGLTextureTarget() {
#if defined(USE_OZONE)
  return GL_TEXTURE_EXTERNAL_OES;
#else
  return GL_TEXTURE_2D;
#endif
}

}  // namespace media
