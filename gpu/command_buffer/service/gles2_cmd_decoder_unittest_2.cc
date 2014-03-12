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
using ::testing::AnyNumber;
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

  void TestAcceptedUniform(GLenum uniform_type, uint32 accepts_apis) {
    SetupShaderForUniform(uniform_type);
    bool valid_uniform = false;

    EXPECT_CALL(*gl_, Uniform1i(1, _)).Times(AnyNumber());
    EXPECT_CALL(*gl_, Uniform1iv(1, _, _)).Times(AnyNumber());
    EXPECT_CALL(*gl_, Uniform2iv(1, _, _)).Times(AnyNumber());
    EXPECT_CALL(*gl_, Uniform3iv(1, _, _)).Times(AnyNumber());
    EXPECT_CALL(*gl_, Uniform4iv(1, _, _)).Times(AnyNumber());
    EXPECT_CALL(*gl_, Uniform1f(1, _)).Times(AnyNumber());
    EXPECT_CALL(*gl_, Uniform1fv(1, _, _)).Times(AnyNumber());
    EXPECT_CALL(*gl_, Uniform2fv(1, _, _)).Times(AnyNumber());
    EXPECT_CALL(*gl_, Uniform3fv(1, _, _)).Times(AnyNumber());
    EXPECT_CALL(*gl_, Uniform4fv(1, _, _)).Times(AnyNumber());
    EXPECT_CALL(*gl_, UniformMatrix2fv(1, _, _, _)).Times(AnyNumber());
    EXPECT_CALL(*gl_, UniformMatrix3fv(1, _, _, _)).Times(AnyNumber());
    EXPECT_CALL(*gl_, UniformMatrix4fv(1, _, _, _)).Times(AnyNumber());

    {
      valid_uniform = accepts_apis & Program::kUniform1i;
      cmds::Uniform1i cmd;
      cmd.Init(1, 2);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = accepts_apis & Program::kUniform1i;
      cmds::Uniform1iv cmd;
      cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = accepts_apis & Program::kUniform2i;
      cmds::Uniform2i cmd;
      cmd.Init(1, 2, 3);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = accepts_apis & Program::kUniform2i;
      cmds::Uniform2iv cmd;
      cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = accepts_apis & Program::kUniform3i;
      cmds::Uniform3i cmd;
      cmd.Init(1, 2, 3, 4);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = accepts_apis & Program::kUniform3i;
      cmds::Uniform3iv cmd;
      cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = accepts_apis & Program::kUniform4i;
      cmds::Uniform4i cmd;
      cmd.Init(1, 2, 3, 4, 5);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = accepts_apis & Program::kUniform4i;
      cmds::Uniform4iv cmd;
      cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    ////////////////////

    {
      valid_uniform = accepts_apis & Program::kUniform1f;
      cmds::Uniform1f cmd;
      cmd.Init(1, 2);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = accepts_apis & Program::kUniform1f;
      cmds::Uniform1fv cmd;
      cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = accepts_apis & Program::kUniform2f;
      cmds::Uniform2f cmd;
      cmd.Init(1, 2, 3);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = accepts_apis & Program::kUniform2f;
      cmds::Uniform2fv cmd;
      cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = accepts_apis & Program::kUniform3f;
      cmds::Uniform3f cmd;
      cmd.Init(1, 2, 3, 4);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = accepts_apis & Program::kUniform3f;
      cmds::Uniform3fv cmd;
      cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = accepts_apis & Program::kUniform4f;
      cmds::Uniform4f cmd;
      cmd.Init(1, 2, 3, 4, 5);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = accepts_apis & Program::kUniform4f;
      cmds::Uniform4fv cmd;
      cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = accepts_apis & Program::kUniformMatrix2f;
      cmds::UniformMatrix2fv cmd;
      cmd.Init(1, 2, false, shared_memory_id_, shared_memory_offset_);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = accepts_apis & Program::kUniformMatrix3f;
      cmds::UniformMatrix3fv cmd;
      cmd.Init(1, 2, false, shared_memory_id_, shared_memory_offset_);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = accepts_apis & Program::kUniformMatrix4f;
      cmds::UniformMatrix4fv cmd;
      cmd.Init(1, 2, false, shared_memory_id_, shared_memory_offset_);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }
  }
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

#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest_2_autogen.h"

TEST_F(GLES2DecoderTest2, AcceptsUniform_GL_INT) {
  TestAcceptedUniform(GL_INT, Program::kUniform1i);
}

TEST_F(GLES2DecoderTest2, AcceptsUniform_GL_INT_VEC2) {
  TestAcceptedUniform(GL_INT_VEC2, Program::kUniform2i);
}

TEST_F(GLES2DecoderTest2, AcceptsUniform_GL_INT_VEC3) {
  TestAcceptedUniform(GL_INT_VEC3, Program::kUniform3i);
}

TEST_F(GLES2DecoderTest2, AcceptsUniform_GL_INT_VEC4) {
  TestAcceptedUniform(GL_INT_VEC4, Program::kUniform4i);
}

TEST_F(GLES2DecoderTest2, AcceptsUniform_GL_BOOL) {
  TestAcceptedUniform(GL_BOOL, Program::kUniform1i | Program::kUniform1f);
}

TEST_F(GLES2DecoderTest2, AcceptsUniform_GL_BOOL_VEC2) {
  TestAcceptedUniform(GL_BOOL_VEC2, Program::kUniform2i | Program::kUniform2f);
}

TEST_F(GLES2DecoderTest2, AcceptsUniform_GL_BOOL_VEC3) {
  TestAcceptedUniform(GL_BOOL_VEC3, Program::kUniform3i | Program::kUniform3f);
}

TEST_F(GLES2DecoderTest2, AcceptsUniform_GL_BOOL_VEC4) {
  TestAcceptedUniform(GL_BOOL_VEC4, Program::kUniform4i | Program::kUniform4f);
}

TEST_F(GLES2DecoderTest2, AcceptsUniformTypeFLOAT) {
  TestAcceptedUniform(GL_FLOAT, Program::kUniform1f);
}

TEST_F(GLES2DecoderTest2, AcceptsUniform_GL_FLOAT_VEC2) {
  TestAcceptedUniform(GL_FLOAT_VEC2, Program::kUniform2f);
}

TEST_F(GLES2DecoderTest2, AcceptsUniform_GL_FLOAT_VEC3) {
  TestAcceptedUniform(GL_FLOAT_VEC3, Program::kUniform3f);
}

TEST_F(GLES2DecoderTest2, AcceptsUniform_GL_FLOAT_VEC4) {
  TestAcceptedUniform(GL_FLOAT_VEC4, Program::kUniform4f);
}

TEST_F(GLES2DecoderTest2, AcceptsUniform_GL_FLOAT_MAT2) {
  TestAcceptedUniform(GL_FLOAT_MAT2, Program::kUniformMatrix2f);
}

TEST_F(GLES2DecoderTest2, AcceptsUniform_GL_FLOAT_MAT3) {
  TestAcceptedUniform(GL_FLOAT_MAT3, Program::kUniformMatrix3f);
}

TEST_F(GLES2DecoderTest2, AcceptsUniform_GL_FLOAT_MAT4) {
  TestAcceptedUniform(GL_FLOAT_MAT4, Program::kUniformMatrix4f);
}

}  // namespace gles2
}  // namespace gpu

