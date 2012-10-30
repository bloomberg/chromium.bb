// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/context_state.h"

#include "gpu/command_buffer/common/gles2_cmd_utils.h"

namespace gpu {
namespace gles2 {

namespace {

void EnableDisable(GLenum pname, bool enable) {
  if (enable) {
    glEnable(pname);
  } else {
    glDisable(pname);
  }
}

}  // anonymous namespace.

TextureUnit::TextureUnit()
    : bind_target(GL_TEXTURE_2D) {
}

TextureUnit::~TextureUnit() {
}

ContextState::ContextState()
    : pack_alignment(4),
      unpack_alignment(4),
      active_texture_unit(0),
      viewport_max_width(0),
      viewport_max_height(0),
      hint_generate_mipmap(GL_DONT_CARE),
      hint_fragment_shader_derivative(GL_DONT_CARE),
      pack_reverse_row_order(false) {
  Initialize();
}

ContextState::~ContextState() {
}

// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "gpu/command_buffer/service/context_state_impl_autogen.h"

}  // namespace gles2
}  // namespace gpu


