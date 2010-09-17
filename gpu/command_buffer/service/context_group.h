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
#include "gpu/command_buffer/service/gles2_cmd_validation.h"

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

// A Context Group helps manage multiple GLES2Decoders that share
// resources.
class ContextGroup {
 public:
  struct ExtensionFlags {
    ExtensionFlags()
        : ext_framebuffer_multisample(false),
          oes_standard_derivatives(false) {
    }

    bool ext_framebuffer_multisample;
    bool oes_standard_derivatives;
  };

  ContextGroup();
  ~ContextGroup();

  // This should only be called by GLES2Decoder.
  bool Initialize();

  // Destroys all the resources. MUST be called before destruction.
  void Destroy(bool have_context);

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

  const Validators* validators() const {
    return &validators_;
  }

  const std::string& extensions() const {
    return extensions_;
  }

  const ExtensionFlags& extension_flags() const {
    return extension_flags_;
  }

 private:
  void AddExtensionString(const std::string& str);

  // Whether or not this context is initialized.
  bool initialized_;

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

  Validators validators_;

  // The extensions string returned by glGetString(GL_EXTENSIONS);
  std::string extensions_;

  // Flags for some extensions
  ExtensionFlags extension_flags_;

  DISALLOW_COPY_AND_ASSIGN(ContextGroup);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_CONTEXT_GROUP_H_


