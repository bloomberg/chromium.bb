// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest.h"

namespace gpu {
namespace gles2 {

using namespace cmds;

TEST_F(GLES3DecoderPassthroughTest, ReadPixelsBufferBound) {
  const GLsizei kWidth = 5;
  const GLsizei kHeight = 3;
  const GLint kBytesPerPixel = 4;
  GLint size = kWidth * kHeight * kBytesPerPixel;
  typedef ReadPixels::Result Result;
  Result* result = GetSharedMemoryAs<Result*>();
  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t pixels_shm_id = shared_memory_id_;
  uint32_t pixels_shm_offset = kSharedMemoryOffset + sizeof(Result);

  DoBindBuffer(GL_PIXEL_PACK_BUFFER, kClientBufferId);
  DoBufferData(GL_PIXEL_PACK_BUFFER, size, nullptr, GL_STATIC_DRAW);

  ReadPixels cmd;
  cmd.Init(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels_shm_id,
           pixels_shm_offset, result_shm_id, result_shm_offset, false);
  result->success = 0;
  EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES3DecoderPassthroughTest, ReadPixels2PixelPackBufferNoBufferBound) {
  const GLsizei kWidth = 5;
  const GLsizei kHeight = 3;

  ReadPixels cmd;
  cmd.Init(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0, 0, 0, false);
  EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES3DecoderPassthroughTest, ReadPixels2PixelPackBuffer) {
  const GLsizei kWidth = 5;
  const GLsizei kHeight = 3;
  const GLint kBytesPerPixel = 4;
  GLint size = kWidth * kHeight * kBytesPerPixel;

  DoBindBuffer(GL_PIXEL_PACK_BUFFER, kClientBufferId);
  DoBufferData(GL_PIXEL_PACK_BUFFER, size, nullptr, GL_STATIC_DRAW);

  ReadPixels cmd;
  cmd.Init(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0, 0, 0, false);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderPassthroughTest, DiscardFramebufferEXTUnsupported) {
  const GLenum target = GL_FRAMEBUFFER;
  const GLsizei count = 1;
  const GLenum attachments[] = {GL_COLOR_EXT};
  DiscardFramebufferEXTImmediate& cmd =
      *GetImmediateAs<DiscardFramebufferEXTImmediate>();
  cmd.Init(target, count, attachments);
  EXPECT_EQ(error::kUnknownCommand,
            ExecuteImmediateCmd(cmd, sizeof(attachments)));
}

TEST_F(GLES2DecoderPassthroughTest, ReadPixelsOutOfRange) {
  const GLint kWidth = 5;
  const GLint kHeight = 3;
  const GLenum kFormat = GL_RGBA;

  // Set up GL objects for the read pixels with a known framebuffer size
  DoBindTexture(GL_TEXTURE_2D, kClientTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, kFormat, kWidth, kHeight, 0, kFormat,
               GL_UNSIGNED_BYTE, 0, 0);
  DoBindFramebuffer(GL_FRAMEBUFFER, kClientFramebufferId);
  DoFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         kClientTextureId, 0);

  // Put the resulting pixels and the result in shared memory
  typedef ReadPixels::Result Result;
  Result* result = GetSharedMemoryAs<Result*>();
  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t pixels_shm_id = shared_memory_id_;
  uint32_t pixels_shm_offset = kSharedMemoryOffset + sizeof(*result);

  uint8_t* dest = reinterpret_cast<uint8_t*>(&result[1]);

  // The test cases
  static struct {
    GLint x, y, w, h;
  } tests[] = {
      {
          -2, -1, 9, 5,
      },  // out of range on all sides
      {
          2, 1, 9, 5,
      },  // out of range on right, bottom
      {
          -7, -4, 9, 5,
      },  // out of range on left, top
      {
          0, -5, 9, 5,
      },  // completely off top
      {
          0, 3, 9, 5,
      },  // completely off bottom
      {
          -9, 0, 9, 5,
      },  // completely off left
      {
          5, 0, 9, 5,
      },  // completely off right
  };

  for (auto test : tests) {
    // Clear the readpixels buffer so that we can see which pixels have been
    // written
    memset(dest, 0, 4 * test.w * test.h);

    ReadPixels cmd;
    cmd.Init(test.x, test.y, test.w, test.h, kFormat, GL_UNSIGNED_BYTE,
             pixels_shm_id, pixels_shm_offset, result_shm_id, result_shm_offset,
             false);
    result->success = 0;
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

    EXPECT_TRUE(result->success);

    // Check the Result has the correct metadata for what was read.
    GLint startx = std::max(test.x, 0);
    GLint endx = std::min(test.x + test.w, kWidth);
    EXPECT_EQ(result->row_length, endx - startx);

    GLint starty = std::max(test.y, 0);
    GLint endy = std::min(test.y + test.h, kHeight);
    EXPECT_EQ(result->num_rows, endy - starty);

    // Check each pixel and expect them to be non-zero if they were written. The
    // non-zero values are written by ANGLE's NULL backend to simulate the
    // memory that would be modified by the call.
    for (GLint dx = 0; dx < test.w; ++dx) {
      GLint x = test.x + dx;
      for (GLint dy = 0; dy < test.h; ++dy) {
        GLint y = test.y + dy;

        bool expect_written = 0 <= x && x < kWidth && 0 <= y && y < kHeight;
        for (GLint component = 0; component < 4; ++component) {
          uint8_t value = dest[4 * (dy * test.w + dx) + component];
          EXPECT_EQ(expect_written, value != 0)
              << x << " " << y << " " << value;
        }
      }
    }
  }
}

}  // namespace gles2
}  // namespace gpu
