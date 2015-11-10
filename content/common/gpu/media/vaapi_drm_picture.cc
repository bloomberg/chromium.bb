// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_descriptor_posix.h"
#include "content/common/gpu/media/va_surface.h"
#include "content/common/gpu/media/vaapi_drm_picture.h"
#include "content/common/gpu/media/vaapi_wrapper.h"
#include "third_party/libva/va/drm/va_drm.h"
#include "third_party/libva/va/va.h"
#include "third_party/libva/va/va_drmcommon.h"
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

uint32_t BufferFormatToVAFourCC(gfx::BufferFormat fmt) {
  switch (fmt) {
    case gfx::BufferFormat::BGRX_8888:
      return VA_FOURCC_BGRX;
    case gfx::BufferFormat::UYVY_422:
      return VA_FOURCC_UYVY;
    default:
      NOTREACHED();
      return 0;
  }
}

uint32_t BufferFormatToVARTFormat(gfx::BufferFormat fmt) {
  switch (fmt) {
    case gfx::BufferFormat::UYVY_422:
      return VA_RT_FORMAT_YUV422;
    case gfx::BufferFormat::BGRX_8888:
      return VA_RT_FORMAT_RGB32;
    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace

namespace content {

VaapiDrmPicture::VaapiDrmPicture(
    VaapiWrapper* vaapi_wrapper,
    const base::Callback<bool(void)>& make_context_current,
    int32 picture_buffer_id,
    uint32 texture_id,
    const gfx::Size& size)
    : VaapiPicture(picture_buffer_id, texture_id, size),
      vaapi_wrapper_(vaapi_wrapper),
      make_context_current_(make_context_current),
      weak_this_factory_(this) {
}

VaapiDrmPicture::~VaapiDrmPicture() {
  if (gl_image_ && make_context_current_.Run()) {
    gl_image_->ReleaseTexImage(GL_TEXTURE_EXTERNAL_OES);
    gl_image_->Destroy(true);

    DCHECK_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
  }
}

scoped_refptr<VASurface> VaapiDrmPicture::CreateVASurfaceForPixmap(
    scoped_refptr<ui::NativePixmap> pixmap,
    gfx::Size pixmap_size) {
  // Get the dmabuf of the created buffer.
  int dmabuf_fd = pixmap->GetDmaBufFd();
  if (dmabuf_fd < 0) {
    LOG(ERROR) << "Failed to get dmabuf from an Ozone NativePixmap";
    return nullptr;
  }
  int dmabuf_pitch = pixmap->GetDmaBufPitch();

  // Create a VASurface out of the created buffer using the dmabuf.
  VASurfaceAttribExternalBuffers va_attrib_extbuf;
  memset(&va_attrib_extbuf, 0, sizeof(va_attrib_extbuf));
  va_attrib_extbuf.pixel_format =
      BufferFormatToVAFourCC(pixmap->GetBufferFormat());
  va_attrib_extbuf.width = pixmap_size.width();
  va_attrib_extbuf.height = pixmap_size.height();
  va_attrib_extbuf.data_size = pixmap_size.height() * dmabuf_pitch;
  va_attrib_extbuf.num_planes = 1;
  va_attrib_extbuf.pitches[0] = dmabuf_pitch;
  va_attrib_extbuf.offsets[0] = 0;
  va_attrib_extbuf.buffers = reinterpret_cast<unsigned long*>(&dmabuf_fd);
  va_attrib_extbuf.num_buffers = 1;
  va_attrib_extbuf.flags = 0;
  va_attrib_extbuf.private_data = NULL;

  std::vector<VASurfaceAttrib> va_attribs;
  va_attribs.resize(2);

  va_attribs[0].type = VASurfaceAttribMemoryType;
  va_attribs[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
  va_attribs[0].value.type = VAGenericValueTypeInteger;
  va_attribs[0].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;

  va_attribs[1].type = VASurfaceAttribExternalBufferDescriptor;
  va_attribs[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
  va_attribs[1].value.type = VAGenericValueTypePointer;
  va_attribs[1].value.value.p = &va_attrib_extbuf;

  scoped_refptr<VASurface> va_surface = vaapi_wrapper_->CreateUnownedSurface(
      BufferFormatToVARTFormat(pixmap->GetBufferFormat()), pixmap_size,
      va_attribs);
  if (!va_surface) {
    LOG(ERROR) << "Failed to create VASurface for an Ozone NativePixmap";
    return nullptr;
  }

  return va_surface;
}

scoped_refptr<ui::NativePixmap> VaapiDrmPicture::CreateNativePixmap(
    gfx::Size size,
    gfx::BufferFormat format) {
  ui::OzonePlatform* platform = ui::OzonePlatform::GetInstance();
  ui::SurfaceFactoryOzone* factory = platform->GetSurfaceFactoryOzone();

  // Create a buffer from Ozone.
  return factory->CreateNativePixmap(gfx::kNullAcceleratedWidget, size, format,
                                     gfx::BufferUsage::SCANOUT);
}

bool VaapiDrmPicture::Initialize() {
  // We want to create a VASurface and an EGLImage out of the same
  // memory buffer, so we can output decoded pictures to it using
  // VAAPI and also use it to paint with GL.
  pixmap_ = CreateNativePixmap(size(), kPictureForGLImageFormat);
  if (!pixmap_) {
    LOG(ERROR) << "Failed creating an Ozone NativePixmap";
    return false;
  }

  va_surface_ = CreateVASurfaceForPixmap(pixmap_, size());
  if (!va_surface_) {
    LOG(ERROR) << "Failed creating VASurface for NativePixmap";
    return false;
  }

  // Weak pointers can only bind to methods without return values,
  // hence we cannot bind ProcessPixmap here. Instead we use a
  // static function to solve this problem.
  pixmap_->SetProcessingCallback(base::Bind(&VaapiDrmPicture::CallProcessPixmap,
                                            weak_this_factory_.GetWeakPtr()));

  if (!make_context_current_.Run())
    return false;

  gfx::ScopedTextureBinder texture_binder(GL_TEXTURE_EXTERNAL_OES,
                                          texture_id());
  scoped_refptr<gfx::GLImageOzoneNativePixmap> image(
      new gfx::GLImageOzoneNativePixmap(size(), GL_BGRA_EXT));
  if (!image->Initialize(pixmap_.get())) {
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

// static
scoped_refptr<ui::NativePixmap> VaapiDrmPicture::CallProcessPixmap(
    base::WeakPtr<VaapiDrmPicture> weak_ptr,
    gfx::Size target_size,
    gfx::BufferFormat target_format) {
  if (!weak_ptr.get()) {
    LOG(ERROR) << "Failed processing NativePixmap as processing "
                  "unit(VaapiDrmPicture) is deleted";
    return nullptr;
  }
  return weak_ptr->ProcessPixmap(target_size, target_format);
}

scoped_refptr<ui::NativePixmap> VaapiDrmPicture::ProcessPixmap(
    gfx::Size target_size,
    gfx::BufferFormat target_format) {
  if (!processed_va_surface_.get() ||
      processed_va_surface_->size() != target_size ||
      processed_va_surface_->format() !=
          BufferFormatToVARTFormat(target_format)) {
    processed_pixmap_ = CreateNativePixmap(target_size, target_format);
    if (!processed_pixmap_) {
      LOG(ERROR) << "Failed creating an Ozone NativePixmap for processing";
      processed_va_surface_ = nullptr;
      return nullptr;
    }
    processed_va_surface_ =
        CreateVASurfaceForPixmap(processed_pixmap_, target_size);
    if (!processed_va_surface_) {
      LOG(ERROR) << "Failed creating VA Surface for pixmap";
      processed_pixmap_ = nullptr;
      return nullptr;
    }
  }

  DCHECK(processed_pixmap_);
  bool vpp_result =
      vaapi_wrapper_->BlitSurface(va_surface_, processed_va_surface_);
  if (!vpp_result) {
    LOG(ERROR) << "Failed scaling NativePixmap";
    processed_pixmap_ = nullptr;
    processed_va_surface_ = nullptr;
    return nullptr;
  }

  return processed_pixmap_;
}

scoped_refptr<gl::GLImage> VaapiDrmPicture::GetImageToBind() {
  return gl_image_;
}

bool VaapiDrmPicture::AllowOverlay() const {
  return true;
}

}  // namespace
