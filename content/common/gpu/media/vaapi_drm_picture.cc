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
#include "ui/gl/gl_image_linux_dma_buffer.h"
#include "ui/gl/scoped_binders.h"
#include "ui/ozone/gpu/gpu_memory_buffer_factory_ozone_native_buffer.h"
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace content {

VaapiDrmPicture::VaapiDrmPicture(
    VaapiWrapper* vaapi_wrapper,
    const base::Callback<bool(void)>& make_context_current,
    int32 picture_buffer_id,
    uint32 texture_id,
    const gfx::Size& size)
    : VaapiPicture(picture_buffer_id, texture_id, size),
      vaapi_wrapper_(vaapi_wrapper),
      make_context_current_(make_context_current) {
}

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

  // Create a buffer from Ozone.
  pixmap_ = factory->CreateNativePixmap(gfx::kNullAcceleratedWidget, size(),
                                        ui::SurfaceFactoryOzone::RGBA_8888,
                                        ui::SurfaceFactoryOzone::SCANOUT);
  if (!pixmap_) {
    LOG(ERROR) << "Failed creating an Ozone NativePixmap";
    return false;
  }

  // Get the dmabuf of the created buffer.
  int dmabuf_fd = pixmap_->GetDmaBufFd();
  if (dmabuf_fd < 0) {
    LOG(ERROR) << "Failed to get dmabuf from an Ozone NativePixmap";
    return false;
  }
  int dmabuf_pitch = pixmap_->GetDmaBufPitch();

  // Create a VASurface out of the created buffer using the dmabuf.
  VASurfaceAttribExternalBuffers va_attrib_extbuf;
  memset(&va_attrib_extbuf, 0, sizeof(va_attrib_extbuf));
  va_attrib_extbuf.pixel_format = VA_FOURCC_BGRX;
  va_attrib_extbuf.width = size().width();
  va_attrib_extbuf.height = size().height();
  va_attrib_extbuf.data_size = size().height() * dmabuf_pitch;
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

  va_surface_ = vaapi_wrapper_->CreateUnownedSurface(VA_RT_FORMAT_RGB32, size(),
                                                     va_attribs);
  if (!va_surface_) {
    LOG(ERROR) << "Failed to create VASurface for an Ozone NativePixmap";
    return false;
  }

  if (!make_context_current_.Run())
    return false;

  gfx::ScopedTextureBinder texture_binder(GL_TEXTURE_EXTERNAL_OES,
                                          texture_id());
  gl_image_ = ui::GpuMemoryBufferFactoryOzoneNativeBuffer::CreateImageForPixmap(
      pixmap_, size(), gfx::GpuMemoryBuffer::BGRA_8888, GL_RGBA);
  if (!gl_image_) {
    LOG(ERROR) << "Failed to create GLImage";
    return false;
  }
  if (!gl_image_->BindTexImage(GL_TEXTURE_EXTERNAL_OES)) {
    LOG(ERROR) << "Failed to bind texture to GLImage";
    return false;
  }

  return true;
}

bool VaapiDrmPicture::DownloadFromSurface(
    const scoped_refptr<VASurface>& va_surface) {
  return vaapi_wrapper_->BlitSurface(va_surface->id(), va_surface->size(),
                                     va_surface_->id(), va_surface_->size());
}

scoped_refptr<gfx::GLImage> VaapiDrmPicture::GetImageToBind() {
  return gl_image_;
}

bool VaapiDrmPicture::AllowOverlay() const {
  return true;
}

}  // namespace
