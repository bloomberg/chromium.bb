// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/gl/gl_implementation.h"
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

  // Check if we should allow GL_EXT_texture_compression_dxt1 and
  // GL_EXT_texture_compression_s3tc.
  bool enable_dxt1 = false;
  bool enable_s3tc = false;

  if (strstr(extensions, "GL_EXT_texture_compression_dxt1")) {
    enable_dxt1 = true;
  }
  if (strstr(extensions, "GL_EXT_texture_compression_s3tc")) {
    enable_dxt1 = true;
    enable_s3tc = true;
  }

  if (enable_dxt1) {
    AddExtensionString("GL_EXT_texture_compression_dxt1");
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_RGBA_S3TC_DXT1_EXT);
  }

  if (enable_s3tc) {
    AddExtensionString("GL_EXT_texture_compression_s3tc");
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_RGBA_S3TC_DXT3_EXT);
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
  }

  // Check if we should enable GL_EXT_texture_filter_anisotropic.
  if (strstr(extensions, "GL_EXT_texture_filter_anisotropic")) {
    AddExtensionString("GL_EXT_texture_filter_anisotropic");
    validators_.texture_parameter.AddValue(
        GL_TEXTURE_MAX_ANISOTROPY_EXT);
    validators_.g_l_state.AddValue(
        GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT);
  }

  // Check if we should support GL_OES_packed_depth_stencil and/or
  // GL_GOOGLE_depth_texture.
  // NOTE: GL_OES_depth_texture requires support for depth
  // cubemaps. GL_ARB_depth_texture requires other features that
  // GL_OES_packed_depth_stencil does not provide. Therefore we made up
  // GL_GOOGLE_depth_texture.
  bool enable_depth_texture = false;
  if (strstr(extensions, "GL_ARB_depth_texture") ||
      strstr(extensions, "GL_OES_depth_texture")) {
    enable_depth_texture = true;
    AddExtensionString("GL_GOOGLE_depth_texture");
    validators_.texture_internal_format.AddValue(GL_DEPTH_COMPONENT);
    validators_.texture_format.AddValue(GL_DEPTH_COMPONENT);
    validators_.pixel_type.AddValue(GL_UNSIGNED_SHORT);
    validators_.pixel_type.AddValue(GL_UNSIGNED_INT);
  }
  // TODO(gman): Add depth types fo ElementsPerGroup and BytesPerElement

  if (strstr(extensions, "GL_EXT_packed_depth_stencil") ||
      strstr(extensions, "GL_OES_packed_depth_stencil")) {
    AddExtensionString("GL_OES_packed_depth_stencil");
    if (enable_depth_texture) {
      validators_.texture_internal_format.AddValue(GL_DEPTH_STENCIL);
      validators_.texture_format.AddValue(GL_DEPTH_STENCIL);
      validators_.pixel_type.AddValue(GL_UNSIGNED_INT_24_8);
    }
    validators_.render_buffer_format.AddValue(GL_DEPTH24_STENCIL8);
  }

  bool enable_texture_format_bgra8888 = false;
  bool enable_read_format_bgra = false;
  // Check if we should allow GL_EXT_texture_format_BGRA8888
  if (strstr(extensions, "GL_EXT_texture_format_BGRA8888") ||
      strstr(extensions, "GL_APPLE_texture_format_BGRA8888")) {
    enable_texture_format_bgra8888 = true;
  }

  if (strstr(extensions, "GL_EXT_bgra")) {
    enable_texture_format_bgra8888 = true;
    enable_read_format_bgra = true;
  }

  if (strstr(extensions, "GL_EXT_read_format_bgra")) {
    enable_read_format_bgra = true;
  }

  if (enable_texture_format_bgra8888) {
    AddExtensionString("GL_EXT_texture_format_BGRA8888");
    validators_.texture_internal_format.AddValue(GL_BGRA_EXT);
    validators_.texture_format.AddValue(GL_BGRA_EXT);
  }

  if (enable_read_format_bgra) {
    AddExtensionString("GL_EXT_read_format_bgra");
    validators_.read_pixel_format.AddValue(GL_BGRA_EXT);
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

  // Check for multisample support
  if (strstr(extensions, "GL_EXT_framebuffer_multisample")) {
    extension_flags_.ext_framebuffer_multisample = true;
    validators_.frame_buffer_target.AddValue(GL_READ_FRAMEBUFFER_EXT);
    validators_.frame_buffer_target.AddValue(GL_DRAW_FRAMEBUFFER_EXT);
    validators_.g_l_state.AddValue(GL_READ_FRAMEBUFFER_BINDING_EXT);
    validators_.render_buffer_parameter.AddValue(GL_MAX_SAMPLES_EXT);
    AddExtensionString("GL_EXT_framebuffer_multisample");
    AddExtensionString("GL_EXT_framebuffer_blit");
  }

  if (strstr(extensions, "GL_OES_depth24") ||
      gfx::GetGLImplementation() == gfx::kGLImplementationDesktopGL) {
    AddExtensionString("GL_OES_depth24");
    validators_.render_buffer_format.AddValue(GL_DEPTH_COMPONENT24);
  }

  if (strstr(extensions, "GL_OES_standard_derivatives") ||
      gfx::GetGLImplementation() == gfx::kGLImplementationDesktopGL) {
    AddExtensionString("GL_OES_standard_derivatives");
    extension_flags_.oes_standard_derivatives = true;
  }

  // TODO(gman): Add support for these extensions.
  //     GL_OES_depth32
  //     GL_OES_element_index_uint

  buffer_manager_.reset(new BufferManager());
  framebuffer_manager_.reset(new FramebufferManager());
  renderbuffer_manager_.reset(new RenderbufferManager());
  shader_manager_.reset(new ShaderManager());
  program_manager_.reset(new ProgramManager());

  // Lookup GL things we need to know.
  GetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_vertex_attribs_);
  const GLuint kGLES2RequiredMinimumVertexAttribs = 8u;
  if (max_vertex_attribs_ < kGLES2RequiredMinimumVertexAttribs) {
    LOG(ERROR) << "ContextGroup::Initialize failed because too few "
               << "vertex attributes supported.";
    return false;
  }

  GetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &max_texture_units_);
  const GLuint kGLES2RequiredMinimumTextureUnits = 8u;
  if (max_texture_units_ < kGLES2RequiredMinimumTextureUnits) {
    LOG(ERROR) << "ContextGroup::Initialize failed because too few "
               << "texture units supported.";
    return false;
  }

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

  if (gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2) {
    GetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS,
        &max_fragment_uniform_vectors_);
    GetIntegerv(GL_MAX_VARYING_VECTORS, &max_varying_vectors_);
    GetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &max_vertex_uniform_vectors_);
  } else {
    GetIntegerv(
        GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &max_fragment_uniform_vectors_);
    max_fragment_uniform_vectors_ /= 4;
    GetIntegerv(GL_MAX_VARYING_FLOATS, &max_varying_vectors_);
    max_varying_vectors_ /= 4;
    GetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &max_vertex_uniform_vectors_);
    max_vertex_uniform_vectors_ /= 4;
  }

  if (!texture_manager_->Initialize()) {
    LOG(ERROR) << "Context::Group::Initialize failed because texture manager "
               << "failed to initialize.";
    return false;
  }

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


