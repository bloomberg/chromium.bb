// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/tests/gl_test_utils.h"

#include <GLES2/gl2extchromium.h>
#include <stdint.h>
#include <stdio.h>

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/strings/stringize_macros.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

// GCC requires these declarations, but MSVC requires they not be present.
#ifndef COMPILER_MSVC
const uint8_t GLTestHelper::kCheckClearValue;
#endif

// Compiles a fragment shader for sampling out of a texture of |size| bound to
// |target| and checks for compilation errors.
GLuint LoadFragmentShader(unsigned target, const gfx::Size& size) {
  // clang-format off
  const char kFragmentShader[] = STRINGIZE(
    uniform SamplerType u_texture;
    varying vec2 v_texCoord;
    void main() {
      gl_FragColor = TextureLookup(u_texture, v_texCoord * TextureScale);
    }
  );
  const char kShaderFloatPrecision[] = STRINGIZE(
    precision mediump float;
  );
  // clang-format on

  switch (target) {
    case GL_TEXTURE_2D:
      return GLTestHelper::LoadShader(
          GL_FRAGMENT_SHADER,
          base::StringPrintf("%s\n"
                             "#define SamplerType sampler2D\n"
                             "#define TextureLookup texture2D\n"
                             "#define TextureScale vec2(1.0, 1.0)\n"
                             "%s",
                             kShaderFloatPrecision,
                             kFragmentShader)
              .c_str());
    case GL_TEXTURE_RECTANGLE_ARB:
      return GLTestHelper::LoadShader(
          GL_FRAGMENT_SHADER,
          base::StringPrintf("%s\n"
                             "#extension GL_ARB_texture_rectangle : require\n"
                             "#define SamplerType sampler2DRect\n"
                             "#define TextureLookup texture2DRect\n"
                             "#define TextureScale vec2(%f, %f)\n"
                             "%s",
                             kShaderFloatPrecision,
                             static_cast<double>(size.width()),
                             static_cast<double>(size.height()),
                             kFragmentShader)
              .c_str());
    default:
      NOTREACHED();
      return 0;
  }
}

bool GLTestHelper::HasExtension(const char* extension) {
  // Pad with an extra space to ensure that |extension| is not a substring of
  // another extension.
  std::string extensions =
      std::string(reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS))) +
      " ";
  std::string extension_padded = std::string(extension) + " ";
  return extensions.find(extension_padded) != std::string::npos;
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

GLuint GLTestHelper::CompileShader(GLenum type, const char* shaderSrc) {
  GLuint shader = glCreateShader(type);
  // Load the shader source
  glShaderSource(shader, 1, &shaderSrc, NULL);
  // Compile the shader
  glCompileShader(shader);

  return shader;
}

GLuint GLTestHelper::LoadShader(GLenum type, const char* shaderSrc) {
  GLuint shader = CompileShader(type, shaderSrc);

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

GLuint GLTestHelper::LinkProgram(
    GLuint vertex_shader, GLuint fragment_shader) {
  // Create the program object
  GLuint program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  // Link the program
  glLinkProgram(program);

  return program;
}

GLuint GLTestHelper::SetupProgram(
    GLuint vertex_shader, GLuint fragment_shader) {
  GLuint program = LinkProgram(vertex_shader, fragment_shader);
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

GLuint GLTestHelper::SetupUnitQuad(GLint position_location) {
  GLuint vbo = 0;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  static const float vertices[] = {
      1.0f, 1.0f, -1.0f, 1.0f,  -1.0f, -1.0f,
      1.0f, 1.0f, -1.0f, -1.0f, 1.0f,  -1.0f,
  };
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  glEnableVertexAttribArray(position_location);
  glVertexAttribPointer(position_location, 2, GL_FLOAT, GL_FALSE, 0, 0);

  return vbo;
}

std::vector<GLuint> GLTestHelper::SetupIndexedUnitQuad(
    GLint position_location) {
  GLuint array_buffer = SetupUnitQuad(position_location);
  static const uint8_t indices[] = {0, 1, 2, 3, 4, 5};
  GLuint index_buffer = 0;
  glGenBuffers(1, &index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6, indices, GL_STATIC_DRAW);
  std::vector<GLuint> buffers(2);
  buffers[0] = array_buffer;
  buffers[1] = index_buffer;
  return buffers;
}

GLuint GLTestHelper::SetupColorsForUnitQuad(
    GLint location, const GLfloat color[4], GLenum usage) {
  GLuint vbo = 0;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  GLfloat vertices[6 * 4];
  for (int ii = 0; ii < 6; ++ii) {
    for (int jj = 0; jj < 4; ++jj) {
      vertices[ii * 4 + jj] = color[jj];
    }
  }
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, usage);
  glEnableVertexAttribArray(location);
  glVertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE, 0, 0);

  return vbo;
}

