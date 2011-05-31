// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated. DO NOT EDIT!

// It is included by gles2_cmd_decoder_unittest_3.cc
#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_3_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_3_AUTOGEN_H_

// TODO(gman): WaitLatchCHROMIUM


TEST_F(GLES2DecoderTest3, SetSurfaceCHROMIUMValidArgs) {
  EXPECT_CALL(*gl_, SetSurfaceCHROMIUM(1));
  SpecializedSetup<SetSurfaceCHROMIUM, 0>(true);
  SetSurfaceCHROMIUM cmd;
  cmd.Init(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_3_AUTOGEN_H_

