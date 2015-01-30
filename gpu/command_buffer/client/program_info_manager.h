// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_PROGRAM_INFO_MANAGER_H_
#define GPU_COMMAND_BUFFER_CLIENT_PROGRAM_INFO_MANAGER_H_

#include <GLES2/gl2.h>

#include <string>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/synchronization/lock.h"
#include "gles2_impl_export.h"
#include "gpu/command_buffer/client/gles2_implementation.h"

namespace gpu {
namespace gles2 {

// Manages info about OpenGL ES Programs.
class GLES2_IMPL_EXPORT ProgramInfoManager {
 public:
  ProgramInfoManager();
  ~ProgramInfoManager();

  void CreateInfo(GLuint program);

  void DeleteInfo(GLuint program);

  bool GetProgramiv(
      GLES2Implementation* gl, GLuint program, GLenum pname, GLint* params);

  GLint GetAttribLocation(
      GLES2Implementation* gl, GLuint program, const char* name);

  GLint GetUniformLocation(
      GLES2Implementation* gl, GLuint program, const char* name);

  GLint GetFragDataLocation(
      GLES2Implementation* gl, GLuint program, const char* name);

  bool GetActiveAttrib(
      GLES2Implementation* gl, GLuint program, GLuint index, GLsizei bufsize,
      GLsizei* length, GLint* size, GLenum* type, char* name);

  bool GetActiveUniform(
      GLES2Implementation* gl, GLuint program, GLuint index, GLsizei bufsize,
      GLsizei* length, GLint* size, GLenum* type, char* name);

 private:
  class Program {
   public:
    struct UniformInfo {
      UniformInfo(GLsizei _size, GLenum _type, const std::string& _name);
      ~UniformInfo();

      GLsizei size;
      GLenum type;
      bool is_array;
      std::string name;
      std::vector<GLint> element_locations;
    };
    struct VertexAttrib {
      VertexAttrib(GLsizei _size, GLenum _type, const std::string& _name,
                   GLint _location);
      ~VertexAttrib();

      GLsizei size;
      GLenum type;
      GLint location;
      std::string name;
    };

    Program();
    ~Program();

    const VertexAttrib* GetAttribInfo(GLint index) const;

    GLint GetAttribLocation(const std::string& name) const;

    const UniformInfo* GetUniformInfo(GLint index) const;

    // Gets the location of a uniform by name.
    GLint GetUniformLocation(const std::string& name) const;

    GLint GetFragDataLocation(const std::string& name) const;
    void CacheFragDataLocation(const std::string& name, GLint loc);

    bool GetProgramiv(GLenum pname, GLint* params);

    // Updates the program info after a successful link.
    void Update(GLES2Implementation* gl,
                GLuint program,
                const std::vector<int8>& result);

    bool cached() const;

   private:
    bool cached_;

    GLsizei max_attrib_name_length_;

    // Attrib by index.
    std::vector<VertexAttrib> attrib_infos_;

    GLsizei max_uniform_name_length_;

    // Uniform info by index.
    std::vector<UniformInfo> uniform_infos_;

    base::hash_map<std::string, GLint> frag_data_locations_;

    // This is true if glLinkProgram was successful last time it was called.
    bool link_status_;
  };

  Program* GetProgramInfo(GLES2Implementation* gl, GLuint program);

  typedef base::hash_map<GLuint, Program> ProgramInfoMap;

  ProgramInfoMap program_infos_;

  mutable base::Lock lock_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_PROGRAM_INFO_MANAGER_H_
