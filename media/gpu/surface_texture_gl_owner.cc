// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/surface_texture_gl_owner.h"

#include "base/logging.h"
#include "ui/gl/scoped_make_current.h"

namespace media {

scoped_refptr<SurfaceTextureGLOwner> SurfaceTextureGLOwner::Create() {
  GLuint texture_id;
  glGenTextures(1, &texture_id);
  if (!texture_id)
    return nullptr;

  return new SurfaceTextureGLOwner(texture_id);
}

SurfaceTextureGLOwner::SurfaceTextureGLOwner(GLuint texture_id)
    : SurfaceTexture(CreateJavaSurfaceTexture(texture_id)),
      context_(gl::GLContext::GetCurrent()),
      surface_(gl::GLSurface::GetCurrent()),
      texture_id_(texture_id) {
  DCHECK(context_);
  DCHECK(surface_);
}

SurfaceTextureGLOwner::~SurfaceTextureGLOwner() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Make sure that the SurfaceTexture isn't using the GL objects.
  DestroyJavaObject();

  ui::ScopedMakeCurrent scoped_make_current(context_.get(), surface_.get());
  if (scoped_make_current.Succeeded()) {
    glDeleteTextures(1, &texture_id_);
    DCHECK_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  }
}

void SurfaceTextureGLOwner::AttachToGLContext() {
  NOTIMPLEMENTED();
}

void SurfaceTextureGLOwner::DetachFromGLContext() {
  NOTIMPLEMENTED();
}

}  // namespace media
