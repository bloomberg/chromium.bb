// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/scoped_app_gl_state_restore.h"

#include "base/lazy_instance.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface_stub.h"

namespace android_webview {

namespace {

// "App" context is a bit of a stretch. Basically we use this context while
// saving and restoring the App GL state.
class AppContextSurface {
 public:
  AppContextSurface()
      : surface(new gfx::GLSurfaceStub),
        context(gfx::GLContext::CreateGLContext(NULL,
                                                surface.get(),
                                                gfx::PreferDiscreteGpu)) {}
  void MakeCurrent() { context->MakeCurrent(surface.get()); }

 private:
  scoped_refptr<gfx::GLSurfaceStub> surface;
  scoped_refptr<gfx::GLContext> context;

  DISALLOW_COPY_AND_ASSIGN(AppContextSurface);
};

base::LazyInstance<AppContextSurface> g_app_context_surface =
    LAZY_INSTANCE_INITIALIZER;

// Make the global g_app_context_surface current so that the gl_binding is not
// NULL for making gl* calls. The binding can be null if another GlContext was
// destroyed immediately before gl* calls here.
void MakeAppContextCurrent() {
  g_app_context_surface.Get().MakeCurrent();
}

}  // namespace

ScopedAppGLStateRestore::ScopedAppGLStateRestore(CallMode mode) {
  MakeAppContextCurrent();

  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &vertex_array_buffer_binding_);
  glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &index_array_buffer_binding_);

  switch(mode) {
    case MODE_DRAW:
      // TODO(boliu): These should always be 0 in draw case. When we have
      // guarantee that we are no longer making GL calls outside of draw, DCHECK
      // these are 0 here.
      LOG_IF(ERROR, vertex_array_buffer_binding_)
          << "GL_ARRAY_BUFFER_BINDING not zero in draw: "
          << vertex_array_buffer_binding_;
      LOG_IF(ERROR, index_array_buffer_binding_)
          << "GL_ELEMENT_ARRAY_BUFFER_BINDING not zero in draw: "
          << index_array_buffer_binding_;

      vertex_array_buffer_binding_ = 0;
      index_array_buffer_binding_ = 0;
      break;
    case MODE_DETACH_FROM_WINDOW:
      break;
  }

  glGetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES,
                &texture_external_oes_binding_);
  glGetIntegerv(GL_PACK_ALIGNMENT, &pack_alignment_);
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack_alignment_);

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(vertex_attrib_); ++i) {
    glGetVertexAttribiv(
        i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &vertex_attrib_[i].enabled);
    glGetVertexAttribiv(
        i, GL_VERTEX_ATTRIB_ARRAY_SIZE, &vertex_attrib_[i].size);
    glGetVertexAttribiv(
        i, GL_VERTEX_ATTRIB_ARRAY_TYPE, &vertex_attrib_[i].type);
    glGetVertexAttribiv(
        i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &vertex_attrib_[i].normalized);
    glGetVertexAttribiv(
        i, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &vertex_attrib_[i].stride);
    glGetVertexAttribPointerv(
        i, GL_VERTEX_ATTRIB_ARRAY_POINTER, &vertex_attrib_[i].pointer);
  }

  glGetBooleanv(GL_DEPTH_TEST, &depth_test_);
  glGetBooleanv(GL_CULL_FACE, &cull_face_);
  glGetBooleanv(GL_COLOR_WRITEMASK, color_mask_);
  glGetBooleanv(GL_BLEND, &blend_enabled_);
  glGetIntegerv(GL_BLEND_SRC_RGB, &blend_src_rgb_);
  glGetIntegerv(GL_BLEND_SRC_ALPHA, &blend_src_alpha_);
  glGetIntegerv(GL_BLEND_DST_RGB, &blend_dest_rgb_);
  glGetIntegerv(GL_BLEND_DST_ALPHA, &blend_dest_alpha_);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &active_texture_);
  glGetIntegerv(GL_VIEWPORT, viewport_);
  glGetBooleanv(GL_SCISSOR_TEST, &scissor_test_);
  glGetIntegerv(GL_SCISSOR_BOX, scissor_box_);
  glGetIntegerv(GL_CURRENT_PROGRAM, &current_program_);
}

ScopedAppGLStateRestore::~ScopedAppGLStateRestore() {
  MakeAppContextCurrent();

  glBindBuffer(GL_ARRAY_BUFFER, vertex_array_buffer_binding_);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_array_buffer_binding_);

  glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_external_oes_binding_);
  glPixelStorei(GL_PACK_ALIGNMENT, pack_alignment_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, unpack_alignment_);

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(vertex_attrib_); ++i) {
    glVertexAttribPointer(i,
                          vertex_attrib_[i].size,
                          vertex_attrib_[i].type,
                          vertex_attrib_[i].normalized,
                          vertex_attrib_[i].stride,
                          vertex_attrib_[i].pointer);

    if (vertex_attrib_[i].enabled) {
      glEnableVertexAttribArray(i);
    } else {
      glDisableVertexAttribArray(i);
    }
  }

  if (depth_test_) {
    glEnable(GL_DEPTH_TEST);
  } else {
    glDisable(GL_DEPTH_TEST);
  }

  if (cull_face_) {
    glEnable(GL_CULL_FACE);
  } else {
    glDisable(GL_CULL_FACE);
  }

  glColorMask(color_mask_[0], color_mask_[1], color_mask_[2], color_mask_[3]);

  if (blend_enabled_) {
    glEnable(GL_BLEND);
  } else {
    glDisable(GL_BLEND);
  }

  glBlendFuncSeparate(
      blend_src_rgb_, blend_dest_rgb_, blend_src_alpha_, blend_dest_alpha_);
  glActiveTexture(active_texture_);

  glViewport(viewport_[0], viewport_[1], viewport_[2], viewport_[3]);

  if (scissor_test_) {
    glEnable(GL_SCISSOR_TEST);
  } else {
    glDisable(GL_SCISSOR_TEST);
  }

  glScissor(
      scissor_box_[0], scissor_box_[1], scissor_box_[2], scissor_box_[3]);

  glUseProgram(current_program_);
}

}  // namespace android_webview
