// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "ui/gl/gl_bindings.h"

namespace gfx {
class GLContext;
}

namespace android_webview {

// This class is not thread safe and should only be used on the UI thread.
class ScopedAppGLStateRestore {
 public:
  ScopedAppGLStateRestore();
  ~ScopedAppGLStateRestore();

 private:
  GLint texture_external_oes_binding_;
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

  GLboolean depth_test_;
  GLboolean cull_face_;
  GLboolean color_mask_[4];
  GLboolean blend_enabled_;
  GLint blend_src_rgb_;
  GLint blend_src_alpha_;
  GLint blend_dest_rgb_;
  GLint blend_dest_alpha_;
  GLint active_texture_;
  GLint viewport_[4];
  GLboolean scissor_test_;
  GLint scissor_box_[4];
  GLint current_program_;

  DISALLOW_COPY_AND_ASSIGN(ScopedAppGLStateRestore);
};

}  // namespace android_webview
