// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

TEST_F(GLES2UtilTest, ComputeImageDataSizeFormats) {
  const uint32 kWidth = 16;
  const uint32 kHeight = 12;
  uint32 size;
  EXPECT_TRUE(GLES2Util::ComputeImageDataSize(
      kWidth, kHeight, GL_RGB, GL_UNSIGNED_BYTE, 1, &size));
  EXPECT_EQ(kWidth * kHeight * 3, size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSize(
      kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, 1, &size));
  EXPECT_EQ(kWidth * kHeight * 4, size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSize(
      kWidth, kHeight, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1, &size));
  EXPECT_EQ(kWidth * kHeight * 1, size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSize(
      kWidth, kHeight, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, 1, &size));
  EXPECT_EQ(kWidth * kHeight * 2, size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSize(
      kWidth, kHeight, GL_BGRA_EXT, GL_UNSIGNED_BYTE, 1, &size));
  EXPECT_EQ(kWidth * kHeight * 4, size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSize(
      kWidth, kHeight, GL_ALPHA, GL_UNSIGNED_BYTE, 1, &size));
  EXPECT_EQ(kWidth * kHeight * 1, size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSize(
      kWidth, kHeight, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 1, &size));
  EXPECT_EQ(kWidth * kHeight * 2, size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSize(
      kWidth, kHeight, GL_DEPTH_STENCIL_OES, GL_UNSIGNED_INT_24_8_OES, 1,
      &size));
  EXPECT_EQ(kWidth * kHeight * 4, size);
}

TEST_F(GLES2UtilTest, ComputeImageDataSizeTypes) {
  const uint32 kWidth = 16;
  const uint32 kHeight = 12;
  uint32 size;
  EXPECT_TRUE(GLES2Util::ComputeImageDataSize(
      kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, 1, &size));
  EXPECT_EQ(kWidth * kHeight * 4, size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSize(
      kWidth, kHeight, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, 1, &size));
  EXPECT_EQ(kWidth * kHeight * 2, size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSize(
      kWidth, kHeight, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, 1, &size));
  EXPECT_EQ(kWidth * kHeight * 2, size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSize(
      kWidth, kHeight, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 1, &size));
  EXPECT_EQ(kWidth * kHeight * 2, size);
}

TEST_F(GLES2UtilTest, ComputeImageDataSizeUnpackAlignment) {
  const uint32 kWidth = 19;
  const uint32 kHeight = 12;
  uint32 size;
  EXPECT_TRUE(GLES2Util::ComputeImageDataSize(
      kWidth, kHeight, GL_RGB, GL_UNSIGNED_BYTE, 1, &size));
  EXPECT_EQ(kWidth * kHeight * 3, size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSize(
      kWidth, kHeight, GL_RGB, GL_UNSIGNED_BYTE, 2, &size));
  EXPECT_EQ((kWidth * 3 + 1) * (kHeight - 1) +
            kWidth * 3, size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSize(
      kWidth, kHeight, GL_RGB, GL_UNSIGNED_BYTE, 4, &size));
  EXPECT_EQ((kWidth * 3 + 3) * (kHeight - 1) +
            kWidth * 3, size);
  EXPECT_TRUE(GLES2Util::ComputeImageDataSize(
      kWidth, kHeight, GL_RGB, GL_UNSIGNED_BYTE, 8, &size));
  EXPECT_EQ((kWidth * 3 + 7) * (kHeight - 1) +
            kWidth * 3, size);
}

}  // namespace gles2
}  // namespace gpu

