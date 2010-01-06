// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/demos/app_framework/gles2_utils.h"

namespace {
static const int kInfoBufferLength = 1024;
}  // namespace

namespace gpu_demos {
namespace gles2_utils {

GLuint LoadShader(GLenum type, const char* shader_src) {
  GLuint shader = glCreateShader(type);
  if (shader == 0) return 0;

  // Load the shader source
  glShaderSource(shader, 1, &shader_src, NULL);
  // Compile the shader
  glCompileShader(shader);
  // Check the compile status
  GLint value;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &value);
  if (value == 0) {
    char buffer[kInfoBufferLength];
    GLsizei length;
    glGetShaderInfoLog(shader, sizeof(buffer), &length, buffer);
    std::string log(buffer, length);
    DLOG(ERROR) << "Error compiling shader:" << log;
    glDeleteShader(shader);
    shader = 0;
  }
  return shader;
}

GLuint LoadProgram(const char* v_shader_src, const char* f_shader_src) {
  GLuint v_shader = LoadShader(GL_VERTEX_SHADER, v_shader_src);
  if (v_shader == 0) return 0;

  GLuint f_shader = LoadShader(GL_FRAGMENT_SHADER, f_shader_src);
  if (f_shader == 0) return 0;

  // Create the program object
  GLuint program_object = glCreateProgram();
  if (program_object == 0) return 0;

  // Link the program and check status.
  glAttachShader(program_object, v_shader);
  glAttachShader(program_object, f_shader);
  glLinkProgram(program_object);
  GLint linked = 0;
  glGetProgramiv(program_object, GL_LINK_STATUS, &linked);
  if (linked == 0) {
    char buffer[kInfoBufferLength];
    GLsizei length;
    glGetProgramInfoLog(program_object, sizeof(buffer), &length, buffer);
    std::string log(buffer, length);
    DLOG(ERROR) << "Error linking program:" << log;
    glDeleteProgram(program_object);
    program_object = 0;
  }
  return program_object;
}

}  // namespace gles2_utils
}  // namespace gpu_demos
