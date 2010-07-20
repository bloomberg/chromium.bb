// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/common/id_allocator.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/renderbuffer_manager.h"
#include "gpu/command_buffer/service/shader_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/GLES2/gles2_command_buffer.h"

namespace gpu {
namespace gles2 {

ContextGroup::ContextGroup()
    : initialized_(false),
      max_vertex_attribs_(0u),
      max_texture_units_(0u),
      max_texture_image_units_(0u),
      max_vertex_texture_image_units_(0u),
      max_fragment_uniform_vectors_(0u),
      max_varying_vectors_(0u),
      max_vertex_uniform_vectors_(0u) {
}

ContextGroup::~ContextGroup() {
  // Check that Destroy has been called.
  DCHECK(buffer_manager_ == NULL);
  DCHECK(framebuffer_manager_ == NULL);
  DCHECK(renderbuffer_manager_ == NULL);
  DCHECK(texture_manager_ == NULL);
  DCHECK(program_manager_ == NULL);
  DCHECK(shader_manager_ == NULL);
}

static void GetIntegerv(GLenum pname, uint32* var) {
  GLint value = 0;
  glGetIntegerv(pname, &value);
  *var = value;
}

bool ContextGroup::Initialize() {
  if (initialized_) {
    return true;
  }

  // Figure out what extensions to turn on.
  const char* extensions =
      reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
  bool npot_ok = false;

  AddExtensionString("GL_CHROMIUM_map_sub");

  // Check if we should allow GL_EXT_texture_compression_dxt1.
  if (strstr(extensions, "GL_EXT_texture_compression_dxt1") ||
      strstr(extensions, "GL_EXT_texture_compression_s3tc")) {
    AddExtensionString("GL_EXT_texture_compression_dxt1");
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_RGBA_S3TC_DXT1_EXT);
  }

  // Check if we should allow GL_EXT_texture_format_BGRA8888
  if (strstr(extensions, "GL_EXT_texture_format_BGRA8888") ||
      strstr(extensions, "GL_APPLE_texture_format_BGRA8888") ||
      strstr(extensions, "GL_EXT_bgra")) {
    AddExtensionString("GL_EXT_texture_format_BGRA8888");
    validators_.texture_internal_format.AddValue(GL_BGRA_EXT);
    validators_.texture_format.AddValue(GL_BGRA_EXT);
  }

  // Check if we should allow GL_OES_texture_npot
  if (strstr(extensions, "GL_ARB_texture_non_power_of_two") ||
      strstr(extensions, "GL_OES_texture_npot")) {
    AddExtensionString("GL_OES_texture_npot");
    npot_ok = true;
  }

  // Check if we should allow GL_OES_texture_float, GL_OES_texture_half_float,
  // GL_OES_texture_float_linear, GL_OES_texture_half_float_linear
  bool enable_texture_float = false;
  bool enable_texture_float_linear = false;
  bool enable_texture_half_float = false;
  bool enable_texture_half_float_linear = false;
  if (strstr(extensions, "GL_ARB_texture_float")) {
    enable_texture_float = true;
    enable_texture_float_linear = true;
    enable_texture_half_float = true;
    enable_texture_half_float_linear = true;
  } else {
    if (strstr(extensions, "GL_OES_texture_float")) {
      enable_texture_float = true;
      if (strstr(extensions, "GL_OES_texture_float_linear")) {
        enable_texture_float_linear = true;
      }
    }
    if (strstr(extensions, "GL_OES_texture_half_float")) {
      enable_texture_half_float = true;
      if (strstr(extensions, "GL_OES_texture_half_float_linear")) {
        enable_texture_half_float_linear = true;
      }
    }
  }

  if (enable_texture_float) {
    validators_.pixel_type.AddValue(GL_FLOAT);
    AddExtensionString("GL_OES_texture_float");
    if (enable_texture_float_linear) {
      AddExtensionString("GL_OES_texture_float_linear");
    }
  }

