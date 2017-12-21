// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_drm_picture.h"

#include "base/file_descriptor_posix.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
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
    uint32_t client_texture_id,
    uint32_t texture_target)
    : VaapiPicture(vaapi_wrapper,
                   make_context_current_cb,
                   bind_image_cb,
                   picture_buffer_id,
                   size,
                   texture_id,
                   client_texture_id,
                   texture_target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

VaapiDrmPicture::~VaapiDrmPicture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (gl_image_ && make_context_current_cb_.Run()) {
    gl_image_->ReleaseTexImage(texture_target_);
    DCHECK_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
  }
}

bool VaapiDrmPicture::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pixmap_);

  // Create a |va_surface_| from dmabuf fds (pixmap->GetDmaBufFd)
  va_surface_ = vaapi_wrapper_->CreateVASurfaceForPixmap(pixmap_);
  if (!va_surface_) {
    LOG(ERROR) << "Failed creating VASurface for NativePixmap";
    return false;
  }

#if defined(USE_OZONE)
  // Import dmabuf fds into the output gl texture through EGLImage.
  if (texture_id_ != 0 && !make_context_current_cb_.is_null()) {
    if (!make_context_current_cb_.Run())
      return false;

    gl::ScopedTextureBinder texture_binder(texture_target_, texture_id_);

    gfx::BufferFormat format = pixmap_->GetBufferFormat();

    scoped_refptr<gl::GLImageNativePixmap> image(new gl::GLImageNativePixmap(
        size_, BufferFormatToInternalFormat(format)));
    if (!image->Initialize(pixmap_.get(), format)) {
      LOG(ERROR) << "Failed to create GLImage";
      return false;
    }
    gl_image_ = image;
    if (!gl_image_->BindTexImage(texture_target_)) {
      LOG(ERROR) << "Failed to bind texture to GLImage";
      return false;
    }
  }
#else
  // On non-ozone, no need to import dmabuf fds into output the gl texture
  // because the dmabuf fds have been made from it.
  DCHECK(pixmap_->AreDmaBufFdsValid());
#endif

  if (client_texture_id_ != 0 && !bind_image_cb_.is_null()) {
    if (!bind_image_cb_.Run(client_texture_id_, texture_target_, gl_image_,
                            true)) {
      LOG(ERROR) << "Failed to bind client_texture_id";
      return false;
    }
  }

  return true;
}

bool VaapiDrmPicture::Allocate(gfx::BufferFormat format) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

// The goal of this method (ozone and non-ozone) is to allocate the
// |pixmap_| which is a gl::GLImageNativePixmap.
#if defined(USE_OZONE)
  ui::OzonePlatform* platform = ui::OzonePlatform::GetInstance();
  ui::SurfaceFactoryOzone* factory = platform->GetSurfaceFactoryOzone();
  pixmap_ = factory->CreateNativePixmap(gfx::kNullAcceleratedWidget, size_,
                                        format, gfx::BufferUsage::SCANOUT);
#else
  // Export the gl texture as dmabuf.
  if (texture_id_ != 0 && !make_context_current_cb_.is_null()) {
    if (!make_context_current_cb_.Run())
      return false;

    scoped_refptr<gl::GLImageNativePixmap> image(new gl::GLImageNativePixmap(
        size_, BufferFormatToInternalFormat(format)));

    // Create an EGLImage from a gl texture
    if (!image->InitializeFromTexture(texture_id_)) {
      DLOG(ERROR) << "Failed to initialize eglimage from texture id: "
                  << texture_id_;
      return false;
    }

    // Export the EGLImage as dmabuf.
    gfx::NativePixmapHandle native_pixmap_handle = image->ExportHandle();
    if (!native_pixmap_handle.planes.size()) {
      DLOG(ERROR) << "Failed to export EGLImage as dmabuf fds";
      return false;
    }

    // Convert NativePixmapHandle to NativePixmapDmaBuf.
    scoped_refptr<gfx::NativePixmap> native_pixmap_dmabuf(
        new gfx::NativePixmapDmaBuf(size_, format, native_pixmap_handle));
    if (!native_pixmap_dmabuf->AreDmaBufFdsValid()) {
      DLOG(ERROR) << "Invalid dmabuf fds";
      return false;
    }

    if (!image->BindTexImage(texture_target_)) {
      DLOG(ERROR) << "Failed to bind texture to GLImage";
      return false;
    }

    // The |pixmap_| takes ownership of the dmabuf fds. So the only reason to
    // to keep a reference on the image is because the GPU service needs to
    // track this image as it will be attached to a client texture.
    pixmap_ = native_pixmap_dmabuf;
    gl_image_ = image;
  }
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
