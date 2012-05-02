// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper functions for GL.

#ifndef GPU_COMMAND_BUFFER_TESTS_GL_TEST_UTILS_H_
#define GPU_COMMAND_BUFFER_TESTS_GL_TEST_UTILS_H_

#include <GLES2/gl2.h>

class GLTestHelper {
 public:
  static bool HasExtension(const char* extension);
  static bool CheckGLError(const char* msg, int line);
  static GLuint LoadShader(GLenum type, const char* shaderSrc);
  static GLuint SetupProgram(GLuint vertex_shader, GLuint fragment_shader);
  static GLuint LoadProgram(
      const char* vertex_shader_source,
      const char* fragment_shader_source);
  static bool SaveBackbufferAsBMP(const char* filename, int width, int height);
};

#endif  // GPU_COMMAND_BUFFER_TESTS_GL_TEST_UTILS_H_

