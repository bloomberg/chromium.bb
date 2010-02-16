// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_PROGRAM_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_PROGRAM_MANAGER_H_

#include <map>
#include <string>
#include <vector>
#include "base/basictypes.h"
#include "base/logging.h"
#include "gpu/command_buffer/service/gl_utils.h"

namespace gpu {
namespace gles2 {

// Tracks the Programs.
//
// NOTE: To support shared resources an instance of this class will
// need to be shared by multiple GLES2Decoders.
class ProgramManager {
 public:
  // This is used to track which attributes a particular program needs
  // so we can verify at glDrawXXX time that every attribute is either disabled
  // or if enabled that it points to a valid source.
  class ProgramInfo {
   public:
    struct UniformInfo {
      GLsizei size;
      GLenum type;
      std::string name;
      std::vector<GLint> element_locations;
    };
    struct VertexAttribInfo {
      GLsizei size;
      GLenum type;
      GLint location;
      std::string name;
    };

    typedef std::vector<UniformInfo> UniformInfoVector;
    typedef std::vector<VertexAttribInfo> AttribInfoVector;

    explicit ProgramInfo(GLuint program)
        : program_(program) {
    }

    void Update();

    const AttribInfoVector& GetAttribInfos() const {
      return attrib_infos_;
    }

    const VertexAttribInfo* GetAttribInfo(GLint index) const {
      return (static_cast<size_t>(index) < attrib_infos_.size()) ?
         &attrib_infos_[index] : NULL;
    }

    GLint GetAttribLocation(const std::string& name) const;

    const UniformInfo* GetUniformInfo(GLint index) const {
      return (static_cast<size_t>(index) < uniform_infos_.size()) ?
         &uniform_infos_[index] : NULL;
    }

    GLint GetUniformLocation(const std::string& name);

    bool GetUniformTypeByLocation(GLint location, GLenum* type) const;

   private:
    void SetNumAttributes(int num_attribs) {
      attrib_infos_.resize(num_attribs);
    }

    void SetAttributeInfo(
        GLint index, GLsizei size, GLenum type, GLint location,
        const std::string& name) {
      DCHECK(static_cast<unsigned>(index) < attrib_infos_.size());
      VertexAttribInfo& info = attrib_infos_[index];
      info.size = size;
      info.type = type;
      info.name = name;
      info.location = location;
    }

    void SetNumUniforms(int num_uniforms) {
      uniform_infos_.resize(num_uniforms);
    }

    void SetUniformInfo(
        GLint index, GLsizei size, GLenum type, GLint location,
        const std::string& name);

    AttribInfoVector attrib_infos_;

    // Uniform info by info.
    UniformInfoVector uniform_infos_;

    // The program this ProgramInfo is tracking.
    GLuint program_;
  };

  ProgramManager() { };

  // Creates a new program info.
  void CreateProgramInfo(GLuint program);

  // Gets a program info
  ProgramInfo* GetProgramInfo(GLuint program);

  // Deletes the program info for the given program.
  void RemoveProgramInfo(GLuint program);

  // Returns true if prefix is invalid for gl.
  static bool IsInvalidPrefix(const char* name, size_t length);

 private:
  // Info for each "successfully linked" program by service side program Id.
  // TODO(gman): Choose a faster container.
  typedef std::map<GLuint, ProgramInfo> ProgramInfoMap;
  ProgramInfoMap program_infos_;

  DISALLOW_COPY_AND_ASSIGN(ProgramManager);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_PROGRAM_MANAGER_H_

