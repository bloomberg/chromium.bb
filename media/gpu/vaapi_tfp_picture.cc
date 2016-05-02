// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/va_surface.h"
#include "media/gpu/vaapi_tfp_picture.h"
#include "media/gpu/vaapi_wrapper.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image_glx.h"
#include "ui/gl/scoped_binders.h"

namespace media {

VaapiTFPPicture::VaapiTFPPicture(
    const scoped_refptr<VaapiWrapper>& vaapi_wrapper,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    int32_t picture_buffer_id,
    uint32_t texture_id,
    const gfx::Size& size)
    : VaapiPicture(picture_buffer_id, texture_id, size),
      vaapi_wrapper_(vaapi_wrapper),
      make_context_current_cb_(make_context_current_cb),
      x_display_(gfx::GetXDisplay()),
      x_pixmap_(0) {}

VaapiTFPPicture::~VaapiTFPPicture() {
  if (glx_image_.get() && make_context_current_cb_.Run()) {
    glx_image_->ReleaseTexImage(GL_TEXTURE_2D);
    glx_image_->Destroy(true);
    DCHECK_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
  }

  if (x_pixmap_)
    XFreePixmap(x_display_, x_pixmap_);
}

bool VaapiTFPPicture::Initialize() {
  if (!make_context_current_cb_.Run())
    return false;

  XWindowAttributes win_attr;
  int screen = DefaultScreen(x_display_);
  XGetWindowAttributes(x_display_, RootWindow(x_display_, screen), &win_attr);
  // TODO(posciak): pass the depth required by libva, not the RootWindow's
  // depth
  x_pixmap_ = XCreatePixmap(x_display_, RootWindow(x_display_, screen),
                            size().width(), size().height(), win_attr.depth);
  if (!x_pixmap_) {
    LOG(ERROR) << "Failed creating an X Pixmap for TFP";
    return false;
  }

  glx_image_ = new gl::GLImageGLX(size(), GL_RGB);
  if (!glx_image_->Initialize(x_pixmap_)) {
    // x_pixmap_ will be freed in the destructor.
    LOG(ERROR) << "Failed creating a GLX Pixmap for TFP";
    return false;
  }

  gfx::ScopedTextureBinder texture_binder(GL_TEXTURE_2D, texture_id());
  if (!glx_image_->BindTexImage(GL_TEXTURE_2D)) {
    LOG(ERROR) << "Failed to bind texture to glx image";
    return false;
  }

  return true;
}

bool VaapiTFPPicture::DownloadFromSurface(
    const scoped_refptr<VASurface>& va_surface) {
  return vaapi_wrapper_->PutSurfaceIntoPixmap(va_surface->id(), x_pixmap_,
                                              va_surface->size());
}

scoped_refptr<gl::GLImage> VaapiTFPPicture::GetImageToBind() {
  return nullptr;
}

}  // namespace media
