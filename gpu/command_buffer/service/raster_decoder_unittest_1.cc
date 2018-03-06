// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/raster_decoder.h"

#include "base/command_line.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/raster_cmd_format.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/raster_decoder_unittest_base.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"

using ::gl::MockGLInterface;
using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::MatcherCast;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArrayArgument;
using ::testing::SetArgPointee;
using ::testing::StrEq;

namespace gpu {
namespace raster {

class RasterDecoderTest1 : public RasterDecoderTestBase {
 public:
  RasterDecoderTest1() = default;
};

INSTANTIATE_TEST_CASE_P(Service, RasterDecoderTest1, ::testing::Bool());

#include "gpu/command_buffer/service/raster_decoder_unittest_1_autogen.h"

TEST_P(RasterDecoderTest1, TexParameteriValidArgs) {
  EXPECT_CALL(*gl_, TexParameteri(_, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
  cmds::TexParameteri cmd;
  cmd.Init(client_texture_id_, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  EXPECT_CALL(*gl_, TexParameteri(_, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
  cmd.Init(client_texture_id_, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  EXPECT_CALL(*gl_, TexParameteri(_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  cmd.Init(client_texture_id_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  EXPECT_CALL(*gl_, TexParameteri(_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
  cmd.Init(client_texture_id_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(RasterDecoderTest1, TexParameteriInvalidArgsMipMap) {
  EXPECT_CALL(*gl_, TexParameteri(_, _, _)).Times(0);
  cmds::TexParameteri cmd;
  cmd.Init(client_texture_id_, GL_GENERATE_MIPMAP, GL_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(RasterDecoderTest1, TexParameteriInvalidArgsMagFilter) {
  EXPECT_CALL(*gl_, TexParameteri(_, _, _)).Times(0);
  cmds::TexParameteri cmd;
  cmd.Init(client_texture_id_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(RasterDecoderTest1, TexParameteriInvalidArgsMinFilter) {
  EXPECT_CALL(*gl_, TexParameteri(_, _, _)).Times(0);
  cmds::TexParameteri cmd;
  cmd.Init(client_texture_id_, GL_TEXTURE_MIN_FILTER,
           GL_NEAREST_MIPMAP_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(RasterDecoderTest1, TexParameteriInvalidArgsWrapS) {
  EXPECT_CALL(*gl_, TexParameteri(_, _, _)).Times(0);
  cmds::TexParameteri cmd;
  cmd.Init(client_texture_id_, GL_TEXTURE_WRAP_S, GL_REPEAT);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(RasterDecoderTest1, TexParameteriInvalidArgsWrapT) {
  EXPECT_CALL(*gl_, TexParameteri(_, _, _)).Times(0);
  cmds::TexParameteri cmd;
  cmd.Init(client_texture_id_, GL_TEXTURE_WRAP_T, GL_REPEAT);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

}  // namespace raster
}  // namespace gpu
