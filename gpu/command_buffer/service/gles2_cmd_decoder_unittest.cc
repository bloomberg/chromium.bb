// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/service/gl_mock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgumentPointee;

namespace gpu {
namespace gles2 {

class GLES2DecoderTest : public testing::Test {
 protected:
  virtual void SetUp() {
    gl_ = new ::gles2::MockGLInterface();
    ::gles2::GLInterface::SetGLInterface(gl_);

    EXPECT_CALL(*gl_, GetIntegerv(_, _))
        .WillOnce(SetArgumentPointee<1>(16));
    EXPECT_CALL(*gl_, GetError())
        .WillRepeatedly(Return(GL_NO_ERROR));

    decoder_ = GLES2Decoder::Create();
    decoder_->Initialize();
  }

  virtual void TearDown() {
    decoder_->Destroy();
    delete decoder_;
    ::gles2::GLInterface::SetGLInterface(NULL);
    delete gl_;
  }

  template <typename T>
  parse_error::ParseError ExecuteCmd(const T& cmd) {
    COMPILE_ASSERT(T::kArgFlags == cmd::kFixed, Cmd_kArgFlags_not_kFixed);
    return decoder_->DoCommand(cmd.kCmdId,
                               ComputeNumEntries(sizeof(cmd)) - 1,
                               &cmd);
  }

  template <typename T>
  parse_error::ParseError ExecuteImmediateCmd(const T& cmd, size_t data_size) {
    COMPILE_ASSERT(T::kArgFlags == cmd::kAtLeastN, Cmd_kArgFlags_not_kAtLeastN);
    return decoder_.DoCommand(cmd.kCmdId,
                              ComputeNumEntries(sizeof(cmd) + data_size) - 1,
                              &cmd);
  }

  ::gles2::MockGLInterface* gl_;
  GLES2Decoder* decoder_;
};

TEST_F(GLES2DecoderTest, Enable) {
  EXPECT_CALL(*gl_, Enable(GL_BLEND));

  Enable cmd;
  cmd.Init(GL_BLEND);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
}

}  // namespace gles2
}  // namespace gpu


