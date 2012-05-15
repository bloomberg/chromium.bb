// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/context_group.h"

#include <string>

#include "base/command_line.h"
#include "base/string_util.h"
#include "gpu/command_buffer/common/id_allocator.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/renderbuffer_manager.h"
#include "gpu/command_buffer/service/shader_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_implementation.h"

namespace gpu {
namespace gles2 {

ContextGroup::ContextGroup(MailboxManager* mailbox_manager,
                           bool bind_generates_resource)
    : mailbox_manager_(mailbox_manager ? mailbox_manager : new MailboxManager),
      num_contexts_(0),
      enforce_gl_minimums_(CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnforceGLMinimums)),
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
  id_namespaces_[id_namespaces::kQueries].reset(new IdAllocator);
}

ContextGroup::~ContextGroup() {
  CHECK(num_contexts_ == 0);
}

static void GetIntegerv(GLenum pname, uint32* var) {
  GLint value = 0;
  glGetIntegerv(pname, &value);
  *var = value;
}

bool ContextGroup::CheckGLFeature(GLint min_required, GLint* v) {
  GLint value = *v;
  if (enforce_gl_minimums_) {
    value = std::min(min_required, value);
  }
  *v = value;
  return value >= min_required;
}

bool ContextGroup::CheckGLFeatureU(GLint min_required, uint32* v) {
  GLint value = *v;
  if (enforce_gl_minimums_) {
    value = std::min(min_required, value);
  }
  *v = value;
  return value >= min_required;
}

bool ContextGroup::QueryGLFeature(
    GLenum pname, GLint min_required, GLint* v) {
  GLint value = 0;
  glGetIntegerv(pname, &value);
  *v = value;
  return CheckGLFeature(min_required, v);
}

bool ContextGroup::QueryGLFeatureU(
    GLenum pname, GLint min_required, uint32* v) {
  uint32 value = 0;
  GetIntegerv(pname, &value);
  bool result = CheckGLFeatureU(min_required, &value);
  *v = value;
  return result;
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

  const GLint kMinRenderbufferSize = 512;  // GL says 1 pixel!
  GLint max_renderbuffer_size = 0;
  if (!QueryGLFeature(
      GL_MAX_RENDERBUFFER_SIZE, kMinRenderbufferSize,
      &max_renderbuffer_size)) {
    LOG(ERROR) << "ContextGroup::Initialize failed because maximum "
               << "renderbuffer size too small.";
    return false;
  }
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
  const GLint kGLES2RequiredMinimumVertexAttribs = 8u;
  if (!QueryGLFeatureU(
      GL_MAX_VERTEX_ATTRIBS, kGLES2RequiredMinimumVertexAttribs,
      &max_vertex_attribs_)) {
    LOG(ERROR) << "ContextGroup::Initialize failed because too few "
               << "vertex attributes supported.";
    return false;
  }

  const GLuint kGLES2RequiredMinimumTextureUnits = 8u;
  if (!QueryGLFeatureU(
      GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, kGLES2RequiredMinimumTextureUnits,
      &max_texture_units_)) {
    LOG(ERROR) << "ContextGroup::Initialize failed because too few "
               << "texture units supported.";
    return false;
  }

  GLint max_texture_size = 0;
  GLint max_cube_map_texture_size = 0;
  const GLint kMinTextureSize = 2048;  // GL actually says 64!?!?
  const GLint kMinCubeMapSize = 256;  // GL actually says 16!?!?
  if (!QueryGLFeature(
      GL_MAX_TEXTURE_SIZE, kMinTextureSize, &max_texture_size) ||
      !QueryGLFeature(
      GL_MAX_CUBE_MAP_TEXTURE_SIZE, kMinCubeMapSize,
      &max_cube_map_texture_size)) {
    LOG(ERROR) << "ContextGroup::Initialize failed because maximum texture size"
               << "is too small.";
    return false;
  }

  // Limit Intel on Mac to 512.
  // TODO(gman): Update this code to check for a specific version of
  // the drivers above which we no longer need this fix.
#if defined(OS_MACOSX)
  if (feature_info_->feature_flags().is_intel) {
    max_texture_size = std::min(
        static_cast<GLint>(4096), max_texture_size);
    max_cube_map_texture_size = std::min(
        static_cast<GLint>(512), max_cube_map_texture_size);
  }
#endif
  texture_manager_.reset(new TextureManager(feature_info_.get(),
                                            max_texture_size,
                                            max_cube_map_texture_size));

  const GLint kMinTextureImageUnits = 8;
  const GLint kMinVertexTextureImageUnits = 0;
  if (!QueryGLFeatureU(
      GL_MAX_TEXTURE_IMAGE_UNITS, kMinTextureImageUnits,
      &max_texture_image_units_) ||
      !QueryGLFeatureU(
      GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, kMinVertexTextureImageUnits,
      &max_vertex_texture_image_units_)) {
    LOG(ERROR) << "ContextGroup::Initialize failed because too few "
               << "texture units.";
    return false;
  }

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

  const GLint kMinFragmentUniformVectors = 16;
  const GLint kMinVaryingVectors = 8;
  const GLint kMinVertexUniformVectors = 128;
  if (!CheckGLFeatureU(
      kMinFragmentUniformVectors, &max_fragment_uniform_vectors_) ||
      !CheckGLFeatureU(kMinVaryingVectors, &max_varying_vectors_) ||
      !CheckGLFeatureU(
      kMinVertexUniformVectors, &max_vertex_uniform_vectors_)) {
    LOG(ERROR) << "ContextGroup::Initialize failed because too few "
               << "uniforms or varyings supported.";
    return false;
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
    mailbox_manager_->DestroyOwnedTextures(texture_manager_.get(),
                                           have_context);
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


