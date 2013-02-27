// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest_base.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"

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
void GLES2DecoderTestBase::SpecializedSetup<cmds::GenQueriesEXT, 0>(
    bool valid) {
  if (!valid) {
    // Make the client_query_id_ so that trying to make it again
    // will fail.
    GetSharedMemoryAs<GLuint*>()[0] = client_query_id_;
    cmds::GenQueriesEXT cmd;
    cmd.Init(1, shared_memory_id_, shared_memory_offset_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::GenQueriesEXTImmediate, 0>(
    bool valid) {
  if (!valid) {
    // Make the client_query_id_ so that trying to make it again
    // will fail.
    GetSharedMemoryAs<GLuint*>()[0] = client_query_id_;
    cmds::GenQueriesEXT cmd;
    cmd.Init(1, shared_memory_id_, shared_memory_offset_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::DeleteQueriesEXT, 0>(
    bool valid) {
  if (valid) {
    // Make the client_query_id_ so that trying to delete it will succeed.
    GetSharedMemoryAs<GLuint*>()[0] = client_query_id_;
    cmds::GenQueriesEXT cmd;
    cmd.Init(1, shared_memory_id_, shared_memory_offset_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::DeleteQueriesEXTImmediate, 0>(
    bool valid) {
  if (valid) {
    // Make the client_query_id_ so that trying to delete it will succeed.
    GetSharedMemoryAs<GLuint*>()[0] = client_query_id_;
    cmds::GenQueriesEXT cmd;
    cmd.Init(1, shared_memory_id_, shared_memory_offset_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::LinkProgram, 0>(
    bool /* valid */) {
  const GLuint kClientVertexShaderId = 5001;
  const GLuint kServiceVertexShaderId = 6001;
  const GLuint kClientFragmentShaderId = 5002;
  const GLuint kServiceFragmentShaderId = 6002;
  DoCreateShader(
      GL_VERTEX_SHADER, kClientVertexShaderId, kServiceVertexShaderId);
  DoCreateShader(
      GL_FRAGMENT_SHADER, kClientFragmentShaderId, kServiceFragmentShaderId);

  GetShader(kClientVertexShaderId)->SetStatus(true, "", NULL);
  GetShader(kClientFragmentShaderId)->SetStatus(true, "", NULL);

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

  cmds::AttachShader attach_cmd;
  attach_cmd.Init(client_program_id_, kClientVertexShaderId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(attach_cmd));

  attach_cmd.Init(client_program_id_, kClientFragmentShaderId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(attach_cmd));
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::ValidateProgram, 0>(
    bool /* valid */) {
  // Needs the same setup as LinkProgram.
  SpecializedSetup<cmds::LinkProgram, 0>(false);

  EXPECT_CALL(*gl_, LinkProgram(kServiceProgramId))
      .Times(1)
      .RetiresOnSaturation();

  cmds::LinkProgram link_cmd;
  link_cmd.Init(client_program_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(link_cmd));

  EXPECT_CALL(*gl_,
      GetProgramiv(kServiceProgramId, GL_INFO_LOG_LENGTH, _))
      .WillOnce(SetArgumentPointee<2>(0))
      .RetiresOnSaturation();
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform1f, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform1fv, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform1fvImmediate, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform1iv, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_INT);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform1ivImmediate, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_INT);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform2f, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_VEC2);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform2i, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_INT_VEC2);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform2fv, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_VEC2);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform2iv, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_INT_VEC2);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform2fvImmediate, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_VEC2);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform2ivImmediate, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_INT_VEC2);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform3f, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_VEC3);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform3i, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_INT_VEC3);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform3fv, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_VEC3);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform3iv, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_INT_VEC3);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform3fvImmediate, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_VEC3);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform3ivImmediate, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_INT_VEC3);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform4f, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_VEC4);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform4i, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_INT_VEC4);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform4fv, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_VEC4);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform4iv, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_INT_VEC4);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform4fvImmediate, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_VEC4);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform4ivImmediate, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_INT_VEC4);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::UniformMatrix2fv, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_MAT2);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::UniformMatrix2fvImmediate, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_MAT2);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::UniformMatrix3fv, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_MAT3);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::UniformMatrix3fvImmediate, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_MAT3);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::UniformMatrix4fv, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_MAT4);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::UniformMatrix4fvImmediate, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_MAT4);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::RenderbufferStorage, 0>(
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
void GLES2DecoderTestBase::SpecializedSetup<cmds::TexParameterf, 0>(
    bool /* valid */) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::TexParameteri, 0>(
    bool /* valid */) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::TexParameterfv, 0>(
    bool /* valid */) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::TexParameterfvImmediate, 0>(
    bool /* valid */) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::TexParameteriv, 0>(
    bool /* valid */) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::TexParameterivImmediate, 0>(
    bool /* valid */) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
};

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::GetVertexAttribiv, 0>(
    bool valid) {
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

