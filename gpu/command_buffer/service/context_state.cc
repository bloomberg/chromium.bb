// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/context_state.h"

namespace gpu {
namespace gles2 {

TextureUnit::TextureUnit()
    : bind_target(GL_TEXTURE_2D) {
}

TextureUnit::~TextureUnit() {
}

ContextState::ContextState()
    : pack_alignment(4),
      unpack_alignment(4),
      active_texture_unit(0),
      color_clear_red(0),
      color_clear_green(0),
      color_clear_blue(0),
      color_clear_alpha(0),
      color_mask_red(true),
      color_mask_green(true),
      color_mask_blue(true),
      color_mask_alpha(true),
      stencil_clear(0),
      stencil_mask_front(-1),
      stencil_mask_back(-1),
      depth_clear(1.0f),
      depth_mask(true),
      enable_blend(false),
      enable_cull_face(false),
      enable_scissor_test(false),
      enable_depth_test(false),
      enable_stencil_test(false),
      viewport_x(0),
      viewport_y(0),
      viewport_width(0),
      viewport_height(0),
      viewport_max_width(0),
      viewport_max_height(0) {
}

ContextState::~ContextState() {
}

}  // namespace gles2
}  // namespace gpu


