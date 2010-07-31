// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

#include "app/gfx/gl/gl_mock.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest_base.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::gfx::MockGLInterface;
using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::MatcherCast;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArrayArgument;
using ::testing::SetArgumentPointee;
using ::testing::StrEq;

namespace gpu {
namespace gles2 {

class GLES2DecoderTest1 : public GLES2DecoderTestBase {
 public:
  GLES2DecoderTest1() { }
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<GenerateMipmap, 0>(
    bool /* valid */) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE,
      0, 0);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<CheckFramebufferStatus, 0>(
    bool /* valid */) {
  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<CopyTexImage2D, 0>(
    bool valid) {
  if (valid) {
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
  }
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<CopyTexSubImage2D, 0>(
    bool valid) {
  if (valid) {
    DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
    DoTexImage2D(
        GL_TEXTURE_2D, 2, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE,
        0, 0);
  }
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<FramebufferRenderbuffer, 0>(
    bool /* valid */) {
  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<FramebufferTexture2D, 0>(
    bool /* valid */) {
  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<GetFramebufferAttachmentParameteriv,
                                            0>(bool /* valid */) {
  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<GetRenderbufferParameteriv, 0>(
    bool /* valid */) {
  DoBindRenderbuffer(GL_RENDERBUFFER, client_renderbuffer_id_,
                    kServiceRenderbufferId);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<GetProgramInfoLog, 0>(
    bool /* valid */) {
  ProgramManager::ProgramInfo* info = GetProgramInfo(client_program_id_);
  info->set_log_info("hello");
};


#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest_1_autogen.h"

}  // namespace gles2
}  // namespace gpu