  if (enable_texture_half_float) {
    validators_.pixel_type.AddValue(GL_HALF_FLOAT_OES);
    AddExtensionString("GL_OES_texture_half_float");
    if (enable_texture_half_float_linear) {
      AddExtensionString("GL_OES_texture_half_float_linear");
    }
  }

  // TODO(gman): Add support for these extensions.
  //     GL_OES_depth24
  //     GL_OES_depth32
  //     GL_OES_packed_depth_stencil
  //     GL_OES_element_index_uint

  buffer_manager_.reset(new BufferManager());
  framebuffer_manager_.reset(new FramebufferManager());
  renderbuffer_manager_.reset(new RenderbufferManager());
  shader_manager_.reset(new ShaderManager());
  program_manager_.reset(new ProgramManager());

  // Lookup GL things we need to know.
  GetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_vertex_attribs_);
  const GLuint kGLES2RequiredMiniumumVertexAttribs = 8u;
  DCHECK_GE(max_vertex_attribs_, kGLES2RequiredMiniumumVertexAttribs);

  GetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &max_texture_units_);
  const GLuint kGLES2RequiredMiniumumTextureUnits = 8u;
  DCHECK_GE(max_texture_units_, kGLES2RequiredMiniumumTextureUnits);

  GLint max_texture_size;
  GLint max_cube_map_texture_size;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
  glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &max_cube_map_texture_size);
  texture_manager_.reset(new TextureManager(npot_ok,
                                            enable_texture_float_linear,
                                            enable_texture_half_float_linear,
                                            max_texture_size,
                                            max_cube_map_texture_size));

  GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_image_units_);
  GetIntegerv(
      GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &max_vertex_texture_image_units_);

#if defined(GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2)

  GetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS, &max_fragment_uniform_vectors_);
  GetIntegerv(GL_MAX_VARYING_VECTORS, &max_varying_vectors_);
  GetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &max_vertex_uniform_vectors_);

#else  // !defined(GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2)

  GetIntegerv(
      GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &max_fragment_uniform_vectors_);
  max_fragment_uniform_vectors_ /= 4;
  GetIntegerv(GL_MAX_VARYING_FLOATS, &max_varying_vectors_);
  max_varying_vectors_ /= 4;
  GetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &max_vertex_uniform_vectors_);
  max_vertex_uniform_vectors_ /= 4;

#endif  // !defined(GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2)

  initialized_ = true;
  return true;
}

void ContextGroup::Destroy(bool have_context) {
  if (buffer_manager_ != NULL) {
    buffer_manager_->Destroy(have_context);
    buffer_manager_.reset();
  }

  if (framebuffer_manager_ != NULL) {
    framebuffer_manager_->Destroy(have_context);
    framebuffer_manager_.reset();
  }

  if (renderbuffer_manager_ != NULL) {
    renderbuffer_manager_->Destroy(have_context);
    renderbuffer_manager_.reset();
  }

  if (texture_manager_ != NULL) {
    texture_manager_->Destroy(have_context);
    texture_manager_.reset();
  }

  if (program_manager_ != NULL) {
    program_manager_->Destroy(have_context);
    program_manager_.reset();
  }

  if (shader_manager_ != NULL) {
    shader_manager_->Destroy(have_context);
    shader_manager_.reset();
  }
}

void ContextGroup::AddExtensionString(const std::string& str) {
  extensions_ += (extensions_.empty() ? "" : " ") + str;
}

IdAllocator* ContextGroup::GetIdAllocator(unsigned namespace_id) {
  IdAllocatorMap::iterator it = id_namespaces_.find(namespace_id);
  if (it != id_namespaces_.end()) {
    return it->second.get();
  }
  IdAllocator* id_allocator = new IdAllocator();
  id_namespaces_[namespace_id] = linked_ptr<IdAllocator>(id_allocator);
  return id_allocator;
}

}  // namespace gles2
}  // namespace gpu


