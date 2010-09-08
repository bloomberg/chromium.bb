// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is here so other GLES2 related files can have a common set of
// includes where appropriate.

#include "../client/gles2_demo_cc.h"
#include "../common/logging.h"

#include <math.h>
#include <string>

// This is here so we have at least some idea that the inline path is working.
#define GLES2_INLINE_OPTIMIZATION
#include <GLES2/gl2.h>
#include "../common/logging.h"

namespace {

GLuint g_texture = 0;
int g_textureLoc = -1;
GLuint g_programObject = 0;
GLuint g_worldMatrixLoc = 0;
GLuint g_vbo = 0;
GLsizei g_texCoordOffset = 0;
int g_angle = 0;

void CheckGLError(const char* func_name, int line_no) {
#ifndef NDEBUG
  GLenum error = GL_NO_ERROR;
  while ((error = glGetError()) != GL_NO_ERROR) {
    GPU_DLOG(gpu::ERROR) << "GL Error in " << func_name << " at line "
        << line_no << ": " << error;
  }
#endif
}

GLuint LoadShader(GLenum type, const char* shaderSrc) {
  CheckGLError("LoadShader", __LINE__);
  GLuint shader = glCreateShader(type);
  if (shader == 0) {
    return 0;
  }
  // Load the shader source
  glShaderSource(shader, 1, &shaderSrc, NULL);
  // Compile the shader
  glCompileShader(shader);
  // Check the compile status
  GLint value;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &value);
  if (value == 0) {
    char buffer[1024];
    GLsizei length = 0;
    glGetShaderInfoLog(shader, sizeof(buffer), &length, buffer);
    std::string log(buffer, length);
    GPU_DLOG(gpu::ERROR) << "Error compiling shader:" << log;
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

void InitShaders() {
  static const char* vShaderStr =
    "uniform mat4 worldMatrix;\n"
    "attribute vec3 g_Position;\n"
    "attribute vec2 g_TexCoord0;\n"
    "varying vec2 texCoord;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = worldMatrix *\n"
    "                 vec4(g_Position.x, g_Position.y, g_Position.z, 1.0);\n"
    "   texCoord = g_TexCoord0;\n"
    "}\n";
  static const char* fShaderStr =
    "precision mediump float;"
    "uniform sampler2D tex;\n"
    "varying vec2 texCoord;\n"
    "void main()\n"
    "{\n"
    "  gl_FragColor = texture2D(tex, texCoord);\n"
    "}\n";

  CheckGLError("InitShaders", __LINE__);
  GLuint vertexShader = LoadShader(GL_VERTEX_SHADER, vShaderStr);
  GLuint fragmentShader = LoadShader(GL_FRAGMENT_SHADER, fShaderStr);
  // Create the program object
  GLuint programObject = glCreateProgram();
  if (programObject == 0) {
    GPU_DLOG(gpu::ERROR) << "Creating program failed";
    return;
  }
  glAttachShader(programObject, vertexShader);
  glAttachShader(programObject, fragmentShader);
  // Bind g_Position to attribute 0
  // Bind g_TexCoord0 to attribute 1
  glBindAttribLocation(programObject, 0, "g_Position");
  glBindAttribLocation(programObject, 1, "g_TexCoord0");
  // Link the program
  glLinkProgram(programObject);
  // Check the link status
  GLint linked;
  glGetProgramiv(programObject, GL_LINK_STATUS, &linked);
  if (linked == 0) {
    char buffer[1024];
    GLsizei length = 0;
    glGetProgramInfoLog(programObject, sizeof(buffer), &length, buffer);
    std::string log(buffer, length);
    GPU_DLOG(gpu::ERROR) << "Error linking program:" << log;
    glDeleteProgram(programObject);
    return;
  }
  g_programObject = programObject;
  g_worldMatrixLoc = glGetUniformLocation(g_programObject, "worldMatrix");
  g_textureLoc = glGetUniformLocation(g_programObject, "tex");
  glGenBuffers(1, &g_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
  static float vertices[] = {
      0.25,  0.75, 0.0,
     -0.75,  0.75, 0.0,
     -0.75, -0.25, 0.0,
      0.25,  0.75, 0.0,
     -0.75, -0.25, 0.0,
      0.25, -0.25, 0.0,
  };
  static float texCoords[] = {
    1.0, 1.0,
    0.0, 1.0,
    0.0, 0.0,
    1.0, 1.0,
    0.0, 0.0,
    1.0, 0.0,
  };
  g_texCoordOffset = sizeof(vertices);
  glBufferData(GL_ARRAY_BUFFER,
               sizeof(vertices) + sizeof(texCoords),
               NULL,
               GL_STATIC_DRAW);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
  glBufferSubData(GL_ARRAY_BUFFER, g_texCoordOffset,
                  sizeof(texCoords), texCoords);
  CheckGLError("InitShaders", __LINE__);
}

GLuint CreateCheckerboardTexture() {
  CheckGLError("CreateCheckerboardTexture", __LINE__);
  static unsigned char pixels[] = {
    255, 255, 255,
    0,   0,   0,
    0,   0,   0,
    255, 255, 255,
  };
  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE,
               pixels);
  CheckGLError("CreateCheckerboardTexture", __LINE__);
  return texture;
}

}  // anonymous namespace.

void GLFromCPPInit() {
  CheckGLError("GLFromCPPInit", __LINE__);
  glClearColor(0.f, 0.f, .7f, 1.f);
  g_texture = CreateCheckerboardTexture();
  InitShaders();
  CheckGLError("GLFromCPPInit", __LINE__);
}

void GLFromCPPDraw() {
  const float kPi = 3.1415926535897932384626433832795f;

  CheckGLError("GLFromCPPDraw", __LINE__);
  // TODO(kbr): base the angle on time rather than on ticks
  g_angle = (g_angle + 1) % 360;
  // Rotate about the Z axis
  GLfloat rot_matrix[16];
  GLfloat cos_angle = cosf(static_cast<GLfloat>(g_angle) * kPi / 180.0f);
  GLfloat sin_angle = sinf(static_cast<GLfloat>(g_angle) * kPi / 180.0f);
  // OpenGL matrices are column-major
  rot_matrix[0] = cos_angle;
  rot_matrix[1] = sin_angle;
  rot_matrix[2] = 0.0f;
  rot_matrix[3] = 0.0f;

  rot_matrix[4] = -sin_angle;
  rot_matrix[5] = cos_angle;
  rot_matrix[6] = 0.0f;
  rot_matrix[7] = 0.0f;

  rot_matrix[8] = 0.0f;
  rot_matrix[9] = 0.0f;
  rot_matrix[10] = 1.0f;
  rot_matrix[11] = 0.0f;

  rot_matrix[12] = 0.0f;
  rot_matrix[13] = 0.0f;
  rot_matrix[14] = 0.0f;
  rot_matrix[15] = 1.0f;

  // Note: the viewport is automatically set up to cover the entire Canvas.
  // Clear the color buffer
  glClear(GL_COLOR_BUFFER_BIT);
  CheckGLError("GLFromCPPDraw", __LINE__);
  // Use the program object
  glUseProgram(g_programObject);
  CheckGLError("GLFromCPPDraw", __LINE__);
  // Set up the model matrix
  glUniformMatrix4fv(g_worldMatrixLoc, 1, GL_FALSE, rot_matrix);

  // Load the vertex data
  glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0,
                        reinterpret_cast<const void*>(g_texCoordOffset));
  CheckGLError("GLFromCPPDraw", __LINE__);
  // Bind the texture to texture unit 0
  glBindTexture(GL_TEXTURE_2D, g_texture);
  CheckGLError("GLFromCPPDraw", __LINE__);
  // Point the uniform sampler to texture unit 0
  glUniform1i(g_textureLoc, 0);
  CheckGLError("GLFromCPPDraw", __LINE__);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  CheckGLError("GLFromCPPDraw", __LINE__);
  glFlush();
}
