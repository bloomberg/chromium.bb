// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the ContextState class.

#ifndef GPU_COMMAND_BUFFER_SERVICE_CONTEXT_STATE_H_
#define GPU_COMMAND_BUFFER_SERVICE_CONTEXT_STATE_H_

#include <vector>
#include "base/logging.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/query_manager.h"
#include "gpu/command_buffer/service/renderbuffer_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/command_buffer/service/vertex_attrib_manager.h"
#include "gpu/command_buffer/service/vertex_array_manager.h"
#include "gpu/gpu_export.h"

namespace gpu {
namespace gles2 {

class FeatureInfo;

// State associated with each texture unit.
struct GPU_EXPORT TextureUnit {
  TextureUnit();
  ~TextureUnit();

  // The last target that was bound to this texture unit.
  GLenum bind_target;

  // texture currently bound to this unit's GL_TEXTURE_2D with glBindTexture
  TextureManager::TextureInfo::Ref bound_texture_2d;

  // texture currently bound to this unit's GL_TEXTURE_CUBE_MAP with
  // glBindTexture
  TextureManager::TextureInfo::Ref bound_texture_cube_map;

  // texture currently bound to this unit's GL_TEXTURE_EXTERNAL_OES with
  // glBindTexture
  TextureManager::TextureInfo::Ref bound_texture_external_oes;

  // texture currently bound to this unit's GL_TEXTURE_RECTANGLE_ARB with
  // glBindTexture
  TextureManager::TextureInfo::Ref bound_texture_rectangle_arb;

  TextureManager::TextureInfo::Ref GetInfoForSamplerType(GLenum type) {
    DCHECK(type == GL_SAMPLER_2D || type == GL_SAMPLER_CUBE ||
           type == GL_SAMPLER_EXTERNAL_OES || type == GL_SAMPLER_2D_RECT_ARB);
    switch (type) {
      case GL_SAMPLER_2D:
        return bound_texture_2d;
      case GL_SAMPLER_CUBE:
        return bound_texture_cube_map;
      case GL_SAMPLER_EXTERNAL_OES:
        return bound_texture_external_oes;
      case GL_SAMPLER_2D_RECT_ARB:
        return bound_texture_rectangle_arb;
    }

    NOTREACHED();
    return NULL;
  }

  void Unbind(TextureManager::TextureInfo* texture) {
    if (bound_texture_2d == texture) {
      bound_texture_2d = NULL;
    }
    if (bound_texture_cube_map == texture) {
      bound_texture_cube_map = NULL;
    }
    if (bound_texture_external_oes == texture) {
      bound_texture_external_oes = NULL;
    }
  }
};

struct Vec4 {
  Vec4() {
    v[0] = 0.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
    v[3] = 1.0f;
  }
  float v[4];
};

struct GPU_EXPORT ContextState {
  explicit ContextState(FeatureInfo* feature_info);
  ~ContextState();

  void Initialize();

  void RestoreState() const;
  void InitCapabilities() const;
  void InitState() const;

  void RestoreActiveTexture() const;
  void RestoreAttribute(GLuint index) const;
  void RestoreBufferBindings() const;
  void RestoreGlobalState() const;
  void RestoreProgramBindings() const;
  void RestoreRenderbufferBindings() const;
  void RestoreTextureUnitBindings(GLuint unit) const;

  // Helper for getting cached state.
  bool GetStateAsGLint(
      GLenum pname, GLint* params, GLsizei* num_written) const;
  bool GetStateAsGLfloat(
      GLenum pname, GLfloat* params, GLsizei* num_written) const;
  bool GetEnabled(GLenum cap) const;

  #include "gpu/command_buffer/service/context_state_autogen.h"

  EnableFlags enable_flags;

  // pack alignment as last set by glPixelStorei
  GLint pack_alignment;

  // unpack alignment as last set by glPixelStorei
  GLint unpack_alignment;

  // Current active texture by 0 - n index.
  // In other words, if we call glActiveTexture(GL_TEXTURE2) this value would
  // be 2.
  GLuint active_texture_unit;

  // The currently bound array buffer. If this is 0 it is illegal to call
  // glVertexAttribPointer.
  BufferManager::BufferInfo::Ref bound_array_buffer;

  // Which textures are bound to texture units through glActiveTexture.
  std::vector<TextureUnit> texture_units;

  // The values for each attrib.
  std::vector<Vec4> attrib_values;

  // Class that manages vertex attribs.
  VertexAttribManager::Ref vertex_attrib_manager;

  // The program in use by glUseProgram
  ProgramManager::ProgramInfo::Ref current_program;

  // The currently bound framebuffers
  FramebufferManager::FramebufferInfo::Ref bound_read_framebuffer;
  FramebufferManager::FramebufferInfo::Ref bound_draw_framebuffer;

  // The currently bound renderbuffer
  RenderbufferManager::RenderbufferInfo::Ref bound_renderbuffer;

  QueryManager::Query::Ref current_query;

  GLenum hint_generate_mipmap;
  GLenum hint_fragment_shader_derivative;

  bool pack_reverse_row_order;

  FeatureInfo* feature_info_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_CONTEXT_STATE_H_

