// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/tests/gl_test_utils.h"
#include <string>
#include <stdio.h>
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

bool GLTestHelper::HasExtension(const char* extension) {
  std::string extensions(
      reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)));
  return extensions.find(extension) != std::string::npos;
}

bool GLTestHelper::CheckGLError(const char* msg, int line) {
   bool success = true;
   GLenum error = GL_NO_ERROR;
   while ((error = glGetError()) != GL_NO_ERROR) {
     success = false;
     EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), error)
         << "GL ERROR in " << msg << " at line " << line << " : " << error;
   }
   return success;
}

GLuint GLTestHelper::LoadShader(GLenum type, const char* shaderSrc) {
  GLuint shader = glCreateShader(type);
  // Load the shader source
  glShaderSource(shader, 1, &shaderSrc, NULL);
  // Compile the shader
  glCompileShader(shader);
  // Check the compile status
  GLint value = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &value);
  if (value == 0) {
    char buffer[1024];
    GLsizei length = 0;
    glGetShaderInfoLog(shader, sizeof(buffer), &length, buffer);
    std::string log(buffer, length);
    EXPECT_EQ(1, value) << "Error compiling shader: " << log;
    glDeleteShader(shader);
    shader = 0;
  }
  return shader;
}

GLuint GLTestHelper::SetupProgram(
    GLuint vertex_shader, GLuint fragment_shader) {
  // Create the program object
  GLuint program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  // Link the program
  glLinkProgram(program);
  // Check the link status
  GLint linked = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  if (linked == 0) {
    char buffer[1024];
    GLsizei length = 0;
    glGetProgramInfoLog(program, sizeof(buffer), &length, buffer);
    std::string log(buffer, length);
    EXPECT_EQ(1, linked) << "Error linking program: " << log;
    glDeleteProgram(program);
    program = 0;
  }
  return program;
}

GLuint GLTestHelper::LoadProgram(
    const char* vertex_shader_source,
    const char* fragment_shader_source) {
  GLuint vertex_shader = LoadShader(
      GL_VERTEX_SHADER, vertex_shader_source);
  GLuint fragment_shader = LoadShader(
      GL_FRAGMENT_SHADER, fragment_shader_source);
  if (!vertex_shader || !fragment_shader) {
    return 0;
  }
  return SetupProgram(vertex_shader, fragment_shader);
}

namespace {

void Set16BitValue(uint8 dest[2], uint16 value) {
  dest[0] = value & 0xFFu;
  dest[1] = value >> 8;
}

void Set32BitValue(uint8 dest[4], uint32 value) {
  dest[0] = (value >> 0) & 0xFFu;
  dest[1] = (value >> 8) & 0xFFu;
  dest[2] = (value >> 16) & 0xFFu;
  dest[3] = (value >> 24) & 0xFFu;
}

struct BitmapHeaderFile {
  uint8 magic[2];
  uint8 size[4];
  uint8 reserved[4];
  uint8 offset[4];
};

struct BitmapInfoHeader{
  uint8 size[4];
  uint8 width[4];
  uint8 height[4];
  uint8 planes[2];
  uint8 bit_count[2];
  uint8 compression[4];
  uint8 size_image[4];
  uint8 x_pels_per_meter[4];
  uint8 y_pels_per_meter[4];
  uint8 clr_used[4];
  uint8 clr_important[4];
};

}

bool GLTestHelper::SaveBackbufferAsBMP(
    const char* filename, int width, int height) {
  FILE* fp = fopen(filename, "wb");
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  int num_pixels = width * height;
  int size = num_pixels * 4;
  scoped_array<uint8> data(new uint8[size]);
  uint8* pixels = data.get();
  glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

  // RGBA to BGRA
  for (int ii = 0; ii < num_pixels; ++ii) {
    int offset = ii * 4;
    uint8 t = pixels[offset + 0];
    pixels[offset + 0] = pixels[offset + 2];
    pixels[offset + 2] = t;
  }

  BitmapHeaderFile bhf;
  BitmapInfoHeader bih;

  bhf.magic[0] = 'B';
  bhf.magic[1] = 'M';
  Set32BitValue(bhf.size, 0);
  Set32BitValue(bhf.reserved, 0);
  Set32BitValue(bhf.offset, sizeof(bhf) + sizeof(bih));

  Set32BitValue(bih.size, sizeof(bih));
  Set32BitValue(bih.width, width);
  Set32BitValue(bih.height, height);
  Set16BitValue(bih.planes, 1);
  Set16BitValue(bih.bit_count, 32);
  Set32BitValue(bih.compression, 0);
  Set32BitValue(bih.x_pels_per_meter, 0);
  Set32BitValue(bih.y_pels_per_meter, 0);
  Set32BitValue(bih.clr_used, 0);
  Set32BitValue(bih.clr_important, 0);

  fwrite(&bhf, sizeof(bhf), 1, fp);
  fwrite(&bih, sizeof(bih), 1, fp);
  fwrite(pixels, size, 1, fp);
  fclose(fp);

  return true;
}



