// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2chromium.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

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

class GpuMemoryBufferTest
    : public testing::TestWithParam<gfx::GpuMemoryBuffer::Format> {
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

std::vector<uint8> GetTexturePixel(const gfx::GpuMemoryBuffer::Format format) {
  std::vector<uint8> pixel;
  switch (format) {
    case gfx::GpuMemoryBuffer::R_8:
      pixel.push_back(255u);
      return pixel;
    case gfx::GpuMemoryBuffer::RGBA_8888:
      pixel.push_back(255u);
      pixel.push_back(0u);
      pixel.push_back(0u);
      pixel.push_back(255u);
      return pixel;
    case gfx::GpuMemoryBuffer::BGRA_8888:
      pixel.push_back(0u);
      pixel.push_back(0u);
      pixel.push_back(255u);
      pixel.push_back(255u);
      return pixel;
    case gfx::GpuMemoryBuffer::ATC:
    case gfx::GpuMemoryBuffer::ATCIA:
    case gfx::GpuMemoryBuffer::DXT1:
    case gfx::GpuMemoryBuffer::DXT5:
    case gfx::GpuMemoryBuffer::ETC1:
    case gfx::GpuMemoryBuffer::RGBX_8888:
    case gfx::GpuMemoryBuffer::YUV_420:
      NOTREACHED();
      return std::vector<uint8>();
  }

  NOTREACHED();
  return std::vector<uint8>();
}

std::vector<uint8> GetFramebufferPixel(
    const gfx::GpuMemoryBuffer::Format format) {
  std::vector<uint8> pixel;
  switch (format) {
    case gfx::GpuMemoryBuffer::R_8:
    case gfx::GpuMemoryBuffer::RGBA_8888:
    case gfx::GpuMemoryBuffer::BGRA_8888:
      pixel.push_back(255u);
      pixel.push_back(0u);
      pixel.push_back(0u);
      pixel.push_back(255u);
      return pixel;
    case gfx::GpuMemoryBuffer::ATC:
    case gfx::GpuMemoryBuffer::ATCIA:
    case gfx::GpuMemoryBuffer::DXT1:
    case gfx::GpuMemoryBuffer::DXT5:
    case gfx::GpuMemoryBuffer::ETC1:
    case gfx::GpuMemoryBuffer::RGBX_8888:
    case gfx::GpuMemoryBuffer::YUV_420:
      NOTREACHED();
      return std::vector<uint8>();
  }

  NOTREACHED();
  return std::vector<uint8>();
}

GLenum InternalFormat(gfx::GpuMemoryBuffer::Format format) {
  switch (format) {
    case gfx::GpuMemoryBuffer::R_8:
      return GL_R8;
    case gfx::GpuMemoryBuffer::RGBA_8888:
      return GL_RGBA;
    case gfx::GpuMemoryBuffer::BGRA_8888:
      return GL_BGRA_EXT;
    case gfx::GpuMemoryBuffer::ATC:
    case gfx::GpuMemoryBuffer::ATCIA:
    case gfx::GpuMemoryBuffer::DXT1:
    case gfx::GpuMemoryBuffer::DXT5:
    case gfx::GpuMemoryBuffer::ETC1:
    case gfx::GpuMemoryBuffer::RGBX_8888:
    case gfx::GpuMemoryBuffer::YUV_420:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

}  // namespace

// An end to end test that tests the whole GpuMemoryBuffer lifecycle.
TEST_P(GpuMemoryBufferTest, Lifecycle) {
  ASSERT_TRUE((GetParam() != gfx::GpuMemoryBuffer::R_8) ||
              gl_.GetCapabilities().texture_rg);

  GLuint texture_id = 0;
  glGenTextures(1, &texture_id);
  ASSERT_NE(0u, texture_id);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  // Create the gpu memory buffer.
  scoped_ptr<gfx::GpuMemoryBuffer> buffer(gl_.CreateGpuMemoryBuffer(
      gfx::Size(kImageWidth, kImageHeight), GetParam()));

  // Map buffer for writing.
  void* data;
  bool rv = buffer->Map(&data);
  DCHECK(rv);

  uint8* mapped_buffer = static_cast<uint8*>(data);
  ASSERT_TRUE(mapped_buffer != NULL);

  // Assign a value to each pixel.
  uint32 stride = 0;
  buffer->GetStride(&stride);
  ASSERT_NE(stride, 0u);
  std::vector<uint8> pixel = GetTexturePixel(GetParam());
  for (int y = 0; y < kImageHeight; ++y) {
    for (int x = 0; x < kImageWidth; ++x) {
      std::copy(pixel.begin(), pixel.end(),
                mapped_buffer + y * stride + x * pixel.size());
    }
  }

  // Unmap the buffer.
  buffer->Unmap();

  // Create the image. This should add the image ID to the ImageManager.
  GLuint image_id =
      glCreateImageCHROMIUM(buffer->AsClientBuffer(), kImageWidth, kImageHeight,
                            InternalFormat(GetParam()));
  ASSERT_NE(0u, image_id);
  ASSERT_TRUE(gl_.decoder()->GetImageManager()->LookupImage(image_id) != NULL);

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
  GLTestHelper::CheckPixels(0, 0, kImageWidth, kImageHeight, 0,
                            &GetFramebufferPixel(GetParam()).front());
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
                        ::testing::Values(gfx::GpuMemoryBuffer::R_8,
                                          gfx::GpuMemoryBuffer::RGBA_8888,
                                          gfx::GpuMemoryBuffer::BGRA_8888));

}  // namespace gles2
}  // namespace gpu
