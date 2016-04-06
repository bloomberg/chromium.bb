// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gles2/gl_helper.h"

#include "base/macros.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace mus {

namespace {

class ScopedGLuint {
 public:
  typedef void (gpu::gles2::GLES2Interface::*GenFunc)(GLsizei n, GLuint* ids);
  typedef void (gpu::gles2::GLES2Interface::*DeleteFunc)(GLsizei n,
                                                         const GLuint* ids);
  ScopedGLuint(gpu::gles2::GLES2Interface* gl,
               GenFunc gen_func,
               DeleteFunc delete_func)
      : gl_(gl), id_(0u), delete_func_(delete_func) {
    (gl_->*gen_func)(1, &id_);
  }

  operator GLuint() const { return id_; }

  GLuint id() const { return id_; }

  ~ScopedGLuint() {
    if (id_ != 0) {
      (gl_->*delete_func_)(1, &id_);
    }
  }

 private:
  gpu::gles2::GLES2Interface* gl_;
  GLuint id_;
  DeleteFunc delete_func_;

  DISALLOW_COPY_AND_ASSIGN(ScopedGLuint);
};

class ScopedFramebuffer : public ScopedGLuint {
 public:
  explicit ScopedFramebuffer(gpu::gles2::GLES2Interface* gl)
      : ScopedGLuint(gl,
                     &gpu::gles2::GLES2Interface::GenFramebuffers,
                     &gpu::gles2::GLES2Interface::DeleteFramebuffers) {}
};

template <GLenum Target>
class ScopedBinder {
 public:
  typedef void (gpu::gles2::GLES2Interface::*BindFunc)(GLenum target,
                                                       GLuint id);
  ScopedBinder(gpu::gles2::GLES2Interface* gl, GLuint id, BindFunc bind_func)
      : gl_(gl), bind_func_(bind_func) {
    (gl_->*bind_func_)(Target, id);
  }

  virtual ~ScopedBinder() { (gl_->*bind_func_)(Target, 0); }

 private:
  gpu::gles2::GLES2Interface* gl_;
  BindFunc bind_func_;

  DISALLOW_COPY_AND_ASSIGN(ScopedBinder);
};

template <GLenum Target>
class ScopedFramebufferBinder : ScopedBinder<Target> {
 public:
  ScopedFramebufferBinder(gpu::gles2::GLES2Interface* gl, GLuint id)
      : ScopedBinder<Target>(gl,
                             id,
                             &gpu::gles2::GLES2Interface::BindFramebuffer) {}
};

}  // namespace

// Cribbed from content/common/gpu/client/gl_helper.cc
void GLCopySubBufferDamage(gpu::gles2::GLES2Interface* gl,
                           GLenum target,
                           GLuint texture,
                           GLuint previous_texture,
                           const SkRegion& new_damage,
                           const SkRegion& old_damage) {
  SkRegion region(old_damage);
  if (region.op(new_damage, SkRegion::kDifference_Op)) {
    ScopedFramebuffer dst_framebuffer(gl);
    ScopedFramebufferBinder<GL_FRAMEBUFFER> framebuffer_binder(gl,
                                                               dst_framebuffer);
    gl->BindTexture(target, texture);
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target,
                             previous_texture, 0);
    for (SkRegion::Iterator it(region); !it.done(); it.next()) {
      const SkIRect& rect = it.rect();
      gl->CopyTexSubImage2D(target, 0, rect.x(), rect.y(), rect.x(), rect.y(),
                            rect.width(), rect.height());
    }
    gl->BindTexture(target, 0);
    gl->Flush();
  }
}

}  // namespace mus