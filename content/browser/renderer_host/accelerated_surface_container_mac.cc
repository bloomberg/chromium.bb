// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/accelerated_surface_container_mac.h"

#include "app/surface/io_surface_support_mac.h"
#include "base/logging.h"
#include "content/browser/renderer_host/accelerated_surface_container_manager_mac.h"
#include "webkit/plugins/npapi/webplugin.h"

AcceleratedSurfaceContainerMac::AcceleratedSurfaceContainerMac(
    AcceleratedSurfaceContainerManagerMac* manager,
    bool opaque)
    : manager_(manager),
      opaque_(opaque),
      surface_id_(0),
      width_(0),
      height_(0),
      texture_(0),
      texture_needs_upload_(true),
      texture_pending_deletion_(0),
      visible_(false),
      was_painted_to_(false) {
}

AcceleratedSurfaceContainerMac::~AcceleratedSurfaceContainerMac() {
}

void AcceleratedSurfaceContainerMac::SetSizeAndIOSurface(
    int32 width,
    int32 height,
    uint64 io_surface_identifier) {
  // Ignore |io_surface_identifier|: The surface hasn't been painted to and
  // only contains garbage data. Update the surface in |set_was_painted_to()|
  // instead.
  width_ = width;
  height_ = height;
}

void AcceleratedSurfaceContainerMac::SetSizeAndTransportDIB(
    int32 width,
    int32 height,
    TransportDIB::Handle transport_dib) {
  if (TransportDIB::is_valid(transport_dib)) {
    transport_dib_.reset(TransportDIB::Map(transport_dib));
    EnqueueTextureForDeletion();
    width_ = width;
    height_ = height;
  }
}

void AcceleratedSurfaceContainerMac::SetGeometry(
    const webkit::npapi::WebPluginGeometry& geom) {
  visible_ = geom.visible;
  if (geom.rects_valid)
    clip_rect_ = geom.clip_rect;
}

