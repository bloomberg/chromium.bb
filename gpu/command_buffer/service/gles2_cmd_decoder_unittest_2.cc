// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/gl_mock.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest_base.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::gles2::MockGLInterface;
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

class GLES2DecoderTest2 : public GLES2DecoderTestBase {
 public:
  GLES2DecoderTest2() { }
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<LinkProgram, 0>() {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  InSequence dummy;
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_ACTIVE_ATTRIBUTES, _))
      .WillOnce(SetArgumentPointee<2>(0));
  EXPECT_CALL(
      *gl_,
      GetProgramiv(kServiceProgramId, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, _))
      .WillOnce(SetArgumentPointee<2>(0));
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_ACTIVE_UNIFORMS, _))
      .WillOnce(SetArgumentPointee<2>(0));
  EXPECT_CALL(
      *gl_,
      GetProgramiv(kServiceProgramId, GL_ACTIVE_UNIFORM_MAX_LENGTH, _))
      .WillOnce(SetArgumentPointee<2>(0));
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<RenderbufferStorage, 0>() {
  DoBindRenderbuffer(GL_RENDERBUFFER, client_renderbuffer_id_,
                    kServiceRenderbufferId);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<TexParameterf, 0>() {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<TexParameteri, 0>() {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<TexParameterfv, 0>() {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<TexParameterfvImmediate, 0>() {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<TexParameteriv, 0>() {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<TexParameterivImmediate, 0>() {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
};

#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest_2_autogen.h"

}  // namespace gles2
}  // namespace gpu

