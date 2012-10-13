// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the ContextState class.

#ifndef GPU_COMMAND_BUFFER_SERVICE_CONTEXT_STATE_H_
#define GPU_COMMAND_BUFFER_SERVICE_CONTEXT_STATE_H_

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

namespace gpu {
namespace gles2 {

// State associated with each texture unit.
struct TextureUnit {
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


struct ContextState {
  ContextState();
  ~ContextState();

  // pack alignment as last set by glPixelStorei
  GLint pack_alignment;

  // unpack alignment as last set by glPixelStorei
  GLint unpack_alignment;

  // Current active texture by 0 - n index.
  // In other words, if we call glActiveTexture(GL_TEXTURE2) this value would
  // be 2.
  GLuint active_texture_unit;

  GLfloat color_clear_red;
  GLfloat color_clear_green;
  GLfloat color_clear_blue;
  GLfloat color_clear_alpha;
  GLboolean color_mask_red;
  GLboolean color_mask_green;
  GLboolean color_mask_blue;
  GLboolean color_mask_alpha;

  GLint stencil_clear;
  GLuint stencil_mask_front;
  GLuint stencil_mask_back;
  GLclampf depth_clear;
  GLboolean depth_mask;

  bool enable_blend;
  bool enable_cull_face;
  bool enable_scissor_test;
  bool enable_depth_test;
  bool enable_stencil_test;

  // Cached values of the currently assigned viewport dimensions.
  GLint viewport_x;
  GLint viewport_y;
  GLsizei viewport_width;
  GLsizei viewport_height;
  GLsizei viewport_max_width;
  GLsizei viewport_max_height;

  // The currently bound array buffer. If this is 0 it is illegal to call
  // glVertexAttribPointer.
  BufferManager::BufferInfo::Ref bound_array_buffer;

  // Which textures are bound to texture units through glActiveTexture.
  scoped_array<TextureUnit> texture_units;

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

  GLenum cull_mode;
  GLenum front_face;
  GLenum depth_func;
  GLenum source_blend_rgb;
  GLenum dest_blend_rgb;
  GLenum source_blend_alpha;
  GLenum dest_blend_alpha;
  GLenum blend_equation_rgb;
  GLenum blend_equation_alpha;
  GLfloat blend_color_red;
  GLfloat blend_color_green;
  GLfloat blend_color_blue;
  GLfloat blend_color_alpha;
  GLenum stencil_func;
  GLint stencil_ref;
  GLenum stencil_fail;
  GLenum stencil_pass_depth_fail;
  GLenum stencil_pass_depth_pass;
  GLuint stencil_writemask;
  GLenum stencil_back_func;
  GLint stencil_back_ref;
  GLenum stencil_back_fail;
  GLenum stencil_back_pass_depth_fail;
  GLenum stencil_back_pass_depth_pass;
  GLuint stencil_back_writemask;
  bool polygon_offset_fill;
  GLfloat polygon_offset_factor;
  GLfloat polygon_offset_units;
  bool sample_alpha_to_coverage;
  bool sample_coverage;
  GLclampf sample_coverage_value;
  bool sample_coverage_invert;
  bool dither;

  GLfloat line_width;

  GLenum generate_mipmap_hint;
  GLenum fragment_shader_derivative_hint;

  float z_near;
  float z_far;

  GLint scissor_x;
  GLint scissor_y;
  GLsizei scissor_width;
  GLsizei scissor_height;

  bool pack_reverse_row_order;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_CONTEXT_STATE_H_

