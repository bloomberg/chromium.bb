// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/basictypes.h"
#include "ui/gl/gl_bindings.h"

namespace gfx {
class GLContext;
}

namespace android_webview {

// This class is not thread safe and should only be used on the UI thread.
class ScopedAppGLStateRestore {
 public:
  enum CallMode {
    MODE_DRAW,
    MODE_RESOURCE_MANAGEMENT,
  };

  ScopedAppGLStateRestore(CallMode mode);
  ~ScopedAppGLStateRestore();

  bool stencil_enabled() const { return stencil_test_; }
  GLint framebuffer_binding_ext() const { return framebuffer_binding_ext_; }

 private:
  const CallMode mode_;

  GLint pack_alignment_;
  GLint unpack_alignment_;

  struct {
    GLint enabled;
    GLint size;
    GLint type;
    GLint normalized;
    GLint stride;
    GLvoid* pointer;
  } vertex_attrib_[3];

  GLint vertex_array_buffer_binding_;
  GLint index_array_buffer_binding_;

  GLboolean depth_test_;
  GLboolean cull_face_;
  GLint cull_face_mode_;
  GLboolean color_mask_[4];
  GLfloat color_clear_[4];
  GLfloat depth_clear_;
  GLint current_program_;
  GLint depth_func_;
  GLboolean depth_mask_;
  GLfloat depth_rage_[2];
  GLint front_face_;
  GLint hint_generate_mipmap_;
  GLfloat line_width_;
  GLfloat polygon_offset_factor_;
  GLfloat polygon_offset_units_;
  GLfloat sample_coverage_value_;
  GLboolean sample_coverage_invert_;

  GLboolean enable_dither_;
  GLboolean enable_polygon_offset_fill_;
  GLboolean enable_sample_alpha_to_coverage_;
  GLboolean enable_sample_coverage_;

  // Not saved/restored in MODE_DRAW.
  GLboolean blend_enabled_;
  GLint blend_src_rgb_;
  GLint blend_src_alpha_;
  GLint blend_dest_rgb_;
  GLint blend_dest_alpha_;
  GLint active_texture_;
  GLint viewport_[4];
  GLboolean scissor_test_;
  GLint scissor_box_[4];

  GLboolean stencil_test_;
  GLint stencil_func_;
  GLint stencil_mask_;
  GLint stencil_ref_;

  GLint framebuffer_binding_ext_;

  struct TextureBindings {
    GLint texture_2d;
    GLint texture_cube_map;
    GLint texture_external_oes;
    // TODO(boliu): TEXTURE_RECTANGLE_ARB
  };

  std::vector<TextureBindings> texture_bindings_;

  DISALLOW_COPY_AND_ASSIGN(ScopedAppGLStateRestore);
};

}  // namespace android_webview
