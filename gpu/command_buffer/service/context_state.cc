// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/context_state.h"

#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"

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

ContextState::ContextState(FeatureInfo* feature_info)
    : pack_alignment(4),
      unpack_alignment(4),
      active_texture_unit(0),
      hint_generate_mipmap(GL_DONT_CARE),
      hint_fragment_shader_derivative(GL_DONT_CARE),
      pack_reverse_row_order(false),
      feature_info_(feature_info) {
  Initialize();
}

ContextState::~ContextState() {
}

void ContextState::RestoreTextureUnitBindings(GLuint unit) const {
  DCHECK_LT(unit, texture_units.size());
  const TextureUnit& texture_unit = texture_units[unit];
  glActiveTexture(GL_TEXTURE0 + unit);
  GLuint service_id = texture_unit.bound_texture_2d ?
      texture_unit.bound_texture_2d->service_id() : 0;
  glBindTexture(GL_TEXTURE_2D, service_id);
  service_id = texture_unit.bound_texture_cube_map ?
      texture_unit.bound_texture_cube_map->service_id() : 0;
  glBindTexture(GL_TEXTURE_CUBE_MAP, service_id);

  if (feature_info_->feature_flags().oes_egl_image_external) {
    service_id = texture_unit.bound_texture_external_oes ?
        texture_unit.bound_texture_external_oes->service_id() : 0;
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, service_id);
  }

  if (feature_info_->feature_flags().arb_texture_rectangle) {
    service_id = texture_unit.bound_texture_rectangle_arb ?
        texture_unit.bound_texture_rectangle_arb->service_id() : 0;
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, service_id);
  }
}

void ContextState::RestoreBufferBindings() const {
  if (vertex_attrib_manager) {
    BufferManager::BufferInfo* element_array_buffer =
        vertex_attrib_manager->element_array_buffer();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
        element_array_buffer ? element_array_buffer->service_id() : 0);
  }
  glBindBuffer(
      GL_ARRAY_BUFFER,
      bound_array_buffer ? bound_array_buffer->service_id() : 0);
}

void ContextState::RestoreRenderbufferBindings() const {
  // Restore Bindings
  glBindRenderbufferEXT(
      GL_RENDERBUFFER,
      bound_renderbuffer ? bound_renderbuffer->service_id() : 0);
}

void ContextState::RestoreProgramBindings() const {
  glUseProgram(current_program ? current_program->service_id() : 0);
}

void ContextState::RestoreActiveTexture() const {
  glActiveTexture(GL_TEXTURE0 + active_texture_unit);
}

void ContextState::RestoreAttribute(GLuint attrib) const {
  const VertexAttribManager::VertexAttribInfo* info =
      vertex_attrib_manager->GetVertexAttribInfo(attrib);
  const void* ptr = reinterpret_cast<const void*>(info->offset());
  BufferManager::BufferInfo* buffer_info = info->buffer();
  glBindBuffer(
      GL_ARRAY_BUFFER, buffer_info ? buffer_info->service_id() : 0);
  glVertexAttribPointer(
      attrib, info->size(), info->type(), info->normalized(),
      info->gl_stride(), ptr);
  if (info->divisor())
    glVertexAttribDivisorANGLE(attrib, info->divisor());
  // Never touch vertex attribute 0's state (in particular, never
  // disable it) when running on desktop GL because it will never be
  // re-enabled.
  if (attrib != 0 ||
      gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2) {
    if (info->enabled()) {
      glEnableVertexAttribArray(attrib);
    } else {
      glDisableVertexAttribArray(attrib);
    }
  }
  glVertexAttrib4fv(attrib, attrib_values[attrib].v);
}

void ContextState::RestoreGlobalState() const {
  InitCapabilities();
  InitState();

  glPixelStorei(GL_PACK_ALIGNMENT, pack_alignment);
  glPixelStorei(GL_UNPACK_ALIGNMENT, unpack_alignment);

  glHint(GL_GENERATE_MIPMAP_HINT, hint_generate_mipmap);
  // TODO: If OES_standard_derivatives is available
  // restore GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES
}

void ContextState::RestoreState() const {
  // Restore Texture state.
  for (size_t ii = 0; ii < texture_units.size(); ++ii) {
    RestoreTextureUnitBindings(ii);
  }
  RestoreActiveTexture();

  // Restore Attrib State
  // TODO: This if should not be needed. RestoreState is getting called
  // before GLES2Decoder::Initialize which is a bug.
  if (vertex_attrib_manager) {
    // TODO(gman): Move this restoration to VertexAttribManager.
    for (size_t attrib = 0; attrib < vertex_attrib_manager->num_attribs();
         ++attrib) {
      RestoreAttribute(attrib);
    }
  }

  RestoreBufferBindings();
  RestoreRenderbufferBindings();
  RestoreProgramBindings();
  RestoreGlobalState();
}

// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "gpu/command_buffer/service/context_state_impl_autogen.h"

}  // namespace gles2
}  // namespace gpu


