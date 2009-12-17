// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is here so other GLES2 related files can have a common set of
// includes where appropriate.

#include <GLES2/gl2.h>
#include "gpu/command_buffer/client/gles2_demo_cc.h"

namespace {

int g_width = 512;
int g_height = 512;
GLuint g_texture = 0;
int g_textureLoc = -1;
GLuint g_programObject = 0;
GLuint g_vbo = 0;
GLsizei g_texCoordOffset = 0;

void CheckGLError() {
  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    DLOG(ERROR) << "GL Error: " << error;
  }
}

GLuint LoadShader(GLenum type, const char* shaderSrc) {
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
    GLsizei length;
    glGetShaderInfoLog(shader, sizeof(buffer), &length, buffer);
    std::string log(buffer, length);
    DLOG(ERROR) << "Error compiling shader:" << log;
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

void InitShaders() {
  static const char* vShaderStr =
    "attribute vec3 g_Position;\n"
    "attribute vec2 g_TexCoord0;\n"
    "varying vec2 texCoord;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = vec4(g_Position.x, g_Position.y, g_Position.z, 1.0);\n"
    "   texCoord = g_TexCoord0;\n"
    "}\n";
  static const char* fShaderStr =
    "uniform sampler2D tex;\n"
    "varying vec2 texCoord;\n"
    "void main()\n"
    "{\n"
    "  gl_FragColor = texture2D(tex, texCoord);\n"
    "}\n";

  GLuint vertexShader = LoadShader(GL_VERTEX_SHADER, vShaderStr);
  GLuint fragmentShader = LoadShader(GL_FRAGMENT_SHADER, fShaderStr);
  // Create the program object
  GLuint programObject = glCreateProgram();
  if (programObject == 0) {
    DLOG(ERROR) << "Creating program failed";
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
    GLsizei length;
    glGetProgramInfoLog(programObject, sizeof(buffer), &length, buffer);
    std::string log(buffer, length);
    DLOG(ERROR) << "Error linking program:" << log;
    glDeleteProgram(programObject);
    return;
  }
  g_programObject = programObject;
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
  CheckGLError();
}

void Draw() {
  // Note: the viewport is automatically set up to cover the entire Canvas.
  // Clear the color buffer
  glClear(GL_COLOR_BUFFER_BIT);
  CheckGLError();
  // Use the program object
  glUseProgram(g_programObject);
  CheckGLError();
  // Load the vertex data
  glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0,
                        reinterpret_cast<const void*>(g_texCoordOffset));
  CheckGLError();
  // Bind the texture to texture unit 0
  glBindTexture(GL_TEXTURE_2D, g_texture);
  CheckGLError();
  // Point the uniform sampler to texture unit 0
  glUniform1i(g_textureLoc, 0);
  CheckGLError();
  glDrawArrays(GL_TRIANGLES, 0, 6);
  CheckGLError();
  glFlush();
}

GLuint CreateCheckerboardTexture() {
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
  return texture;
}

void Init() {
  glClearColor(0.f, 0.f, .7f, 1.f);
  g_texture = CreateCheckerboardTexture();
  InitShaders();
}

}  // anonymous namespace.

void GLFromCPPTestFunction() {
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    Init();
  }
  Draw();
}



