// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "media/tools/shader_bench/gpu_painter.h"

// Vertices for a full screen quad.
static const float kVertices[8] = {
  -1.f, 1.f,
  -1.f, -1.f,
  1.f, 1.f,
  1.f, -1.f,
};

// Texture Coordinates mapping the entire texture.
static const float kTextureCoords[8] = {
  0, 0,
  0, 1,
  1, 0,
  1, 1,
};

// Buffer size for compile errors.
static const unsigned int kErrorSize = 4096;

GPUPainter::GPUPainter()
    : surface_(NULL),
      context_(NULL) {
}

GPUPainter::~GPUPainter() {
}

void GPUPainter::SetGLContext(gfx::GLSurface* surface,
                              gfx::GLContext* context) {
  surface_ = surface;
  context_ = context;
}

GLuint GPUPainter::LoadShader(unsigned type, const char* shader_source) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &shader_source, NULL);
  glCompileShader(shader);
  int result = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
  if (!result) {
    char log[kErrorSize];
    int len;
    glGetShaderInfoLog(shader, kErrorSize - 1, &len, log);
    log[kErrorSize - 1] = 0;
    LOG(FATAL) << "Shader did not compile: " << log;
  }
  return shader;
}

GLuint GPUPainter::CreateShaderProgram(const char* vertex_shader_source,
                                       const char* fragment_shader_source) {

  // Create vertex and pixel shaders.
  GLuint vertex_shader = LoadShader(GL_VERTEX_SHADER, vertex_shader_source);
  GLuint fragment_shader =
      LoadShader(GL_FRAGMENT_SHADER, fragment_shader_source);

  // Create program and attach shaders.
  GLuint program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  glLinkProgram(program);
  int result = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &result);
  if (!result) {
    char log[kErrorSize];
    int len;
    glGetProgramInfoLog(program, kErrorSize - 1, &len, log);
    log[kErrorSize - 1] = 0;
    LOG(FATAL) << "Program did not link: " << log;
  }
  glUseProgram(program);

  // Set common vertex parameters.
  int pos_location = glGetAttribLocation(program, "in_pos");
  glEnableVertexAttribArray(pos_location);
  glVertexAttribPointer(pos_location, 2, GL_FLOAT, GL_FALSE, 0, kVertices);

  int tc_location = glGetAttribLocation(program, "in_tc");
  glEnableVertexAttribArray(tc_location);
  glVertexAttribPointer(tc_location, 2, GL_FLOAT, GL_FALSE, 0, kTextureCoords);
  return program;
}
