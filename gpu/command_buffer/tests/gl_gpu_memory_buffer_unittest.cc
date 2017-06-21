// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2chromium.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/process/process_handle.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/half_float.h"
#include "ui/gl/gl_image.h"

using testing::_;
using testing::IgnoreResult;
using testing::InvokeWithoutArgs;
using testing::Invoke;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;

namespace gpu {
namespace gles2 {

static const int kImageWidth = 32;
static const int kImageHeight = 32;

class GpuMemoryBufferTest : public testing::TestWithParam<gfx::BufferFormat> {
 protected:
  void SetUp() override {
    GLManager::Options options;
    options.size = gfx::Size(kImageWidth, kImageHeight);
    gl_.Initialize(options);
    gl_.MakeCurrent();
  }

  void TearDown() override {
    gl_.Destroy();
  }

  GLManager gl_;
};

namespace {

#define SHADER(Src) #Src

// clang-format off
const char kVertexShader[] =
SHADER(
  attribute vec4 a_position;
  varying vec2 v_texCoord;
  void main() {
    gl_Position = a_position;
    v_texCoord = vec2((a_position.x + 1.0) * 0.5, (a_position.y + 1.0) * 0.5);
  }
);

const char* kFragmentShader =
SHADER(
  precision mediump float;
  uniform sampler2D a_texture;
  varying vec2 v_texCoord;
  void main() {
    gl_FragColor = texture2D(a_texture, v_texCoord);
  }
);
// clang-format on

void SetRow(gfx::BufferFormat format,
            uint8_t* buffer,
            int width,
            uint8_t pixel[4]) {
  switch (format) {
    case gfx::BufferFormat::R_8:
      for (int i = 0; i < width; ++i)
        buffer[i] = pixel[0];
      return;
    case gfx::BufferFormat::BGR_565:
      for (int i = 0; i < width * 2; i += 2) {
        *reinterpret_cast<uint16_t*>(&buffer[i]) =
            ((pixel[2] >> 3) << 11) | ((pixel[1] >> 2) << 5) | (pixel[0] >> 3);
      }
      return;
    case gfx::BufferFormat::RGBA_4444:
      for (int i = 0; i < width * 2; i += 2) {
        buffer[i + 0] = (pixel[1] << 4) | (pixel[0] & 0xf);
        buffer[i + 1] = (pixel[3] << 4) | (pixel[2] & 0xf);
      }
      return;
    case gfx::BufferFormat::RGBA_8888:
      for (int i = 0; i < width * 4; i += 4) {
        buffer[i + 0] = pixel[0];
        buffer[i + 1] = pixel[1];
        buffer[i + 2] = pixel[2];
        buffer[i + 3] = pixel[3];
      }
      return;
    case gfx::BufferFormat::BGRA_8888:
      for (int i = 0; i < width * 4; i += 4) {
        buffer[i + 0] = pixel[2];
        buffer[i + 1] = pixel[1];
        buffer[i + 2] = pixel[0];
        buffer[i + 3] = pixel[3];
      }
      return;
    case gfx::BufferFormat::RGBA_F16: {
      float float_pixel[4] = {
          pixel[0] / 255.f, pixel[1] / 255.f, pixel[2] / 255.f,
          pixel[3] / 255.f,
      };
      uint16_t half_float_pixel[4];
      gfx::FloatToHalfFloat(float_pixel, half_float_pixel, 4);
      uint16_t* half_float_buffer = reinterpret_cast<uint16_t*>(buffer);
      for (int i = 0; i < width * 4; i += 4) {
        half_float_buffer[i + 0] = half_float_pixel[0];
        half_float_buffer[i + 1] = half_float_pixel[1];
        half_float_buffer[i + 2] = half_float_pixel[2];
        half_float_buffer[i + 3] = half_float_pixel[3];
      }
      return;
    }
    case gfx::BufferFormat::ATC:
    case gfx::BufferFormat::ATCIA:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::DXT1:
    case gfx::BufferFormat::DXT5:
    case gfx::BufferFormat::ETC1:
    case gfx::BufferFormat::R_16:
    case gfx::BufferFormat::RG_88:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::UYVY_422:
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      NOTREACHED();
      return;
  }

  NOTREACHED();
}

GLenum InternalFormat(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
      return GL_RED;
    case gfx::BufferFormat::R_16:
      return GL_R16_EXT;
    case gfx::BufferFormat::RG_88:
      return GL_RG;
    case gfx::BufferFormat::BGR_565:
      return GL_RGB;
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBA_8888:
      return GL_RGBA;
    case gfx::BufferFormat::BGRA_8888:
      return GL_BGRA_EXT;
    case gfx::BufferFormat::RGBA_F16:
      return GL_RGBA;
    case gfx::BufferFormat::ATC:
    case gfx::BufferFormat::ATCIA:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::DXT1:
    case gfx::BufferFormat::DXT5:
    case gfx::BufferFormat::ETC1:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::UYVY_422:
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

}  // namespace

// An end to end test that tests the whole GpuMemoryBuffer lifecycle.
TEST_P(GpuMemoryBufferTest, Lifecycle) {
  if (GetParam() == gfx::BufferFormat::R_8 &&
      !gl_.GetCapabilities().texture_rg) {
    LOG(WARNING) << "texture_rg not supported. Skipping test.";
    return;
  }

  GLuint texture_id = 0;
  glGenTextures(1, &texture_id);
  ASSERT_NE(0u, texture_id);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  // Create the gpu memory buffer.
  std::unique_ptr<gfx::GpuMemoryBuffer> buffer(gl_.CreateGpuMemoryBuffer(
      gfx::Size(kImageWidth, kImageHeight), GetParam()));

  // Map buffer for writing.
  ASSERT_TRUE(buffer->Map());
  ASSERT_NE(nullptr, buffer->memory(0));
  ASSERT_NE(0, buffer->stride(0));
  uint8_t pixel[] = {255u, 0u, 0u, 255u};

  // Assign a value to each pixel.
  for (int y = 0; y < kImageHeight; ++y) {
    SetRow(GetParam(),
           static_cast<uint8_t*>(buffer->memory(0)) + y * buffer->stride(0),
           kImageWidth, pixel);
  }
  // Unmap the buffer.
  buffer->Unmap();

  // Create the image. This should add the image ID to the ImageManager.
  GLuint image_id =
      glCreateImageCHROMIUM(buffer->AsClientBuffer(), kImageWidth, kImageHeight,
                            InternalFormat(GetParam()));
  ASSERT_NE(0u, image_id);
  ASSERT_TRUE(gl_.decoder()->GetImageManagerForTest()->LookupImage(image_id) !=
              NULL);

  // Bind the image.
  glBindTexImage2DCHROMIUM(GL_TEXTURE_2D, image_id);

  // Build program, buffers and draw the texture.
  GLuint vertex_shader =
      GLTestHelper::LoadShader(GL_VERTEX_SHADER, kVertexShader);
  GLuint fragment_shader =
      GLTestHelper::LoadShader(GL_FRAGMENT_SHADER, kFragmentShader);
  GLuint program = GLTestHelper::SetupProgram(vertex_shader, fragment_shader);
  ASSERT_NE(0u, program);
  glUseProgram(program);

  GLint sampler_location = glGetUniformLocation(program, "a_texture");
  ASSERT_NE(-1, sampler_location);
  glUniform1i(sampler_location, 0);

  GLuint vbo =
      GLTestHelper::SetupUnitQuad(glGetAttribLocation(program, "a_position"));
  ASSERT_NE(0u, vbo);
  glViewport(0, 0, kImageWidth, kImageHeight);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  ASSERT_TRUE(glGetError() == GL_NO_ERROR);

  // Check if pixels match the values that were assigned to the mapped buffer.
  GLTestHelper::CheckPixels(0, 0, kImageWidth, kImageHeight, 0, pixel, nullptr);
  EXPECT_TRUE(GL_NO_ERROR == glGetError());

  // Release the image.
  glReleaseTexImage2DCHROMIUM(GL_TEXTURE_2D, image_id);

  // Clean up.
  glDeleteProgram(program);
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  glDeleteBuffers(1, &vbo);
  glDestroyImageCHROMIUM(image_id);
  glDeleteTextures(1, &texture_id);
}

INSTANTIATE_TEST_CASE_P(GpuMemoryBufferTests,
                        GpuMemoryBufferTest,
                        ::testing::Values(gfx::BufferFormat::R_8,
                                          gfx::BufferFormat::BGR_565,
                                          gfx::BufferFormat::RGBA_4444,
                                          gfx::BufferFormat::RGBA_8888,
                                          gfx::BufferFormat::BGRA_8888,
                                          gfx::BufferFormat::RGBA_F16));

}  // namespace gles2
}  // namespace gpu
