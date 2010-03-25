// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/id_manager.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/renderbuffer_manager.h"
#include "gpu/command_buffer/service/shader_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"

namespace gpu {
namespace gles2 {

ContextGroup::ContextGroup()
    : initialized_(false),
      max_vertex_attribs_(0u),
      max_texture_units_(0u) {
}

ContextGroup::~ContextGroup() {
}

bool ContextGroup::Initialize() {
  if (initialized_) {
    return true;
  }

  id_manager_.reset(new IdManager());
  buffer_manager_.reset(new BufferManager());
  framebuffer_manager_.reset(new FramebufferManager());
  renderbuffer_manager_.reset(new RenderbufferManager());
  shader_manager_.reset(new ShaderManager());
  program_manager_.reset(new ProgramManager());

  // Lookup GL things we need to know.
  GLint value;
  glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &value);
  max_vertex_attribs_ = value;
  const GLuint kGLES2RequiredMiniumumVertexAttribs = 8u;
  DCHECK_GE(max_vertex_attribs_, kGLES2RequiredMiniumumVertexAttribs);

  glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &value);
  max_texture_units_ = value;
  const GLuint kGLES2RequiredMiniumumTextureUnits = 8u;
  DCHECK_GE(max_texture_units_, kGLES2RequiredMiniumumTextureUnits);

  GLint max_texture_size;
  GLint max_cube_map_texture_size;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
  glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &max_cube_map_texture_size);
  texture_manager_.reset(new TextureManager(max_texture_size,
                                            max_cube_map_texture_size));
  initialized_ = true;
  return true;
}

}  // namespace gles2
}  // namespace gpu


