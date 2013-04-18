// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/context_state.h"

#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/error_state.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/renderbuffer_manager.h"
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

ContextState::ContextState(FeatureInfo* feature_info, Logger* logger)
    : pack_alignment(4),
      unpack_alignment(4),
      active_texture_unit(0),
      hint_generate_mipmap(GL_DONT_CARE),
      hint_fragment_shader_derivative(GL_DONT_CARE),
      pack_reverse_row_order(false),
      fbo_binding_for_scissor_workaround_dirty_(false),
      feature_info_(feature_info),
      error_state_(ErrorState::Create(logger)) {
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
    Buffer* element_array_buffer =
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

void ContextState::RestoreAttribute(GLuint attrib_index) const {
  const VertexAttrib* attrib =
      vertex_attrib_manager->GetVertexAttrib(attrib_index);
  const void* ptr = reinterpret_cast<const void*>(attrib->offset());
  Buffer* buffer = attrib->buffer();
  glBindBuffer(GL_ARRAY_BUFFER, buffer ? buffer->service_id() : 0);
  glVertexAttribPointer(
      attrib_index, attrib->size(), attrib->type(), attrib->normalized(),
      attrib->gl_stride(), ptr);
  if (attrib->divisor())
    glVertexAttribDivisorANGLE(attrib_index, attrib->divisor());
  // Never touch vertex attribute 0's state (in particular, never
  // disable it) when running on desktop GL because it will never be
  // re-enabled.
  if (attrib_index != 0 ||
      gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2) {
    if (attrib->enabled()) {
      glEnableVertexAttribArray(attrib_index);
    } else {
      glDisableVertexAttribArray(attrib_index);
    }
  }
  glVertexAttrib4fv(attrib_index, attrib_values[attrib_index].v);
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

ErrorState* ContextState::GetErrorState() {
  return error_state_.get();
}

// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "gpu/command_buffer/service/context_state_impl_autogen.h"

}  // namespace gles2
}  // namespace gpu


