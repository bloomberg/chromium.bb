// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_picture_factory.h"

#include "media/gpu/vaapi_wrapper.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"

#if defined(USE_OZONE)
#include "media/gpu/vaapi/vaapi_drm_picture.h"
#elif defined(USE_X11)
#include "media/gpu/vaapi/vaapi_tfp_picture.h"
#endif

namespace media {

VaapiPictureFactory::VaapiPictureFactory() = default;

VaapiPictureFactory::~VaapiPictureFactory() = default;

std::unique_ptr<VaapiPicture> VaapiPictureFactory::Create(
    const scoped_refptr<VaapiWrapper>& vaapi_wrapper,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb,
    int32_t picture_buffer_id,
    const gfx::Size& size,
    uint32_t texture_id,
    uint32_t client_texture_id) {
#if defined(USE_OZONE)
  DCHECK_EQ(gl::kGLImplementationEGLGLES2, gl::GetGLImplementation());
  return std::make_unique<VaapiDrmPicture>(
      vaapi_wrapper, make_context_current_cb, bind_image_cb, picture_buffer_id,
      size, texture_id, client_texture_id);
#elif defined(USE_X11)
  DCHECK_EQ(gl::kGLImplementationDesktopGL, gl::GetGLImplementation());
  return std::make_unique<VaapiTFPPicture>(
      vaapi_wrapper, make_context_current_cb, bind_image_cb, picture_buffer_id,
      size, texture_id, client_texture_id);
#else
#error Unsupported platform
#endif
}

uint32_t VaapiPictureFactory::GetGLTextureTarget() {
#if defined(USE_OZONE)
  return GL_TEXTURE_EXTERNAL_OES;
#elif defined(USE_X11)
  return GL_TEXTURE_2D;
#else
#error Unsupported platform
#endif
}

}  // namespace media
