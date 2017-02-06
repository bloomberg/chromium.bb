// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// It is included by gles2_cmd_decoder_unittest_4.cc
#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_4_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_4_AUTOGEN_H_

TEST_P(GLES2DecoderTest4, SwapBuffersWithBoundsCHROMIUMImmediateValidArgs) {
  cmds::SwapBuffersWithBoundsCHROMIUMImmediate& cmd =
      *GetImmediateAs<cmds::SwapBuffersWithBoundsCHROMIUMImmediate>();
  SpecializedSetup<cmds::SwapBuffersWithBoundsCHROMIUMImmediate, 0>(true);
  GLint temp[4 * 2] = {
      0,
  };
  EXPECT_CALL(*gl_, SwapBuffersWithBoundsCHROMIUM(1, PointsToArray(temp, 4)));
  cmd.Init(1, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_4_AUTOGEN_H_