bool GLTestHelper::CheckPixels(GLint x,
                               GLint y,
                               GLsizei width,
                               GLsizei height,
                               GLint tolerance,
                               const uint8_t* color) {
  GLsizei size = width * height * 4;
  std::unique_ptr<uint8_t[]> pixels(new uint8_t[size]);
  memset(pixels.get(), kCheckClearValue, size);
  glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());
  int bad_count = 0;
  for (GLint yy = 0; yy < height; ++yy) {
    for (GLint xx = 0; xx < width; ++xx) {
      int offset = yy * width * 4 + xx * 4;
      for (int jj = 0; jj < 4; ++jj) {
        uint8_t actual = pixels[offset + jj];
        uint8_t expected = color[jj];
        int diff = actual - expected;
        diff = diff < 0 ? -diff: diff;
        if (diff > tolerance) {
          EXPECT_EQ(expected, actual) << " at " << (xx + x) << ", " << (yy + y)
                                      << " channel " << jj;
          ++bad_count;
          // Exit early just so we don't spam the log but we print enough
          // to hopefully make it easy to diagnose the issue.
          if (bad_count > 16) {
            return false;
          }
        }
      }
    }
  }
  return bad_count == 0;
}

namespace {

void Set16BitValue(uint8_t dest[2], uint16_t value) {
  dest[0] = value & 0xFFu;
  dest[1] = value >> 8;
}

void Set32BitValue(uint8_t dest[4], uint32_t value) {
  dest[0] = (value >> 0) & 0xFFu;
  dest[1] = (value >> 8) & 0xFFu;
  dest[2] = (value >> 16) & 0xFFu;
  dest[3] = (value >> 24) & 0xFFu;
}

struct BitmapHeaderFile {
  uint8_t magic[2];
  uint8_t size[4];
  uint8_t reserved[4];
  uint8_t offset[4];
};

struct BitmapInfoHeader{
  uint8_t size[4];
  uint8_t width[4];
  uint8_t height[4];
  uint8_t planes[2];
  uint8_t bit_count[2];
  uint8_t compression[4];
  uint8_t size_image[4];
  uint8_t x_pels_per_meter[4];
  uint8_t y_pels_per_meter[4];
  uint8_t clr_used[4];
  uint8_t clr_important[4];
};

}

bool GLTestHelper::SaveBackbufferAsBMP(
    const char* filename, int width, int height) {
  FILE* fp = fopen(filename, "wb");
  EXPECT_TRUE(fp != NULL);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  int num_pixels = width * height;
  int size = num_pixels * 4;
  std::unique_ptr<uint8_t[]> data(new uint8_t[size]);
  uint8_t* pixels = data.get();
  glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

  // RGBA to BGRA
  for (int ii = 0; ii < num_pixels; ++ii) {
    int offset = ii * 4;
    uint8_t t = pixels[offset + 0];
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

void GLTestHelper::DrawTextureQuad(GLenum target, const gfx::Size& size) {
  // clang-format off
  const char kVertexShader[] = STRINGIZE(
    attribute vec2 a_position;
    varying vec2 v_texCoord;
    void main() {
      gl_Position = vec4(a_position.x, a_position.y, 0.0, 1.0);
      v_texCoord = (a_position + vec2(1.0, 1.0)) * 0.5;
    }
  );
  // clang-format on

  // Setup program.
  GLuint vertex_shader =
      GLTestHelper::LoadShader(GL_VERTEX_SHADER, kVertexShader);
  GLuint fragment_shader = LoadFragmentShader(target, size);
  GLuint program = GLTestHelper::SetupProgram(vertex_shader, fragment_shader);
  EXPECT_NE(program, 0u);
  glUseProgram(program);

  GLint position_loc = glGetAttribLocation(program, "a_position");
  GLint sampler_location = glGetUniformLocation(program, "u_texture");
  ASSERT_NE(position_loc, -1);
  ASSERT_NE(sampler_location, -1);

  GLuint vertex_buffer = GLTestHelper::SetupUnitQuad(position_loc);
  ASSERT_NE(vertex_buffer, 0u);
  glUniform1i(sampler_location, 0);

  glDrawArrays(GL_TRIANGLES, 0, 6);

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  glDeleteProgram(program);
  glDeleteBuffers(1, &vertex_buffer);
}