void AcceleratedSurfaceContainerMac::Draw(CGLContextObj context) {
  IOSurfaceSupport* io_surface_support = IOSurfaceSupport::Initialize();
  GLenum target = GL_TEXTURE_RECTANGLE_ARB;
  if (texture_pending_deletion_) {
    // Clean up an old texture object. This is essentially a pre-emptive
    // cleanup, as the resources will be released when the OpenGL context
    // associated with our containing NSView is destroyed. However, if we
    // resize a plugin often, we might generate a lot of textures, so we
    // should try to eagerly reclaim their resources. Note also that the
    // OpenGL context must be current when performing the deletion, and it
    // seems risky to make the OpenGL context current at an arbitrary point
    // in time, which is why the deletion does not occur in the container's
    // destructor.
    glDeleteTextures(1, &texture_pending_deletion_);
    texture_pending_deletion_ = 0;
  }
  if (!texture_) {
    if ((io_surface_support && !surface_.get()) ||
        (!io_surface_support && !transport_dib_.get()))
      return;
    glGenTextures(1, &texture_);
    glBindTexture(target, texture_);
    glTexParameterf(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    if (io_surface_support) {
      texture_needs_upload_ = true;
    } else {
      // Reserve space on the card for the actual texture upload, done with the
      // glTexSubImage2D() call, below.
      glTexImage2D(target,
                   0,  // mipmap level 0
                   GL_RGBA,  // internal format
                   width_,
                   height_,
                   0,  // no border
                   GL_BGRA,  // The GPU plugin read BGRA pixels
                   GL_UNSIGNED_INT_8_8_8_8_REV,
                   NULL);  // No data, this call just reserves room.
    }
  }

  // When using an IOSurface, the texture does not need to be repeatedly
  // uploaded, just when we've been told we have to.
  if (io_surface_support && texture_needs_upload_) {
    DCHECK(surface_.get());
    glBindTexture(target, texture_);
    // Don't think we need to identify a plane.
    GLuint plane = 0;
    io_surface_support->CGLTexImageIOSurface2D(context,
                                               target,
                                               GL_RGBA,
                                               surface_width_,
                                               surface_height_,
                                               GL_BGRA,
                                               GL_UNSIGNED_INT_8_8_8_8_REV,
                                               surface_.get(),
                                               plane);
    texture_needs_upload_ = false;
  }
  // If using TransportDIBs, the texture needs to be uploaded every frame.
  if (transport_dib_.get() != NULL) {
    void* pixel_memory = transport_dib_->memory();
    if (pixel_memory) {
      glBindTexture(target, texture_);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);  // Needed for NPOT textures.
      glTexSubImage2D(target,
                      0,  // mipmap level 0
                      0,  // x-offset
                      0,  // y-offset
                      width_,
                      height_,
                      GL_BGRA,  // The GPU plugin gave us BGRA pixels
                      GL_UNSIGNED_INT_8_8_8_8_REV,
                      pixel_memory);
    }
  }

  if (texture_) {
    int texture_width = io_surface_support ? surface_width_ : width_;
    int texture_height = io_surface_support ? surface_height_ : height_;

    // TODO(kbr): convert this to use only OpenGL ES 2.0 functionality.

    // TODO(kbr): may need to pay attention to cutout rects.
    int clipX = clip_rect_.x();
    int clipY = clip_rect_.y();
    int clipWidth = clip_rect_.width();
    int clipHeight = clip_rect_.height();

    if (clipX + clipWidth > texture_width)
      clipWidth = texture_width - clipX;
    if (clipY + clipHeight > texture_height)
      clipHeight = texture_height - clipY;

    if (opaque_) {
      // Pepper 3D's output is currently considered opaque even if the
      // program draws pixels with alpha less than 1. In order to have
      // this effect, we need to drop the alpha channel of the input,
      // replacing it with alpha = 1.

      // First fill the rectangle with alpha=1.
      glColorMask(false, false, false, true);
      glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
      glBegin(GL_TRIANGLE_STRIP);
      glVertex3f(0, 0, 0);
      glVertex3f(clipWidth, 0, 0);
      glVertex3f(0, clipHeight, 0);
      glVertex3f(clipWidth, clipHeight, 0);
      glEnd();

      // Now draw the color channels from the incoming texture.
      glColorMask(true, true, true, false);
      // This call shouldn't be necessary -- we are using the GL_REPLACE
      // texture environment mode -- but it appears to be.
      glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    } else {
      glColorMask(true, true, true, true);
    }

    // Draw the color channels from the incoming texture.
    glBindTexture(target, texture_);
    glEnable(target);
    glBegin(GL_TRIANGLE_STRIP);

      glTexCoord2f(clipX, texture_height - clipY);
      glVertex3f(0, 0, 0);

      glTexCoord2f(clipX + clipWidth, texture_height - clipY);
      glVertex3f(clipWidth, 0, 0);

      glTexCoord2f(clipX, texture_height - clipY - clipHeight);
      glVertex3f(0, clipHeight, 0);

      glTexCoord2f(clipX + clipWidth, texture_height - clipY - clipHeight);
      glVertex3f(clipWidth, clipHeight, 0);

    glEnd();
    glDisable(target);
  }
}

bool AcceleratedSurfaceContainerMac::ShouldBeVisible() const {
  return visible_ && was_painted_to_ && !clip_rect_.IsEmpty();
}

void AcceleratedSurfaceContainerMac::set_was_painted_to(uint64 surface_id) {
  if (surface_id && (!surface_ || surface_id != surface_id_)) {
    // Keep the surface that was most recently painted to around.
    if (IOSurfaceSupport* io_surface_support = IOSurfaceSupport::Initialize()) {
      CFTypeRef surface = io_surface_support->IOSurfaceLookup(
          static_cast<uint32>(surface_id));
      // Can fail if IOSurface with that ID was already released by the
      // gpu process or the plugin process. We will get a |set_was_painted_to()|
      // message with a new surface soon in that case.
      if (surface) {
        surface_.reset(surface);
        surface_id_ = surface_id;
        surface_width_ = io_surface_support->IOSurfaceGetWidth(surface_);
        surface_height_ = io_surface_support->IOSurfaceGetHeight(surface_);
        EnqueueTextureForDeletion();
      }
    }
  }
  was_painted_to_ = true;
}

void AcceleratedSurfaceContainerMac::EnqueueTextureForDeletion() {
  if (texture_) {
    DCHECK(texture_pending_deletion_ == 0);
    texture_pending_deletion_ = texture_;
    texture_ = 0;
  }
}

