// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/context_group.h"

#include <string>

#include "base/string_util.h"
#include "gpu/command_buffer/common/id_allocator.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/renderbuffer_manager.h"
#include "gpu/command_buffer/service/shader_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gfx/gl/gl_implementation.h"

namespace gpu {
namespace gles2 {

ContextGroup::ContextGroup(bool bind_generates_resource)
    : num_contexts_(0),
      bind_generates_resource_(bind_generates_resource),
      max_vertex_attribs_(0u),
      max_texture_units_(0u),
      max_texture_image_units_(0u),
      max_vertex_texture_image_units_(0u),
      max_fragment_uniform_vectors_(0u),
      max_varying_vectors_(0u),
      max_vertex_uniform_vectors_(0u),
      feature_info_(new FeatureInfo()) {
  id_namespaces_[id_namespaces::kBuffers].reset(new IdAllocator);
  id_namespaces_[id_namespaces::kFramebuffers].reset(new IdAllocator);
  id_namespaces_[id_namespaces::kProgramsAndShaders].reset(
      new NonReusedIdAllocator);
  id_namespaces_[id_namespaces::kRenderbuffers].reset(new IdAllocator);
  id_namespaces_[id_namespaces::kTextures].reset(new IdAllocator);
}

ContextGroup::~ContextGroup() {
  CHECK(num_contexts_ == 0);
}

static void GetIntegerv(GLenum pname, uint32* var) {
  GLint value = 0;
  glGetIntegerv(pname, &value);
  *var = value;
}

bool ContextGroup::Initialize(const DisallowedFeatures& disallowed_features,
                              const char* allowed_features) {
  if (num_contexts_ > 0) {
    ++num_contexts_;
    return true;
  }

  if (!feature_info_->Initialize(disallowed_features, allowed_features)) {
    LOG(ERROR) << "ContextGroup::Initialize failed because FeatureInfo "
               << "initialization failed.";
    return false;
  }

  GLint max_renderbuffer_size = 0;
  glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &max_renderbuffer_size);
  GLint max_samples = 0;
  if (feature_info_->feature_flags().chromium_framebuffer_multisample) {
    glGetIntegerv(GL_MAX_SAMPLES, &max_samples);
  }

  buffer_manager_.reset(new BufferManager());
  framebuffer_manager_.reset(new FramebufferManager());
  renderbuffer_manager_.reset(new RenderbufferManager(
      max_renderbuffer_size, max_samples));
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

  GLint max_texture_size = 0;
  GLint max_cube_map_texture_size = 0;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
  glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &max_cube_map_texture_size);

  // Limit Intel on Mac to 512.
  // TODO(gman): Update this code to check for a specific version of
  // the drivers above which we no longer need this fix.
#if defined(OS_MACOSX)
  const char* vendor_str = reinterpret_cast<const char*>(
      glGetString(GL_VENDOR));
  if (vendor_str) {
    std::string lc_str(::StringToLowerASCII(std::string(vendor_str)));
    bool intel_on_mac = strstr(lc_str.c_str(), "intel");
    if (intel_on_mac) {
      max_cube_map_texture_size = std::min(
        static_cast<GLint>(512), max_cube_map_texture_size);
    }
  }
#endif

  texture_manager_.reset(new TextureManager(feature_info_.get(),
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

  ++num_contexts_;
  return true;
}

void ContextGroup::Destroy(bool have_context) {
  DCHECK(num_contexts_ > 0);
  if (--num_contexts_ > 0)
    return;

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

IdAllocatorInterface* ContextGroup::GetIdAllocator(unsigned namespace_id) {
  if (namespace_id >= arraysize(id_namespaces_))
    return NULL;

  return id_namespaces_[namespace_id].get();
}

}  // namespace gles2
}  // namespace gpu


