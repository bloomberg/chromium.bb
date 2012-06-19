// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/gles2_cmd_utils.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace gles2 {

class GLES2UtilTest : public testing:: Test {
 protected:
  GLES2Util util_;
};

TEST_F(GLES2UtilTest, GLGetNumValuesReturned) {
  EXPECT_EQ(0, util_.GLGetNumValuesReturned(GL_COMPRESSED_TEXTURE_FORMATS));
  EXPECT_EQ(0, util_.GLGetNumValuesReturned(GL_SHADER_BINARY_FORMATS));

  EXPECT_EQ(0, util_.num_compressed_texture_formats());
  EXPECT_EQ(0, util_.num_shader_binary_formats());

  util_.set_num_compressed_texture_formats(1);
  util_.set_num_shader_binary_formats(2);

  EXPECT_EQ(1, util_.GLGetNumValuesReturned(GL_COMPRESSED_TEXTURE_FORMATS));
  EXPECT_EQ(2, util_.GLGetNumValuesReturned(GL_SHADER_BINARY_FORMATS));

  EXPECT_EQ(1, util_.num_compressed_texture_formats());
  EXPECT_EQ(2, util_.num_shader_binary_formats());
}

TEST_F(GLES2UtilTest, ComputeImageDataSizesFormats) {
  const uint32 kWidth = 16;
  const uint32 kHeight = 12;
  uint32 size;
  uint32 unpadded_row_size;
  uint32 padded_row_size;
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, GL_RGB, GL_UNSIGNED_BYTE, 1, &size, &unpadded_row_size,
      &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 3, size);
  EXPECT_EQ(kWidth * 3, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, 1, &size, &unpadded_row_size,
      &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 4, size);
  EXPECT_EQ(kWidth * 4, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 1, size);
  EXPECT_EQ(kWidth * 1, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, 1, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 2, size);
  EXPECT_EQ(kWidth * 2, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, GL_BGRA_EXT, GL_UNSIGNED_BYTE, 1, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 4, size);
  EXPECT_EQ(kWidth * 4, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, GL_ALPHA, GL_UNSIGNED_BYTE, 1, &size, &unpadded_row_size,
      &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 1, size);
  EXPECT_EQ(kWidth * 1, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 1, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 2, size);
  EXPECT_EQ(kWidth * 2, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, GL_DEPTH_STENCIL_OES, GL_UNSIGNED_INT_24_8_OES, 1,
      &size, &unpadded_row_size,
      &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 4, size);
  EXPECT_EQ(kWidth * 4, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
}

TEST_F(GLES2UtilTest, ComputeImageDataSizeTypes) {
  const uint32 kWidth = 16;
  const uint32 kHeight = 12;
  uint32 size;
  uint32 unpadded_row_size;
  uint32 padded_row_size;
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, 1, &size, &unpadded_row_size,
      &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 4, size);
  EXPECT_EQ(kWidth * 4, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, 1, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 2, size);
  EXPECT_EQ(kWidth * 2, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, 1, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 2, size);
  EXPECT_EQ(kWidth * 2, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 1, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 2, size);
  EXPECT_EQ(kWidth * 2, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 1, &size,
      &unpadded_row_size, &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 4, size);
  EXPECT_EQ(kWidth * 4, padded_row_size);
  EXPECT_EQ(padded_row_size, unpadded_row_size);
}

TEST_F(GLES2UtilTest, ComputeImageDataSizesUnpackAlignment) {
  const uint32 kWidth = 19;
  const uint32 kHeight = 12;
  uint32 size;
  uint32 unpadded_row_size;
  uint32 padded_row_size;
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, GL_RGB, GL_UNSIGNED_BYTE, 1, &size, &unpadded_row_size,
      &padded_row_size));
  EXPECT_EQ(kWidth * kHeight * 3, size);
  EXPECT_EQ(kWidth * 3, unpadded_row_size);
  EXPECT_EQ(kWidth * 3, padded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, GL_RGB, GL_UNSIGNED_BYTE, 2, &size, &unpadded_row_size,
      &padded_row_size));
  EXPECT_EQ((kWidth * 3 + 1) * (kHeight - 1) +
            kWidth * 3, size);
  EXPECT_EQ(kWidth * 3, unpadded_row_size);
  EXPECT_EQ(kWidth * 3 + 1, padded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, GL_RGB, GL_UNSIGNED_BYTE, 4, &size, &unpadded_row_size,
      &padded_row_size));
  EXPECT_EQ((kWidth * 3 + 3) * (kHeight - 1) +
            kWidth * 3, size);
  EXPECT_EQ(kWidth * 3, unpadded_row_size);
  EXPECT_EQ(kWidth * 3 + 3, padded_row_size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, GL_RGB, GL_UNSIGNED_BYTE, 8, &size, &unpadded_row_size,
      &padded_row_size));
  EXPECT_EQ((kWidth * 3 + 7) * (kHeight - 1) +
            kWidth * 3, size);
  EXPECT_EQ(kWidth * 3, unpadded_row_size);
  EXPECT_EQ(kWidth * 3 + 7, padded_row_size);
}

TEST_F(GLES2UtilTest, RenderbufferBytesPerPixel) {
   EXPECT_EQ(1u, GLES2Util::RenderbufferBytesPerPixel(GL_STENCIL_INDEX8));
   EXPECT_EQ(2u, GLES2Util::RenderbufferBytesPerPixel(GL_RGBA4));
   EXPECT_EQ(2u, GLES2Util::RenderbufferBytesPerPixel(GL_RGB565));
   EXPECT_EQ(2u, GLES2Util::RenderbufferBytesPerPixel(GL_RGB5_A1));
   EXPECT_EQ(2u, GLES2Util::RenderbufferBytesPerPixel(GL_DEPTH_COMPONENT16));
   EXPECT_EQ(4u, GLES2Util::RenderbufferBytesPerPixel(GL_RGB));
   EXPECT_EQ(4u, GLES2Util::RenderbufferBytesPerPixel(GL_RGBA));
   EXPECT_EQ(
       4u, GLES2Util::RenderbufferBytesPerPixel(GL_DEPTH24_STENCIL8_OES));
   EXPECT_EQ(4u, GLES2Util::RenderbufferBytesPerPixel(GL_RGB8_OES));
   EXPECT_EQ(4u, GLES2Util::RenderbufferBytesPerPixel(GL_RGBA8_OES));
   EXPECT_EQ(
       4u, GLES2Util::RenderbufferBytesPerPixel(GL_DEPTH_COMPONENT24_OES));
   EXPECT_EQ(0u, GLES2Util::RenderbufferBytesPerPixel(-1));
}

TEST_F(GLES2UtilTest, SwizzleLocation) {
  GLint power = 1;
  for (GLint p = 0; p < 5; ++p, power *= 10) {
    GLint limit = power * 20 + 1;
    for (GLint ii = -limit; ii < limit; ii += power) {
      GLint s = GLES2Util::SwizzleLocation(ii);
      EXPECT_EQ(ii, GLES2Util::UnswizzleLocation(s));
    }
  }
}

}  // namespace gles2
}  // namespace gpu

