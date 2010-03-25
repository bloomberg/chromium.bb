// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_CONTEXT_GROUP_H_
#define GPU_COMMAND_BUFFER_SERVICE_CONTEXT_GROUP_H_

#include <vector>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"

namespace gpu {
namespace gles2 {

class GLES2Decoder;
class BufferManager;
class FramebufferManager;
class RenderbufferManager;
class IdManager;
class ProgramManager;
class ShaderManager;
class TextureManager;

// A Context Group helps manage multiple GLES2Decoders that share
// resources.
class ContextGroup {
 public:
  ContextGroup();
  ~ContextGroup();

  // This should only be called by GLES2Decoder.
  bool Initialize();

  uint32 max_vertex_attribs() const {
    return max_vertex_attribs_;
  }

  uint32 max_texture_units() const {
    return max_texture_units_;
  }

  // Map of client ids to GL ids.
  IdManager* id_manager() const {
    return id_manager_.get();
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

 private:
  // Whether or not this context is initialized.
  bool initialized_;

  uint32 max_vertex_attribs_;

  uint32 max_texture_units_;

  // Map of client ids to GL ids.
  scoped_ptr<IdManager> id_manager_;

  scoped_ptr<BufferManager> buffer_manager_;

  scoped_ptr<FramebufferManager> framebuffer_manager_;

  scoped_ptr<RenderbufferManager> renderbuffer_manager_;

  scoped_ptr<TextureManager> texture_manager_;

  scoped_ptr<ProgramManager> program_manager_;

  scoped_ptr<ShaderManager> shader_manager_;

  DISALLOW_COPY_AND_ASSIGN(ContextGroup);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_CONTEXT_GROUP_H_


