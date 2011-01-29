// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

#include "gpu/command_buffer/common/gl_mock.h"
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

class GLES2DecoderTest2 : public GLES2DecoderTestBase {
 public:
  GLES2DecoderTest2() { }
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<LinkProgram, 0>(bool /* valid */) {
  const GLuint kClientVertexShaderId = 5001;
  const GLuint kServiceVertexShaderId = 6001;
  const GLuint kClientFragmentShaderId = 5002;
  const GLuint kServiceFragmentShaderId = 6002;
  DoCreateShader(
      GL_VERTEX_SHADER, kClientVertexShaderId, kServiceVertexShaderId);
  DoCreateShader(
      GL_FRAGMENT_SHADER, kClientFragmentShaderId, kServiceFragmentShaderId);

  GetShaderInfo(kClientVertexShaderId)->SetStatus(true, "", NULL);
  GetShaderInfo(kClientFragmentShaderId)->SetStatus(true, "", NULL);

  InSequence dummy;
  EXPECT_CALL(*gl_,
              AttachShader(kServiceProgramId, kServiceVertexShaderId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
              AttachShader(kServiceProgramId, kServiceFragmentShaderId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_LINK_STATUS, _))
      .WillOnce(SetArgumentPointee<2>(1));
  EXPECT_CALL(*gl_,
      GetProgramiv(kServiceProgramId, GL_INFO_LOG_LENGTH, _))
      .WillOnce(SetArgumentPointee<2>(0))
      .RetiresOnSaturation();
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

  AttachShader attach_cmd;
  attach_cmd.Init(client_program_id_, kClientVertexShaderId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(attach_cmd));

  attach_cmd.Init(client_program_id_, kClientFragmentShaderId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(attach_cmd));
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<ValidateProgram, 0>(
    bool /* valid */) {
  // Needs the same setup as LinkProgram.
  SpecializedSetup<LinkProgram, 0>(false);

  EXPECT_CALL(*gl_, LinkProgram(kServiceProgramId))
      .Times(1)
      .RetiresOnSaturation();

  LinkProgram link_cmd;
  link_cmd.Init(client_program_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(link_cmd));

  EXPECT_CALL(*gl_,
      GetProgramiv(kServiceProgramId, GL_INFO_LOG_LENGTH, _))
      .WillOnce(SetArgumentPointee<2>(0))
      .RetiresOnSaturation();
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<Uniform1f, 0>(bool /* valid */) {
  SetupShaderForUniform();
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<Uniform1fv, 0>(bool /* valid */) {
  SetupShaderForUniform();
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<Uniform1fvImmediate, 0>(
    bool /* valid */) {
  SetupShaderForUniform();
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<Uniform2f, 0>(bool /* valid */) {
  SetupShaderForUniform();
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<Uniform2fv, 0>(bool /* valid */) {
  SetupShaderForUniform();
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<Uniform2fvImmediate, 0>(
    bool /* valid */) {
  SetupShaderForUniform();
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<Uniform3f, 0>(bool /* valid */) {
  SetupShaderForUniform();
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<Uniform3fv, 0>(bool /* valid */) {
  SetupShaderForUniform();
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<Uniform3fvImmediate, 0>(
    bool /* valid */) {
  SetupShaderForUniform();
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<Uniform4f, 0>(bool /* valid */) {
  SetupShaderForUniform();
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<Uniform4fv, 0>(bool /* valid */) {
  SetupShaderForUniform();
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<Uniform4fvImmediate, 0>(
    bool /* valid */) {
  SetupShaderForUniform();
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<RenderbufferStorage, 0>(
    bool valid) {
  DoBindRenderbuffer(GL_RENDERBUFFER, client_renderbuffer_id_,
                    kServiceRenderbufferId);
  if (valid) {
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_,
                RenderbufferStorageEXT(GL_RENDERBUFFER, _, 3, 4))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
  }
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<TexParameterf, 0>(
    bool /* valid */) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<TexParameteri, 0>(
    bool /* valid */) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<TexParameterfv, 0>(
    bool /* valid */) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<TexParameterfvImmediate, 0>(
    bool /* valid */) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<TexParameteriv, 0>(
    bool /* valid */) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<TexParameterivImmediate, 0>(
    bool /* valid */) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<GetVertexAttribiv, 0>(bool valid) {
  DoBindBuffer(GL_ARRAY_BUFFER, client_buffer_id_, kServiceBufferId);
  DoVertexAttribPointer(1, 1, GL_FLOAT, 0, 0);
  if (valid) {
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
  }
};

#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest_2_autogen.h"

}  // namespace gles2
}  // namespace gpu

