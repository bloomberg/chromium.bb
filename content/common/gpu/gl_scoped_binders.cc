// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gl_scoped_binders.h"
#include "ui/gl/gl_bindings.h"

namespace content {

ScopedFrameBufferBinder::ScopedFrameBufferBinder(unsigned int fbo) {
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &old_fbo_);
  glBindFramebufferEXT(GL_FRAMEBUFFER, fbo);
}

ScopedFrameBufferBinder::~ScopedFrameBufferBinder() {
  glBindFramebufferEXT(GL_FRAMEBUFFER, old_fbo_);
}

ScopedTextureBinder::ScopedTextureBinder(unsigned int id) {
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &old_id_);
  glBindTexture(GL_TEXTURE_2D, id);
}

ScopedTextureBinder::~ScopedTextureBinder() {
  glBindTexture(GL_TEXTURE_2D, old_id_);
}

}  // namespace content
