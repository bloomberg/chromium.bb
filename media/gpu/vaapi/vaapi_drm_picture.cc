// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_drm_picture.h"

#include "base/file_descriptor_posix.h"
#include "media/gpu/va_surface.h"
#include "media/gpu/vaapi_wrapper.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image_native_pixmap.h"
#include "ui/gl/scoped_binders.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif

namespace media {

namespace {

static unsigned BufferFormatToInternalFormat(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::RGBX_8888:
      return GL_RGB;

    case gfx::BufferFormat::BGRA_8888:
      return GL_BGRA_EXT;

    case gfx::BufferFormat::YVU_420:
      return GL_RGB_YCRCB_420_CHROMIUM;

    default:
      NOTREACHED();
      return GL_BGRA_EXT;
  }
}

}  // anonymous namespace

VaapiDrmPicture::VaapiDrmPicture(
    const scoped_refptr<VaapiWrapper>& vaapi_wrapper,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb,
    int32_t picture_buffer_id,
    const gfx::Size& size,
    uint32_t texture_id,
    uint32_t client_texture_id)
    : VaapiPicture(vaapi_wrapper,
                   make_context_current_cb,
                   bind_image_cb,
                   picture_buffer_id,
                   size,
                   texture_id,
                   client_texture_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

VaapiDrmPicture::~VaapiDrmPicture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (gl_image_ && make_context_current_cb_.Run()) {
    gl_image_->ReleaseTexImage(GL_TEXTURE_EXTERNAL_OES);
    DCHECK_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
  }
}

bool VaapiDrmPicture::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pixmap_);

  va_surface_ = vaapi_wrapper_->CreateVASurfaceForPixmap(pixmap_);
  if (!va_surface_) {
    LOG(ERROR) << "Failed creating VASurface for NativePixmap";
    return false;
  }

  if (texture_id_ != 0 && !make_context_current_cb_.is_null()) {
    if (!make_context_current_cb_.Run())
      return false;

    gl::ScopedTextureBinder texture_binder(GL_TEXTURE_EXTERNAL_OES,
                                           texture_id_);

    gfx::BufferFormat format = pixmap_->GetBufferFormat();

    scoped_refptr<gl::GLImageNativePixmap> image(new gl::GLImageNativePixmap(
        size_, BufferFormatToInternalFormat(format)));
    if (!image->Initialize(pixmap_.get(), format)) {
      LOG(ERROR) << "Failed to create GLImage";
      return false;
    }
    gl_image_ = image;
    if (!gl_image_->BindTexImage(GL_TEXTURE_EXTERNAL_OES)) {
      LOG(ERROR) << "Failed to bind texture to GLImage";
      return false;
    }
  }

  if (client_texture_id_ != 0 && !bind_image_cb_.is_null()) {
    if (!bind_image_cb_.Run(client_texture_id_, GL_TEXTURE_EXTERNAL_OES,
                            gl_image_, true)) {
      LOG(ERROR) << "Failed to bind client_texture_id";
      return false;
    }
  }

  return true;
}

bool VaapiDrmPicture::Allocate(gfx::BufferFormat format) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if defined(USE_OZONE)
  ui::OzonePlatform* platform = ui::OzonePlatform::GetInstance();
  ui::SurfaceFactoryOzone* factory = platform->GetSurfaceFactoryOzone();
  pixmap_ = factory->CreateNativePixmap(gfx::kNullAcceleratedWidget, size_,
                                        format, gfx::BufferUsage::SCANOUT);
#else
  // TODO(jisorce): Implement non-ozone case, see crbug.com/785201.
  NOTIMPLEMENTED();
#endif  // USE_OZONE

  if (!pixmap_) {
    DVLOG(1) << "Failed allocating a pixmap";
    return false;
  }

  return Initialize();
}

bool VaapiDrmPicture::ImportGpuMemoryBufferHandle(
    gfx::BufferFormat format,
    const gfx::GpuMemoryBufferHandle& gpu_memory_buffer_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if defined(USE_OZONE)
  ui::OzonePlatform* platform = ui::OzonePlatform::GetInstance();
  ui::SurfaceFactoryOzone* factory = platform->GetSurfaceFactoryOzone();
  // CreateNativePixmapFromHandle() will take ownership of the handle.
  pixmap_ = factory->CreateNativePixmapFromHandle(
      gfx::kNullAcceleratedWidget, size_, format,
      gpu_memory_buffer_handle.native_pixmap_handle);
#else
  NOTIMPLEMENTED();
#endif
  if (!pixmap_) {
    DVLOG(1) << "Failed creating a pixmap from a native handle";
    return false;
  }

  return Initialize();
}

bool VaapiDrmPicture::DownloadFromSurface(
    const scoped_refptr<VASurface>& va_surface) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return vaapi_wrapper_->BlitSurface(va_surface, va_surface_);
}

bool VaapiDrmPicture::AllowOverlay() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

}  // namespace media
