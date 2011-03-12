// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_CONTEXT_GROUP_H_
#define GPU_COMMAND_BUFFER_SERVICE_CONTEXT_GROUP_H_

#include <map>
#include <string>
#include "base/basictypes.h"
#include "base/linked_ptr.h"
#include "base/scoped_ptr.h"
#include "base/ref_counted.h"
#include "gpu/command_buffer/service/gles2_cmd_validation.h"
#include "gpu/command_buffer/service/feature_info.h"

namespace gpu {

class IdAllocator;

namespace gles2 {

class GLES2Decoder;
class BufferManager;
class FramebufferManager;
class RenderbufferManager;
class ProgramManager;
class ShaderManager;
class TextureManager;
struct DisallowedExtensions;

// A Context Group helps manage multiple GLES2Decoders that share
// resources.
class ContextGroup : public base::RefCounted<ContextGroup> {
 public:
  typedef scoped_refptr<ContextGroup> Ref;

  ContextGroup();
  ~ContextGroup();

  // This should only be called by GLES2Decoder.
  bool Initialize(const DisallowedExtensions& disallowed_extensions,
                  const char* allowed_features);

  // Sets the ContextGroup has having a lost context.
  void set_have_context(bool have_context) {
    have_context_ = have_context;
  }

  uint32 max_vertex_attribs() const {
    return max_vertex_attribs_;
  }

  uint32 max_texture_units() const {
    return max_texture_units_;
  }

  uint32 max_texture_image_units() const {
    return max_texture_image_units_;
  }

  uint32 max_vertex_texture_image_units() const {
    return max_vertex_texture_image_units_;
  }

  uint32 max_fragment_uniform_vectors() const {
    return max_fragment_uniform_vectors_;
  }

  uint32 max_varying_vectors() const {
    return max_varying_vectors_;
  }

  uint32 max_vertex_uniform_vectors() const {
    return max_vertex_uniform_vectors_;
  }

  FeatureInfo* feature_info() {
    return &feature_info_;
  }

  BufferManager* buffer_manager() const {
    return buffer_manager_.get();
  }

  FramebufferManager* framebuffer_manager() const {
    return framebuffer_manager_.get();
  }

  RenderbufferManager* renderbuffer_manager() const {
    return renderbuffer_manager_.get();
  }

  TextureManager* texture_manager() const {
    return texture_manager_.get();
  }

  ProgramManager* program_manager() const {
    return program_manager_.get();
  }

  ShaderManager* shader_manager() const {
    return shader_manager_.get();
  }

  IdAllocator* GetIdAllocator(unsigned namepsace_id);

 private:
  // Destroys all the resources.
  void Destroy();

  // Whether or not this context is initialized.
  bool initialized_;
  bool have_context_;

  uint32 max_vertex_attribs_;
  uint32 max_texture_units_;
  uint32 max_texture_image_units_;
  uint32 max_vertex_texture_image_units_;
  uint32 max_fragment_uniform_vectors_;
  uint32 max_varying_vectors_;
  uint32 max_vertex_uniform_vectors_;

  scoped_ptr<BufferManager> buffer_manager_;

  scoped_ptr<FramebufferManager> framebuffer_manager_;

  scoped_ptr<RenderbufferManager> renderbuffer_manager_;

  scoped_ptr<TextureManager> texture_manager_;

  scoped_ptr<ProgramManager> program_manager_;

  scoped_ptr<ShaderManager> shader_manager_;

  typedef std::map<uint32, linked_ptr<IdAllocator> > IdAllocatorMap;
  IdAllocatorMap id_namespaces_;

  FeatureInfo feature_info_;

  DISALLOW_COPY_AND_ASSIGN(ContextGroup);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_CONTEXT_GROUP_H_


