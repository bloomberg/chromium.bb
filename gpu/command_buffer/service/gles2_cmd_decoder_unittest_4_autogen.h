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

TEST_P(GLES2DecoderTest4, SetDrawRectangleCHROMIUMValidArgs) {
  EXPECT_CALL(*gl_, SetDrawRectangleCHROMIUM(1, 2, 3, 4));
  SpecializedSetup<cmds::SetDrawRectangleCHROMIUM, 0>(true);
  cmds::SetDrawRectangleCHROMIUM cmd;
  cmd.Init(1, 2, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest4, SetEnableDCLayersCHROMIUMValidArgs) {
  EXPECT_CALL(*gl_, SetEnableDCLayersCHROMIUM(true));
  SpecializedSetup<cmds::SetEnableDCLayersCHROMIUM, 0>(true);
  cmds::SetEnableDCLayersCHROMIUM cmd;
  cmd.Init(true);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest4, CreateTransferCacheEntryINTERNALValidArgs) {
  EXPECT_CALL(*gl_, CreateTransferCacheEntryINTERNAL(1, 2, 3, 4, 5, 6, 7));
  SpecializedSetup<cmds::CreateTransferCacheEntryINTERNAL, 0>(true);
  cmds::CreateTransferCacheEntryINTERNAL cmd;
  cmd.Init(1, 2, 3, 4, 5, 6, 7);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest4, DeleteTransferCacheEntryINTERNALValidArgs) {
  EXPECT_CALL(*gl_, DeleteTransferCacheEntryINTERNAL(1, 2));
  SpecializedSetup<cmds::DeleteTransferCacheEntryINTERNAL, 0>(true);
  cmds::DeleteTransferCacheEntryINTERNAL cmd;
  cmd.Init(1, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest4, UnlockTransferCacheEntryINTERNALValidArgs) {
  EXPECT_CALL(*gl_, UnlockTransferCacheEntryINTERNAL(1, 2));
  SpecializedSetup<cmds::UnlockTransferCacheEntryINTERNAL, 0>(true);
  cmds::UnlockTransferCacheEntryINTERNAL cmd;
  cmd.Init(1, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_4_AUTOGEN_H_
