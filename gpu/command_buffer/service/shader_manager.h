// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHADER_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHADER_MANAGER_H_

#include <map>
#include <string>
#include "base/basictypes.h"
#include "gpu/command_buffer/service/gl_utils.h"

namespace gpu {
namespace gles2 {

// Tracks the Shaders.
//
// NOTE: To support shared resources an instance of this class will
// need to be shared by multiple GLES2Decoders.
class ShaderManager {
 public:
  // This is used to keep the source code for a shader. This is because in order
  // to emluate GLES2 the shaders will have to be re-written before passed to
  // the underlying OpenGL. But, when the user calls glGetShaderSource they
  // should get the source they passed in, not the re-written source.
  class ShaderInfo {
   public:
    explicit ShaderInfo(GLuint shader)
        : shader_(shader) {
    }

    void Update(const std::string& source) {
      source_ = source;
    }

    const std::string& source() {
      return source_;
    }

   private:
    // The shader this ShaderInfo is tracking.
    GLuint shader_;

    // The shader source as passed to glShaderSource.
    std::string source_;
  };

  ShaderManager() { };

  // Creates a shader info for the given shader ID.
  void CreateShaderInfo(GLuint shader);

  // Gets an existing shader info for the given shader ID. Returns NULL if none
  // exists.
  ShaderInfo* GetShaderInfo(GLuint shader);

  // Deletes the shader info for the given shader.
  void RemoveShaderInfo(GLuint shader);

 private:
  // Info for each shader by service side shader Id.
  typedef std::map<GLuint, ShaderInfo> ShaderInfoMap;
  ShaderInfoMap shader_infos_;

  DISALLOW_COPY_AND_ASSIGN(ShaderManager);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHADER_MANAGER_H_

