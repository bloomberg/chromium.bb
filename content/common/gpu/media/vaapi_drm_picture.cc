// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_descriptor_posix.h"
#include "content/common/gpu/media/va_surface.h"
#include "content/common/gpu/media/vaapi_drm_picture.h"
#include "content/common/gpu/media/vaapi_wrapper.h"
#include "third_party/libva/va/drm/va_drm.h"
#include "third_party/libva/va/va.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image_ozone_native_pixmap.h"
#include "ui/gl/scoped_binders.h"
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace {
// We decode video into YUV420, but for usage with GLImages we have to convert
// to BGRX_8888.
const gfx::BufferFormat kPictureForGLImageFormat = gfx::BufferFormat::BGRX_8888;

}  // namespace

namespace content {

VaapiDrmPicture::VaapiDrmPicture(
    const scoped_refptr<VaapiWrapper>& vaapi_wrapper,
    const base::Callback<bool(void)>& make_context_current,
    int32_t picture_buffer_id,
    uint32_t texture_id,
    const gfx::Size& size)
    : VaapiPicture(picture_buffer_id, texture_id, size),
      vaapi_wrapper_(vaapi_wrapper),
      make_context_current_(make_context_current) {}

VaapiDrmPicture::~VaapiDrmPicture() {
  if (gl_image_ && make_context_current_.Run()) {
    gl_image_->ReleaseTexImage(GL_TEXTURE_EXTERNAL_OES);
    gl_image_->Destroy(true);

    DCHECK_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
  }
}

bool VaapiDrmPicture::Initialize() {
  // We want to create a VASurface and an EGLImage out of the same
  // memory buffer, so we can output decoded pictures to it using
  // VAAPI and also use it to paint with GL.
  ui::OzonePlatform* platform = ui::OzonePlatform::GetInstance();
  ui::SurfaceFactoryOzone* factory = platform->GetSurfaceFactoryOzone();
  pixmap_ = factory->CreateNativePixmap(gfx::kNullAcceleratedWidget, size(),
                                        kPictureForGLImageFormat,
                                        gfx::BufferUsage::SCANOUT);
  if (!pixmap_) {
    LOG(ERROR) << "Failed creating an Ozone NativePixmap";
    return false;
  }

  va_surface_ = vaapi_wrapper_->CreateVASurfaceForPixmap(pixmap_);
  if (!va_surface_) {
    LOG(ERROR) << "Failed creating VASurface for NativePixmap";
    return false;
  }

  pixmap_->SetProcessingCallback(
      base::Bind(&VaapiWrapper::ProcessPixmap, vaapi_wrapper_));

  if (!make_context_current_.Run())
    return false;

  gfx::ScopedTextureBinder texture_binder(GL_TEXTURE_EXTERNAL_OES,
                                          texture_id());
  scoped_refptr<gfx::GLImageOzoneNativePixmap> image(
      new gfx::GLImageOzoneNativePixmap(size(), GL_BGRA_EXT));
  if (!image->Initialize(pixmap_.get(), pixmap_->GetBufferFormat())) {
    LOG(ERROR) << "Failed to create GLImage";
    return false;
  }
  gl_image_ = image;
  if (!gl_image_->BindTexImage(GL_TEXTURE_EXTERNAL_OES)) {
    LOG(ERROR) << "Failed to bind texture to GLImage";
    return false;
  }

  return true;
}

bool VaapiDrmPicture::DownloadFromSurface(
    const scoped_refptr<VASurface>& va_surface) {
  return vaapi_wrapper_->BlitSurface(va_surface, va_surface_);
}

scoped_refptr<gl::GLImage> VaapiDrmPicture::GetImageToBind() {
  return gl_image_;
}

bool VaapiDrmPicture::AllowOverlay() const {
  return true;
}

}  // namespace
