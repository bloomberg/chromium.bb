// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/gl_mock.h"
#include "gpu/command_buffer/common/id_allocator.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest_base.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::gfx::MockGLInterface;
using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::MatcherCast;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArrayArgument;
using ::testing::SetArgumentPointee;
using ::testing::StrEq;
using ::testing::StrictMock;

namespace gpu {
namespace gles2 {

class GLES2DecoderTest : public GLES2DecoderTestBase {
 public:
  GLES2DecoderTest() { }

 protected:
  void CheckReadPixelsOutOfRange(
      GLint in_read_x, GLint in_read_y,
      GLsizei in_read_width, GLsizei in_read_height,
      bool init);
};

class GLES2DecoderWithShaderTest : public GLES2DecoderWithShaderTestBase {
 public:
  GLES2DecoderWithShaderTest()
      : GLES2DecoderWithShaderTestBase() {
  }

  void AddExpectationsForSimulatedAttrib0(
      GLsizei num_vertices, GLuint buffer_id) {
    EXPECT_CALL(*gl_, BindBuffer(GL_ARRAY_BUFFER, kServiceAttrib0BufferId))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, BufferData(GL_ARRAY_BUFFER,
                                 num_vertices * sizeof(GLfloat) * 4,
                                 _, GL_DYNAMIC_DRAW))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, BufferSubData(
        GL_ARRAY_BUFFER, 0, num_vertices * sizeof(GLfloat) * 4, _))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, VertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, BindBuffer(GL_ARRAY_BUFFER, 0))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, VertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, BindBuffer(GL_ARRAY_BUFFER, buffer_id))
        .Times(1)
        .RetiresOnSaturation();
  }
};

TEST_F(GLES2DecoderWithShaderTest, DrawArraysNoAttributesSucceeds) {
  SetupTexture();
  AddExpectationsForSimulatedAttrib0(kNumVertices, 0);

  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawArraysBadTextureUsesBlack) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  // This is an NPOT texture. As the default filtering requires mips
  // this should trigger replacing with black textures before rendering.
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 3, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               0, 0);
  AddExpectationsForSimulatedAttrib0(kNumVertices, 0);
  {
    InSequence sequence;
    EXPECT_CALL(*gl_, ActiveTexture(GL_TEXTURE0))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, BindTexture(
        GL_TEXTURE_2D, TestHelper::kServiceBlackTexture2dId))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, ActiveTexture(GL_TEXTURE0))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_2D, kServiceTextureId))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, ActiveTexture(GL_TEXTURE0))
        .Times(1)
        .RetiresOnSaturation();
  }
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawArraysMissingAttributesFails) {
  DoEnableVertexAttribArray(1);

  EXPECT_CALL(*gl_, DrawArrays(_, _, _))
      .Times(0);
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest,
       DrawArraysMissingAttributesZeroCountSucceeds) {
  DoEnableVertexAttribArray(1);

  EXPECT_CALL(*gl_, DrawArrays(_, _, _))
      .Times(0);
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawArraysValidAttributesSucceeds) {
  SetupTexture();
  SetupVertexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);
  AddExpectationsForSimulatedAttrib0(kNumVertices, kServiceBufferId);

  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawArraysDeletedBufferFails) {
  SetupVertexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);
  DeleteVertexBuffer();

  EXPECT_CALL(*gl_, DrawArrays(_, _, _))
      .Times(0);
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawArraysDeletedProgramSucceeds) {
  SetupTexture();
  AddExpectationsForSimulatedAttrib0(kNumVertices, 0);
  DoDeleteProgram(client_program_id_, kServiceProgramId);

  EXPECT_CALL(*gl_, DrawArrays(_, _, _))
      .Times(1)
      .RetiresOnSaturation();
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawArraysWithInvalidModeFails) {
  SetupVertexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  EXPECT_CALL(*gl_, DrawArrays(_, _, _))
      .Times(0);
  DrawArrays cmd;
  cmd.Init(GL_QUADS, 0, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  cmd.Init(GL_POLYGON, 0, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawArraysInvalidCountFails) {
  SetupVertexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  // Try start > 0
  EXPECT_CALL(*gl_, DrawArrays(_, _, _)).Times(0);
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 1, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Try with count > size
  cmd.Init(GL_TRIANGLES, 0, kNumVertices + 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Try with attrib offset > 0
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Try with size > 2 (ie, vec3 instead of vec2)
  DoVertexAttribPointer(1, 3, GL_FLOAT, 0, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Try with stride > 8 (vec2 + vec2 byte)
  DoVertexAttribPointer(1, 2, GL_FLOAT, sizeof(GLfloat) * 3, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsNoAttributesSucceeds) {
  SetupTexture();
  SetupIndexBuffer();
  AddExpectationsForSimulatedAttrib0(kMaxValidIndex + 1, 0);
  EXPECT_CALL(*gl_, DrawElements(GL_TRIANGLES, kValidIndexRangeCount,
                                 GL_UNSIGNED_SHORT,
                                 BufferOffset(kValidIndexRangeStart * 2)))
      .Times(1)
      .RetiresOnSaturation();
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsMissingAttributesFails) {
  SetupIndexBuffer();
  DoEnableVertexAttribArray(1);

  EXPECT_CALL(*gl_, DrawElements(_, _, _, _))
      .Times(0);
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest,
       DrawElementsMissingAttributesZeroCountSucceeds) {
  SetupIndexBuffer();
  DoEnableVertexAttribArray(1);

  EXPECT_CALL(*gl_, DrawElements(_, _, _, _))
      .Times(0);
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, 0, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsExtraAttributesFails) {
  SetupIndexBuffer();
  DoEnableVertexAttribArray(6);

  EXPECT_CALL(*gl_, DrawElements(_, _, _, _))
      .Times(0);
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsValidAttributesSucceeds) {
  SetupTexture();
  SetupVertexBuffer();
  SetupIndexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);
  AddExpectationsForSimulatedAttrib0(kMaxValidIndex + 1, kServiceBufferId);

  EXPECT_CALL(*gl_, DrawElements(GL_TRIANGLES, kValidIndexRangeCount,
                                 GL_UNSIGNED_SHORT,
                                 BufferOffset(kValidIndexRangeStart * 2)))
      .Times(1)
      .RetiresOnSaturation();
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsDeletedBufferFails) {
  SetupVertexBuffer();
  SetupIndexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);
  DeleteIndexBuffer();

  EXPECT_CALL(*gl_, DrawElements(_, _, _, _))
      .Times(0);
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsDeletedProgramSucceeds) {
  SetupTexture();
  SetupIndexBuffer();
  AddExpectationsForSimulatedAttrib0(kMaxValidIndex + 1, 0);
  DoDeleteProgram(client_program_id_, kServiceProgramId);

  EXPECT_CALL(*gl_, DrawElements(_, _, _, _))
      .Times(1);
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsWithInvalidModeFails) {
  SetupVertexBuffer();
  SetupIndexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  EXPECT_CALL(*gl_, DrawElements(_, _, _, _))
      .Times(0);
  DrawElements cmd;
  cmd.Init(GL_QUADS, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  cmd.Init(GL_POLYGON, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsInvalidCountFails) {
  SetupVertexBuffer();
  SetupIndexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  // Try start > 0
  EXPECT_CALL(*gl_, DrawElements(_, _, _, _)).Times(0);
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kNumIndices, GL_UNSIGNED_SHORT, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Try with count > size
  cmd.Init(GL_TRIANGLES, kNumIndices + 1, GL_UNSIGNED_SHORT, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsOutOfRangeIndicesFails) {
  SetupVertexBuffer();
  SetupIndexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  EXPECT_CALL(*gl_, DrawElements(_, _, _, _)).Times(0);
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kInvalidIndexRangeCount, GL_UNSIGNED_SHORT,
           kInvalidIndexRangeStart * 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsOddOffsetForUint16Fails) {
  SetupVertexBuffer();
  SetupIndexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  EXPECT_CALL(*gl_, DrawElements(_, _, _, _)).Times(0);
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kInvalidIndexRangeCount, GL_UNSIGNED_SHORT, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetVertexAttribPointervSucceeds) {
  const float dummy = 0;
  const GLuint kOffsetToTestFor = sizeof(dummy) * 4;
  const GLuint kIndexToTest = 1;
  GetVertexAttribPointerv::Result* result =
      static_cast<GetVertexAttribPointerv::Result*>(shared_memory_address_);
  result->size = 0;
  const GLuint* result_value = result->GetData();
  // Test that initial value is 0.
  GetVertexAttribPointerv cmd;
  cmd.Init(kIndexToTest, GL_VERTEX_ATTRIB_ARRAY_POINTER,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(sizeof(*result_value), result->size);
  EXPECT_EQ(0u, *result_value);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Set the value and see that we get it.
  SetupVertexBuffer();
  DoVertexAttribPointer(kIndexToTest, 2, GL_FLOAT, 0, kOffsetToTestFor);
  result->size = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(sizeof(*result_value), result->size);
  EXPECT_EQ(kOffsetToTestFor, *result_value);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetVertexAttribPointervBadArgsFails) {
  const GLuint kIndexToTest = 1;
  GetVertexAttribPointerv::Result* result =
      static_cast<GetVertexAttribPointerv::Result*>(shared_memory_address_);
  result->size = 0;
  const GLuint* result_value = result->GetData();
  // Test pname invalid fails.
  GetVertexAttribPointerv cmd;
  cmd.Init(kIndexToTest, GL_VERTEX_ATTRIB_ARRAY_POINTER + 1,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
  EXPECT_EQ(kInitialResult, *result_value);
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());

  // Test index out of range fails.
  result->size = 0;
  cmd.Init(kNumVertexAttribs, GL_VERTEX_ATTRIB_ARRAY_POINTER,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
  EXPECT_EQ(kInitialResult, *result_value);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());

  // Test memory id bad fails.
  cmd.Init(kIndexToTest, GL_VERTEX_ATTRIB_ARRAY_POINTER,
           kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));

  // Test memory offset bad fails.
  cmd.Init(kIndexToTest, GL_VERTEX_ATTRIB_ARRAY_POINTER,
           shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformivSucceeds) {
  GetUniformiv::Result* result =
      static_cast<GetUniformiv::Result*>(shared_memory_address_);
  result->size = 0;
  GetUniformiv cmd;
  cmd.Init(client_program_id_, kUniform2Location,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformiv(kServiceProgramId, kUniform2Location, _))
      .Times(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GLES2Util::GetGLDataTypeSizeForUniforms(kUniform2Type),
            result->size);
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformivArrayElementSucceeds) {
  GetUniformiv::Result* result =
      static_cast<GetUniformiv::Result*>(shared_memory_address_);
  result->size = 0;
  GetUniformiv cmd;
  cmd.Init(client_program_id_, kUniform2ElementLocation,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_,
              GetUniformiv(kServiceProgramId, kUniform2ElementLocation, _))
      .Times(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GLES2Util::GetGLDataTypeSizeForUniforms(kUniform2Type),
            result->size);
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformivBadProgramFails) {
  GetUniformiv::Result* result =
      static_cast<GetUniformiv::Result*>(shared_memory_address_);
  result->size = 0;
  GetUniformiv cmd;
  // non-existant program
  cmd.Init(kInvalidClientId, kUniform2Location,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformiv(_, _, _))
      .Times(0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  // Valid id that is not a program. The GL spec requires a different error for
  // this case.
#if GLES2_TEST_SHADER_VS_PROGRAM_IDS
  result->size = kInitialResult;
  cmd.Init(client_shader_id_, kUniform2Location,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
#endif  // GLES2_TEST_SHADER_VS_PROGRAM_IDS
  // Unlinked program
  EXPECT_CALL(*gl_, CreateProgram())
      .Times(1)
      .WillOnce(Return(kNewServiceId))
      .RetiresOnSaturation();
  CreateProgram cmd2;
  cmd2.Init(kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  result->size = kInitialResult;
  cmd.Init(kNewClientId, kUniform2Location,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformivBadLocationFails) {
  GetUniformiv::Result* result =
      static_cast<GetUniformiv::Result*>(shared_memory_address_);
  result->size = 0;
  GetUniformiv cmd;
  // invalid location
  cmd.Init(client_program_id_, kInvalidUniformLocation,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformiv(_, _, _))
      .Times(0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformivBadSharedMemoryFails) {
  GetUniformiv cmd;
  cmd.Init(client_program_id_, kUniform2Location,
           kInvalidSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformiv(_, _, _))
      .Times(0);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, kUniform2Location,
           kSharedMemoryId, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
};

TEST_F(GLES2DecoderWithShaderTest, GetUniformfvSucceeds) {
  GetUniformfv::Result* result =
      static_cast<GetUniformfv::Result*>(shared_memory_address_);
  result->size = 0;
  GetUniformfv cmd;
  cmd.Init(client_program_id_, kUniform2Location,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformfv(kServiceProgramId, kUniform2Location, _))
      .Times(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GLES2Util::GetGLDataTypeSizeForUniforms(kUniform2Type),
            result->size);
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformfvArrayElementSucceeds) {
  GetUniformfv::Result* result =
      static_cast<GetUniformfv::Result*>(shared_memory_address_);
  result->size = 0;
  GetUniformfv cmd;
  cmd.Init(client_program_id_, kUniform2ElementLocation,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_,
              GetUniformfv(kServiceProgramId, kUniform2ElementLocation, _))
      .Times(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GLES2Util::GetGLDataTypeSizeForUniforms(kUniform2Type),
            result->size);
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformfvBadProgramFails) {
  GetUniformfv::Result* result =
      static_cast<GetUniformfv::Result*>(shared_memory_address_);
  result->size = 0;
  GetUniformfv cmd;
  // non-existant program
  cmd.Init(kInvalidClientId, kUniform2Location,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformfv(_, _, _))
      .Times(0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  // Valid id that is not a program. The GL spec requires a different error for
  // this case.
#if GLES2_TEST_SHADER_VS_PROGRAM_IDS
  result->size = kInitialResult;
  cmd.Init(client_shader_id_, kUniform2Location,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
#endif  // GLES2_TEST_SHADER_VS_PROGRAM_IDS
  // Unlinked program
  EXPECT_CALL(*gl_, CreateProgram())
      .Times(1)
      .WillOnce(Return(kNewServiceId))
      .RetiresOnSaturation();
  CreateProgram cmd2;
  cmd2.Init(kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  result->size = kInitialResult;
  cmd.Init(kNewClientId, kUniform2Location,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformfvBadLocationFails) {
  GetUniformfv::Result* result =
      static_cast<GetUniformfv::Result*>(shared_memory_address_);
  result->size = 0;
  GetUniformfv cmd;
  // invalid location
  cmd.Init(client_program_id_, kInvalidUniformLocation,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformfv(_, _, _))
      .Times(0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformfvBadSharedMemoryFails) {
  GetUniformfv cmd;
  cmd.Init(client_program_id_, kUniform2Location,
           kInvalidSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformfv(_, _, _))
      .Times(0);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, kUniform2Location,
           kSharedMemoryId, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
};

TEST_F(GLES2DecoderWithShaderTest, GetAttachedShadersSucceeds) {
  GetAttachedShaders cmd;
  typedef GetAttachedShaders::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->size = 0;
  EXPECT_CALL(*gl_, GetAttachedShaders(kServiceProgramId, 1, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(1),
                      SetArgumentPointee<3>(kServiceShaderId)));
  cmd.Init(client_program_id_, shared_memory_id_, shared_memory_offset_,
           Result::ComputeSize(1));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(1, result->GetNumResults());
  EXPECT_EQ(client_shader_id_, result->GetData()[0]);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetAttachedShadersResultNotInitFail) {
  GetAttachedShaders cmd;
  typedef GetAttachedShaders::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->size = 1;
  EXPECT_CALL(*gl_, GetAttachedShaders(_, _, _, _))
      .Times(0);
  cmd.Init(client_program_id_, shared_memory_id_, shared_memory_offset_,
           Result::ComputeSize(1));
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, GetAttachedShadersBadProgramFails) {
  GetAttachedShaders cmd;
  typedef GetAttachedShaders::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->size = 0;
  EXPECT_CALL(*gl_, GetAttachedShaders(_, _, _, _))
      .Times(0);
  cmd.Init(kInvalidClientId, shared_memory_id_, shared_memory_offset_,
           Result::ComputeSize(1));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetAttachedShadersBadSharedMemoryFails) {
  GetAttachedShaders cmd;
  typedef GetAttachedShaders::Result Result;
  cmd.Init(client_program_id_, kInvalidSharedMemoryId, shared_memory_offset_,
           Result::ComputeSize(1));
  EXPECT_CALL(*gl_, GetAttachedShaders(_, _, _, _))
      .Times(0);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, shared_memory_id_, kInvalidSharedMemoryOffset,
           Result::ComputeSize(1));
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, GetShaderPrecisionFormatSucceeds) {
  GetShaderPrecisionFormat cmd;
  typedef GetShaderPrecisionFormat::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->success = 0;
  // NOTE: GL will not be called. There is no equivalent Desktop OpenGL
  // function.
  cmd.Init(GL_VERTEX_SHADER, GL_HIGH_FLOAT,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_NE(0, result->success);
  EXPECT_EQ(-62, result->min_range);
  EXPECT_EQ(62, result->max_range);
  EXPECT_EQ(-16, result->precision);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetShaderPrecisionFormatResultNotInitFails) {
  GetShaderPrecisionFormat cmd;
  typedef GetShaderPrecisionFormat::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->success = 1;
  // NOTE: GL will not be called. There is no equivalent Desktop OpenGL
  // function.
  cmd.Init(GL_VERTEX_SHADER, GL_HIGH_FLOAT,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, GetShaderPrecisionFormatBadArgsFails) {
  typedef GetShaderPrecisionFormat::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->success = 0;
  GetShaderPrecisionFormat cmd;
  cmd.Init(GL_TEXTURE_2D, GL_HIGH_FLOAT,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  result->success = 0;
  cmd.Init(GL_VERTEX_SHADER, GL_TEXTURE_2D,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest,
       GetShaderPrecisionFormatBadSharedMemoryFails) {
  GetShaderPrecisionFormat cmd;
  cmd.Init(GL_VERTEX_SHADER, GL_HIGH_FLOAT,
           kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(GL_VERTEX_SHADER, GL_TEXTURE_2D,
           shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, GetActiveUniformSucceeds) {
  const GLuint kUniformIndex = 1;
  const uint32 kBucketId = 123;
  GetActiveUniform cmd;
  typedef GetActiveUniform::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->success = 0;
  cmd.Init(client_program_id_, kUniformIndex, kBucketId,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_NE(0, result->success);
  EXPECT_EQ(kUniform2Size, result->size);
  EXPECT_EQ(kUniform2Type, result->type);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  ASSERT_TRUE(bucket != NULL);
  EXPECT_EQ(0, memcmp(bucket->GetData(0, bucket->size()), kUniform2Name,
                      bucket->size()));
}

TEST_F(GLES2DecoderWithShaderTest, GetActiveUniformResultNotInitFails) {
  const GLuint kUniformIndex = 1;
  const uint32 kBucketId = 123;
  GetActiveUniform cmd;
  typedef GetActiveUniform::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->success = 1;
  cmd.Init(client_program_id_, kUniformIndex, kBucketId,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, GetActiveUniformBadProgramFails) {
  const GLuint kUniformIndex = 1;
  const uint32 kBucketId = 123;
  GetActiveUniform cmd;
  typedef GetActiveUniform::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->success = 0;
  cmd.Init(kInvalidClientId, kUniformIndex, kBucketId,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->success);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
#if GLES2_TEST_SHADER_VS_PROGRAM_IDS
  result->success = 0;
  cmd.Init(client_shader_id_, kUniformIndex, kBucketId,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->success);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
#endif  // GLES2_TEST_SHADER_VS_PROGRAM_IDS
}

TEST_F(GLES2DecoderWithShaderTest, GetActiveUniformBadIndexFails) {
  const uint32 kBucketId = 123;
  GetActiveUniform cmd;
  typedef GetActiveUniform::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->success = 0;
  cmd.Init(client_program_id_, kBadUniformIndex, kBucketId,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->success);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetActiveUniformBadSharedMemoryFails) {
  const GLuint kUniformIndex = 1;
  const uint32 kBucketId = 123;
  GetActiveUniform cmd;
  typedef GetActiveUniform::Result Result;
  cmd.Init(client_program_id_, kUniformIndex, kBucketId,
           kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, kUniformIndex, kBucketId,
           shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, GetActiveAttribSucceeds) {
  const GLuint kAttribIndex = 1;
  const uint32 kBucketId = 123;
  GetActiveAttrib cmd;
  typedef GetActiveAttrib::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->success = 0;
  cmd.Init(client_program_id_, kAttribIndex, kBucketId,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_NE(0, result->success);
  EXPECT_EQ(kAttrib2Size, result->size);
  EXPECT_EQ(kAttrib2Type, result->type);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  ASSERT_TRUE(bucket != NULL);
  EXPECT_EQ(0, memcmp(bucket->GetData(0, bucket->size()), kAttrib2Name,
                      bucket->size()));
}

TEST_F(GLES2DecoderWithShaderTest, GetActiveAttribResultNotInitFails) {
  const GLuint kAttribIndex = 1;
  const uint32 kBucketId = 123;
  GetActiveAttrib cmd;
  typedef GetActiveAttrib::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->success = 1;
  cmd.Init(client_program_id_, kAttribIndex, kBucketId,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, GetActiveAttribBadProgramFails) {
  const GLuint kAttribIndex = 1;
  const uint32 kBucketId = 123;
  GetActiveAttrib cmd;
  typedef GetActiveAttrib::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->success = 0;
  cmd.Init(kInvalidClientId, kAttribIndex, kBucketId,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->success);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
#if GLES2_TEST_SHADER_VS_PROGRAM_IDS
  result->success = 0;
  cmd.Init(client_shader_id_, kAttribIndex, kBucketId,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->success);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
#endif  // GLES2_TEST_SHADER_VS_PROGRAM_IDS
}

TEST_F(GLES2DecoderWithShaderTest, GetActiveAttribBadIndexFails) {
  const uint32 kBucketId = 123;
  GetActiveAttrib cmd;
  typedef GetActiveAttrib::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->success = 0;
  cmd.Init(client_program_id_, kBadAttribIndex, kBucketId,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->success);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetActiveAttribBadSharedMemoryFails) {
  const GLuint kAttribIndex = 1;
  const uint32 kBucketId = 123;
  GetActiveAttrib cmd;
  typedef GetActiveAttrib::Result Result;
  cmd.Init(client_program_id_, kAttribIndex, kBucketId,
           kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, kAttribIndex, kBucketId,
           shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, GetShaderInfoLogValidArgs) {
  const char* kInfo = "hello";
  const uint32 kBucketId = 123;
  CompileShader compile_cmd;
  GetShaderInfoLog cmd;
  EXPECT_CALL(*gl_, ShaderSource(kServiceShaderId, 1, _, _));
  EXPECT_CALL(*gl_, CompileShader(kServiceShaderId));
  EXPECT_CALL(*gl_, GetShaderiv(kServiceShaderId, GL_COMPILE_STATUS, _))
      .WillOnce(SetArgumentPointee<2>(GL_FALSE))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetShaderiv(kServiceShaderId, GL_INFO_LOG_LENGTH, _))
      .WillOnce(SetArgumentPointee<2>(strlen(kInfo) + 1))
      .RetiresOnSaturation();
  EXPECT_CALL(
      *gl_, GetShaderInfoLog(kServiceShaderId, strlen(kInfo) + 1, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(strlen(kInfo)),
                      SetArrayArgument<3>(kInfo, kInfo + strlen(kInfo) + 1)));
  compile_cmd.Init(client_shader_id_);
  cmd.Init(client_shader_id_, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(compile_cmd));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  ASSERT_TRUE(bucket != NULL);
  EXPECT_EQ(strlen(kInfo) + 1, bucket->size());
  EXPECT_EQ(0, memcmp(bucket->GetData(0, bucket->size()), kInfo,
                      bucket->size()));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetShaderInfoLogInvalidArgs) {
  const uint32 kBucketId = 123;
  GetShaderInfoLog cmd;
  cmd.Init(kInvalidClientId, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderTest, CompileShaderValidArgs) {
  EXPECT_CALL(*gl_, ShaderSource(kServiceShaderId, 1, _, _));
  EXPECT_CALL(*gl_, CompileShader(kServiceShaderId));
  EXPECT_CALL(*gl_, GetShaderiv(kServiceShaderId, GL_COMPILE_STATUS, _))
      .WillOnce(SetArgumentPointee<2>(GL_TRUE))
      .RetiresOnSaturation();
  CompileShader cmd;
  cmd.Init(client_shader_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, CompileShaderInvalidArgs) {
  CompileShader cmd;
  cmd.Init(kInvalidClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
#if GLES2_TEST_SHADER_VS_PROGRAM_IDS
  cmd.Init(client_program_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
#endif  // GLES2_TEST_SHADER_VS_PROGRAM_IDS
}

TEST_F(GLES2DecoderTest, ShaderSourceAndGetShaderSourceValidArgs) {
  const uint32 kBucketId = 123;
  const char kSource[] = "hello";
  const uint32 kSourceSize = sizeof(kSource) - 1;
  memcpy(shared_memory_address_, kSource, kSourceSize);
  ShaderSource cmd;
  cmd.Init(client_shader_id_,
           kSharedMemoryId, kSharedMemoryOffset, kSourceSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  memset(shared_memory_address_, 0, kSourceSize);
  GetShaderSource get_cmd;
  get_cmd.Init(client_shader_id_, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(get_cmd));
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  ASSERT_TRUE(bucket != NULL);
  EXPECT_EQ(kSourceSize + 1, bucket->size());
  EXPECT_EQ(0, memcmp(bucket->GetData(0, bucket->size()), kSource,
                      bucket->size()));
}

TEST_F(GLES2DecoderTest, ShaderSourceInvalidArgs) {
  const char kSource[] = "hello";
  const uint32 kSourceSize = sizeof(kSource) - 1;
  memcpy(shared_memory_address_, kSource, kSourceSize);
  ShaderSource cmd;
  cmd.Init(kInvalidClientId,
           kSharedMemoryId, kSharedMemoryOffset, kSourceSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
#if GLES2_TEST_SHADER_VS_PROGRAM_IDS
  cmd.Init(client_program_id_,
           kSharedMemoryId, kSharedMemoryOffset, kSourceSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
#endif  // GLES2_TEST_SHADER_VS_PROGRAM_IDS
  cmd.Init(client_shader_id_,
           kInvalidSharedMemoryId, kSharedMemoryOffset, kSourceSize);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_shader_id_,
           kSharedMemoryId, kInvalidSharedMemoryOffset, kSourceSize);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_shader_id_,
           kSharedMemoryId, kSharedMemoryOffset, kSharedBufferSize);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, ShaderSourceImmediateAndGetShaderSourceValidArgs) {
  const uint32 kBucketId = 123;
  const char kSource[] = "hello";
  const uint32 kSourceSize = sizeof(kSource) - 1;
  ShaderSourceImmediate& cmd = *GetImmediateAs<ShaderSourceImmediate>();
  cmd.Init(client_shader_id_, kSourceSize);
  memcpy(GetImmediateDataAs<void*>(&cmd), kSource, kSourceSize);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, kSourceSize));
  memset(shared_memory_address_, 0, kSourceSize);
  // TODO(gman): GetShaderSource has to change format so result is always set.
  GetShaderSource get_cmd;
  get_cmd.Init(client_shader_id_, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(get_cmd));
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  ASSERT_TRUE(bucket != NULL);
  EXPECT_EQ(kSourceSize + 1, bucket->size());
  EXPECT_EQ(0, memcmp(bucket->GetData(0, bucket->size()), kSource,
                      bucket->size()));
}

TEST_F(GLES2DecoderTest, ShaderSourceImmediateInvalidArgs) {
  const char kSource[] = "hello";
  const uint32 kSourceSize = sizeof(kSource) - 1;
  ShaderSourceImmediate& cmd = *GetImmediateAs<ShaderSourceImmediate>();
  cmd.Init(kInvalidClientId, kSourceSize);
  memcpy(GetImmediateDataAs<void*>(&cmd), kSource, kSourceSize);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, kSourceSize));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
#if GLES2_TEST_SHADER_VS_PROGRAM_IDS
  cmd.Init(client_program_id_, kSourceSize);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, kSourceSize));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
#endif  // GLES2_TEST_SHADER_VS_PROGRAM_IDS
}

TEST_F(GLES2DecoderTest, ShaderSourceBucketAndGetShaderSourceValidArgs) {
  const uint32 kInBucketId = 123;
  const uint32 kOutBucketId = 125;
  const char kSource[] = "hello";
  const uint32 kSourceSize = sizeof(kSource) - 1;
  SetBucketAsCString(kInBucketId, kSource);
  ShaderSourceBucket cmd;
  cmd.Init(client_shader_id_, kInBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  ClearSharedMemory();
  GetShaderSource get_cmd;
  get_cmd.Init(client_shader_id_, kOutBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(get_cmd));
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kOutBucketId);
  ASSERT_TRUE(bucket != NULL);
  EXPECT_EQ(kSourceSize + 1, bucket->size());
  EXPECT_EQ(0, memcmp(bucket->GetData(0, bucket->size()), kSource,
                      bucket->size()));
}

TEST_F(GLES2DecoderTest, ShaderSourceBucketInvalidArgs) {
  const uint32 kBucketId = 123;
  const char kSource[] = "hello";
  const uint32 kSourceSize = sizeof(kSource) - 1;
  memcpy(shared_memory_address_, kSource, kSourceSize);
  ShaderSourceBucket cmd;
  // Test no bucket.
  cmd.Init(client_texture_id_, kBucketId);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  // Test invalid client.
  SetBucketAsCString(kBucketId, kSource);
  cmd.Init(kInvalidClientId, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderTest, GenerateMipmapWrongFormatsFails) {
  EXPECT_CALL(*gl_, GenerateMipmapEXT(_))
       .Times(0);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 16, 17, 0, GL_RGBA, GL_UNSIGNED_BYTE,
      0, 0);
  GenerateMipmap cmd;
  cmd.Init(GL_TEXTURE_2D);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, Uniform1iValidArgs) {
  EXPECT_CALL(*gl_, Uniform1i(kUniform1Location, 2));
  SpecializedSetup<Uniform1i, 0>(true);
  Uniform1i cmd;
  cmd.Init(kUniform1Location, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, Uniform1ivValidArgs) {
  EXPECT_CALL(
      *gl_, Uniform1iv(
          kUniform1Location, 2,
          reinterpret_cast<const GLint*>(shared_memory_address_)));
  SpecializedSetup<Uniform1iv, 0>(true);
  Uniform1iv cmd;
  cmd.Init(kUniform1Location, 2, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, Uniform1ivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Uniform1iv(_, _, _)).Times(0);
  SpecializedSetup<Uniform1iv, 0>(false);
  Uniform1iv cmd;
  cmd.Init(kUniform1Location, 2, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, Uniform1ivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, Uniform1iv(_, _, _)).Times(0);
  SpecializedSetup<Uniform1iv, 0>(false);
  Uniform1iv cmd;
  cmd.Init(kUniform1Location, 2, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, Uniform1ivImmediateValidArgs) {
  Uniform1ivImmediate& cmd = *GetImmediateAs<Uniform1ivImmediate>();
  EXPECT_CALL(
      *gl_,
      Uniform1iv(kUniform1Location, 2,
          reinterpret_cast<GLint*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<Uniform1ivImmediate, 0>(true);
  GLint temp[1 * 2] = { 0, };
  cmd.Init(kUniform1Location, 2, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderWithShaderTest, BindBufferToDifferentTargetFails) {
  // Bind the buffer to GL_ARRAY_BUFFER
  DoBindBuffer(GL_ARRAY_BUFFER, client_buffer_id_, kServiceBufferId);
  // Attempt to rebind to GL_ELEMENT_ARRAY_BUFFER
  // NOTE: Real GLES2 does not have this restriction but WebGL and we do.
  // This can be restriction can be removed at runtime.
  EXPECT_CALL(*gl_, BindBuffer(_, _))
      .Times(0);
  BindBuffer cmd;
  cmd.Init(GL_ELEMENT_ARRAY_BUFFER, client_buffer_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderTest, ActiveTextureValidArgs) {
  EXPECT_CALL(*gl_, ActiveTexture(GL_TEXTURE1));
  SpecializedSetup<ActiveTexture, 0>(true);
  ActiveTexture cmd;
  cmd.Init(GL_TEXTURE1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest, ActiveTextureInvalidArgs) {
  EXPECT_CALL(*gl_, ActiveTexture(_)).Times(0);
  SpecializedSetup<ActiveTexture, 0>(false);
  ActiveTexture cmd;
  cmd.Init(GL_TEXTURE0 - 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  cmd.Init(kNumTextureUnits);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest, CheckFramebufferStatusWithNoBoundTarget) {
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(_))
      .Times(0);
  CheckFramebufferStatus::Result* result =
      static_cast<CheckFramebufferStatus::Result*>(shared_memory_address_);
  *result = 0;
  CheckFramebufferStatus cmd;
  cmd.Init(GL_FRAMEBUFFER, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE), *result);
}

TEST_F(GLES2DecoderTest, FramebufferRenderbufferWithNoBoundTarget) {
  EXPECT_CALL(*gl_, FramebufferRenderbufferEXT(_, _, _, _))
      .Times(0);
  FramebufferRenderbuffer cmd;
  cmd.Init(
    GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
    client_renderbuffer_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderTest, FramebufferTexture2DWithNoBoundTarget) {
  EXPECT_CALL(*gl_, FramebufferTexture2DEXT(_, _, _, _, _))
      .Times(0);
  FramebufferTexture2D cmd;
  cmd.Init(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, client_texture_id_,
      0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderTest, GetFramebufferAttachmentParameterivWithNoBoundTarget) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetFramebufferAttachmentParameterivEXT(_, _, _, _))
      .Times(0);
  GetFramebufferAttachmentParameteriv cmd;
  cmd.Init(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
      GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderTest, GetFramebufferAttachmentParameterivWithRenderbuffer) {
  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, FramebufferRenderbufferEXT(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
      kServiceRenderbufferId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  SetupExpectationsForFramebufferAttachment(
      GL_COLOR_BUFFER_BIT,    // clear bits
      0, 0, 0, 0,             // color
      0x1111,                 // color bits
      0,                      // stencil
      -1,                     // stencil mask back,
      -1,                     // stencil mask front,
      1.0f,                   // depth
      1,                      // depth mask
      false);                 // scissor test
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetFramebufferAttachmentParameterivEXT(
      GL_FRAMEBUFFER,
      GL_COLOR_ATTACHMENT0,
      GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, _))
      .WillOnce(SetArgumentPointee<3>(kServiceRenderbufferId))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetFramebufferAttachmentParameterivEXT(
      GL_FRAMEBUFFER,
      GL_COLOR_ATTACHMENT0,
      GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, _))
      .WillOnce(SetArgumentPointee<3>(GL_RENDERBUFFER))
      .RetiresOnSaturation();
  GetFramebufferAttachmentParameteriv::Result* result =
      static_cast<GetFramebufferAttachmentParameteriv::Result*>(
          shared_memory_address_);
  result->size = 0;
  const GLint* result_value = result->GetData();
  FramebufferRenderbuffer fbrb_cmd;
  GetFramebufferAttachmentParameteriv cmd;
  fbrb_cmd.Init(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
      client_renderbuffer_id_);
  cmd.Init(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
      GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(fbrb_cmd));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(static_cast<GLuint>(*result_value), client_renderbuffer_id_);
}

TEST_F(GLES2DecoderTest, GetFramebufferAttachmentParameterivWithTexture) {
  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, FramebufferTexture2DEXT(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
      kServiceTextureId, 0))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  SetupExpectationsForFramebufferAttachment(
      0,                      // clear bits
      0, 0, 0, 0,             // color
      0x1111,                 // color bits
      0,                      // stencil
      -1,                     // stencil mask back,
      -1,                     // stencil mask front,
      1.0f,                   // depth
      1,                      // depth mask
      false);                 // scissor test
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetFramebufferAttachmentParameterivEXT(
       GL_FRAMEBUFFER,
       GL_COLOR_ATTACHMENT0,
       GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, _))
      .WillOnce(SetArgumentPointee<3>(kServiceTextureId))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetFramebufferAttachmentParameterivEXT(
      GL_FRAMEBUFFER,
      GL_COLOR_ATTACHMENT0,
      GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, _))
      .WillOnce(SetArgumentPointee<3>(GL_TEXTURE))
      .RetiresOnSaturation();
  GetFramebufferAttachmentParameteriv::Result* result =
      static_cast<GetFramebufferAttachmentParameteriv::Result*>(
          shared_memory_address_);
  result->SetNumResults(0);
  const GLint* result_value = result->GetData();
  FramebufferTexture2D fbtex_cmd;
  GetFramebufferAttachmentParameteriv cmd;
  fbtex_cmd.Init(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, client_texture_id_,
      0);
  cmd.Init(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
      GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(fbtex_cmd));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(static_cast<GLuint>(*result_value), client_texture_id_);
}

TEST_F(GLES2DecoderTest, GetRenderbufferParameterivWithNoBoundTarget) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetRenderbufferParameterivEXT(_, _, _))
      .Times(0);
  GetRenderbufferParameteriv cmd;
  cmd.Init(
      GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderTest, RenderbufferStorageWithNoBoundTarget) {
  EXPECT_CALL(*gl_, RenderbufferStorageEXT(_, _, _, _))
      .Times(0);
  RenderbufferStorage cmd;
  cmd.Init(GL_RENDERBUFFER, GL_RGBA4, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

namespace {

// A class to emulate glReadPixels
class ReadPixelsEmulator {
 public:
  // pack_alignment is the alignment you want ReadPixels to use
  // when copying. The actual data passed in pixels should be contiguous.
  ReadPixelsEmulator(GLsizei width, GLsizei height, GLint bytes_per_pixel,
                     const void* pixels, GLint pack_alignment)
      : width_(width),
        height_(height),
        pack_alignment_(pack_alignment),
        bytes_per_pixel_(bytes_per_pixel),
        pixels_(reinterpret_cast<const int8*>(pixels)) {
  }

  void ReadPixels(
      GLint x, GLint y, GLsizei width, GLsizei height,
      GLenum format, GLenum type, void* pixels) const {
    DCHECK_GE(x, 0);
    DCHECK_GE(y, 0);
    DCHECK_LE(x + width, width_);
    DCHECK_LE(y + height, height_);
    for (GLint yy = 0; yy < height; ++yy) {
      const int8* src = GetPixelAddress(x, y + yy);
      const void* dst = ComputePackAlignmentAddress(0, yy, width, pixels);
      memcpy(const_cast<void*>(dst), src, width * bytes_per_pixel_);
    }
  }

  bool CompareRowSegment(
      GLint x, GLint y, GLsizei width, const void* data) const {
    DCHECK(x + width <= width_ || width == 0);
    return memcmp(data, GetPixelAddress(x, y), width * bytes_per_pixel_) == 0;
  }

  // Helper to compute address of pixel in pack aligned data.
  const void* ComputePackAlignmentAddress(
      GLint x, GLint y, GLsizei width, const void* address) const {
    GLint unpadded_row_size = ComputeImageDataSize(width, 1);
    GLint two_rows_size = ComputeImageDataSize(width, 2);
    GLsizei padded_row_size = two_rows_size - unpadded_row_size;
    GLint offset = y * padded_row_size + x * bytes_per_pixel_;
    return static_cast<const int8*>(address) + offset;
  }

  GLint ComputeImageDataSize(GLint width, GLint height) const {
    GLint row_size = width * bytes_per_pixel_;
    if (height > 1) {
      GLint temp = row_size + pack_alignment_;
      GLint padded_row_size = (temp / pack_alignment_) * pack_alignment_;
      GLint size_of_all_but_last_row = (height - 1) * padded_row_size;
      return size_of_all_but_last_row + row_size;
    } else {
      return height * row_size;
    }
  }

 private:
  const int8* GetPixelAddress(GLint x, GLint y) const {
    return pixels_ + (width_ * y + x) * bytes_per_pixel_;
  }

  GLsizei width_;
  GLsizei height_;
  GLint pack_alignment_;
  GLint bytes_per_pixel_;
  const int8* pixels_;
};

}  // anonymous namespace

void GLES2DecoderTest::CheckReadPixelsOutOfRange(
    GLint in_read_x, GLint in_read_y,
    GLsizei in_read_width, GLsizei in_read_height,
    bool init) {
  const GLsizei kWidth = 5;
  const GLsizei kHeight = 3;
  const GLint kBytesPerPixel = 3;
  const GLint kPackAlignment = 4;
  const GLenum kFormat = GL_RGB;
  static const int8 kSrcPixels[kWidth * kHeight * kBytesPerPixel] = {
    12, 13, 14, 18, 19, 18, 19, 12, 13, 14, 18, 19, 18, 19, 13,
    29, 28, 23, 22, 21, 22, 21, 29, 28, 23, 22, 21, 22, 21, 28,
    31, 34, 39, 37, 32, 37, 32, 31, 34, 39, 37, 32, 37, 32, 34,
  };

  ClearSharedMemory();

  // We need to setup an FBO so we can know the max size that ReadPixels will
  // access
  if (init) {
    DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
    DoTexImage2D(GL_TEXTURE_2D, 0, kFormat, kWidth, kHeight, 0,
                 kFormat, GL_UNSIGNED_BYTE, 0, 0);
    DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                      kServiceFramebufferId);
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, FramebufferTexture2DEXT(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
        kServiceTextureId, 0))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
    SetupExpectationsForFramebufferAttachment(
        0,                      // clear bits
        0, 0, 0, 0,             // color
        0x1111,                 // color bits
        0,                      // stencil
        -1,                     // stencil mask back,
        -1,                     // stencil mask front,
        1.0f,                   // depth
        1,                      // depth mask
        false);                 // scissor test
    FramebufferTexture2D fbtex_cmd;
    fbtex_cmd.Init(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, client_texture_id_,
        0);
    EXPECT_EQ(error::kNoError, ExecuteCmd(fbtex_cmd));
  }

  ReadPixelsEmulator emu(
      kWidth, kHeight, kBytesPerPixel, kSrcPixels, kPackAlignment);
  typedef ReadPixels::Result Result;
  Result* result = GetSharedMemoryAs<Result*>();
  uint32 result_shm_id = kSharedMemoryId;
  uint32 result_shm_offset = kSharedMemoryOffset;
  uint32 pixels_shm_id = kSharedMemoryId;
  uint32 pixels_shm_offset = kSharedMemoryOffset + sizeof(*result);
  void* dest = &result[1];
  EXPECT_CALL(*gl_, GetError())
     .WillOnce(Return(GL_NO_ERROR))
     .WillOnce(Return(GL_NO_ERROR))
     .RetiresOnSaturation();
  // ReadPixels will be called for valid size only even though the command
  // is requesting a larger size.
  GLint read_x = std::max(0, in_read_x);
  GLint read_y = std::max(0, in_read_y);
  GLint read_end_x = std::max(0, std::min(kWidth, in_read_x + in_read_width));
  GLint read_end_y = std::max(0, std::min(kHeight, in_read_y + in_read_height));
  GLint read_width = read_end_x - read_x;
  GLint read_height = read_end_y - read_y;
  if (read_width > 0 && read_height > 0) {
    for (GLint yy = read_y; yy < read_end_y; ++yy) {
      EXPECT_CALL(
          *gl_, ReadPixels(read_x, yy, read_width, 1,
                           kFormat, GL_UNSIGNED_BYTE, _))
          .WillOnce(Invoke(&emu, &ReadPixelsEmulator::ReadPixels))
          .RetiresOnSaturation();
    }
  }
  ReadPixels cmd;
  cmd.Init(in_read_x, in_read_y, in_read_width, in_read_height,
           kFormat, GL_UNSIGNED_BYTE,
           pixels_shm_id, pixels_shm_offset,
           result_shm_id, result_shm_offset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

  GLint unpadded_row_size = emu.ComputeImageDataSize(in_read_width, 1);
  scoped_array<int8> zero(new int8[unpadded_row_size]);
  scoped_array<int8> pack(new int8[kPackAlignment]);
  memset(zero.get(), 0, unpadded_row_size);
  memset(pack.get(), kInitialMemoryValue, kPackAlignment);
  for (GLint yy = 0; yy < in_read_height; ++yy) {
    const int8* row = static_cast<const int8*>(
        emu.ComputePackAlignmentAddress(0, yy, in_read_width, dest));
    GLint y = in_read_y + yy;
    if (y < 0 || y >= kHeight) {
      EXPECT_EQ(0, memcmp(zero.get(), row, unpadded_row_size));
    } else {
      // check off left.
      GLint num_left_pixels = std::max(-in_read_x, 0);
      GLint num_left_bytes = num_left_pixels * kBytesPerPixel;
      EXPECT_EQ(0, memcmp(zero.get(), row, num_left_bytes));

      // check off right.
      GLint num_right_pixels = std::max(in_read_x + in_read_width - kWidth, 0);
      GLint num_right_bytes = num_right_pixels * kBytesPerPixel;
      EXPECT_EQ(0, memcmp(zero.get(),
                            row + unpadded_row_size - num_right_bytes,
                            num_right_bytes));

      // check middle.
      GLint x = std::max(in_read_x, 0);
      GLint num_middle_pixels =
          std::max(in_read_width - num_left_pixels - num_right_pixels, 0);
      EXPECT_TRUE(emu.CompareRowSegment(
          x, y, num_middle_pixels, row + num_left_bytes));
    }

    // check padding
    if (yy != in_read_height - 1) {
      GLint num_padding_bytes =
          (kPackAlignment - 1) - (unpadded_row_size % kPackAlignment);
      EXPECT_EQ(0,
                memcmp(pack.get(), row + unpadded_row_size, num_padding_bytes));
    }
  }
}

TEST_F(GLES2DecoderTest, ReadPixels) {
  const GLsizei kWidth = 5;
  const GLsizei kHeight = 3;
  const GLint kBytesPerPixel = 3;
  const GLint kPackAlignment = 4;
  static const int8 kSrcPixels[kWidth * kHeight * kBytesPerPixel] = {
    12, 13, 14, 18, 19, 18, 19, 12, 13, 14, 18, 19, 18, 19, 13,
    29, 28, 23, 22, 21, 22, 21, 29, 28, 23, 22, 21, 22, 21, 28,
    31, 34, 39, 37, 32, 37, 32, 31, 34, 39, 37, 32, 37, 32, 34,
  };

  context_->SetSize(gfx::Size(INT_MAX, INT_MAX));

  ReadPixelsEmulator emu(
      kWidth, kHeight, kBytesPerPixel, kSrcPixels, kPackAlignment);
  typedef ReadPixels::Result Result;
  Result* result = GetSharedMemoryAs<Result*>();
  uint32 result_shm_id = kSharedMemoryId;
  uint32 result_shm_offset = kSharedMemoryOffset;
  uint32 pixels_shm_id = kSharedMemoryId;
  uint32 pixels_shm_offset = kSharedMemoryOffset + sizeof(*result);
  void* dest = &result[1];
  EXPECT_CALL(*gl_, GetError())
     .WillOnce(Return(GL_NO_ERROR))
     .WillOnce(Return(GL_NO_ERROR))
     .RetiresOnSaturation();
  EXPECT_CALL(
      *gl_, ReadPixels(0, 0, kWidth, kHeight, GL_RGB, GL_UNSIGNED_BYTE, _))
      .WillOnce(Invoke(&emu, &ReadPixelsEmulator::ReadPixels));
  ReadPixels cmd;
  cmd.Init(0, 0, kWidth, kHeight, GL_RGB, GL_UNSIGNED_BYTE,
           pixels_shm_id, pixels_shm_offset,
           result_shm_id, result_shm_offset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  for (GLint yy = 0; yy < kHeight; ++yy) {
    EXPECT_TRUE(emu.CompareRowSegment(
        0, yy, kWidth,
        emu.ComputePackAlignmentAddress(0, yy, kWidth, dest)));
  }
}

TEST_F(GLES2DecoderTest, ReadPixelsOutOfRange) {
  static GLint tests[][4] = {
    { -2, -1, 9, 5, },  // out of range on all sides
    { 2, 1, 9, 5, },  // out of range on right, bottom
    { -7, -4, 9, 5, },  // out of range on left, top
    { 0, -5, 9, 5, },  // completely off top
    { 0, 3, 9, 5, },  // completely off bottom
    { -9, 0, 9, 5, },  // completely off left
    { 5, 0, 9, 5, },  // completely off right
  };

  for (size_t tt = 0; tt < arraysize(tests); ++tt) {
    CheckReadPixelsOutOfRange(
        tests[tt][0], tests[tt][1], tests[tt][2], tests[tt][3], tt == 0);
  }
}

TEST_F(GLES2DecoderTest, ReadPixelsInvalidArgs) {
  typedef ReadPixels::Result Result;
  Result* result = GetSharedMemoryAs<Result*>();
  uint32 result_shm_id = kSharedMemoryId;
  uint32 result_shm_offset = kSharedMemoryOffset;
  uint32 pixels_shm_id = kSharedMemoryId;
  uint32 pixels_shm_offset = kSharedMemoryOffset + sizeof(*result);
  EXPECT_CALL(*gl_, ReadPixels(_, _, _, _, _, _, _)).Times(0);
  ReadPixels cmd;
  cmd.Init(0, 0, -1, 1, GL_RGB, GL_UNSIGNED_BYTE,
           pixels_shm_id, pixels_shm_offset,
           result_shm_id, result_shm_offset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(0, 0, 1, -1, GL_RGB, GL_UNSIGNED_BYTE,
           pixels_shm_id, pixels_shm_offset,
           result_shm_id, result_shm_offset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(0, 0, 1, 1, GL_RGB, GL_INT,
           pixels_shm_id, pixels_shm_offset,
           result_shm_id, result_shm_offset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  cmd.Init(0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE,
           kInvalidSharedMemoryId, pixels_shm_offset,
           result_shm_id, result_shm_offset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE,
           pixels_shm_id, kInvalidSharedMemoryOffset,
           result_shm_id, result_shm_offset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE,
           pixels_shm_id, pixels_shm_offset,
           kInvalidSharedMemoryId, result_shm_offset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE,
           pixels_shm_id, pixels_shm_offset,
           result_shm_id, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, BindAttribLocation) {
  const GLint kLocation = 2;
  const char* kName = "testing";
  const uint32 kNameSize = strlen(kName);
  EXPECT_CALL(
      *gl_, BindAttribLocation(kServiceProgramId, kLocation, StrEq(kName)))
      .Times(1);
  memcpy(shared_memory_address_, kName, kNameSize);
  BindAttribLocation cmd;
  cmd.Init(client_program_id_, kLocation, kSharedMemoryId, kSharedMemoryOffset,
           kNameSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, BindAttribLocationInvalidArgs) {
  const GLint kLocation = 2;
  const char* kName = "testing";
  const uint32 kNameSize = strlen(kName);
  EXPECT_CALL(*gl_, BindAttribLocation(_, _, _)).Times(0);
  BindAttribLocation cmd;
  cmd.Init(kInvalidClientId, kLocation,
           kSharedMemoryId, kSharedMemoryOffset, kNameSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(client_program_id_, kLocation,
           kInvalidSharedMemoryId, kSharedMemoryOffset, kNameSize);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, kLocation,
           kSharedMemoryId, kInvalidSharedMemoryOffset, kNameSize);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, kLocation,
           kSharedMemoryId, kSharedMemoryOffset, kSharedBufferSize);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, BindAttribLocationImmediate) {
  const GLint kLocation = 2;
  const char* kName = "testing";
  const uint32 kNameSize = strlen(kName);
  EXPECT_CALL(
      *gl_, BindAttribLocation(kServiceProgramId, kLocation, StrEq(kName)))
      .Times(1);
  BindAttribLocationImmediate& cmd =
      *GetImmediateAs<BindAttribLocationImmediate>();
  cmd.Init(client_program_id_, kLocation, kName, kNameSize);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, kNameSize));
}

TEST_F(GLES2DecoderTest, BindAttribLocationImmediateInvalidArgs) {
  const GLint kLocation = 2;
  const char* kName = "testing";
  const uint32 kNameSize = strlen(kName);
  EXPECT_CALL(*gl_, BindAttribLocation(_, _, _)).Times(0);
  BindAttribLocationImmediate& cmd =
      *GetImmediateAs<BindAttribLocationImmediate>();
  cmd.Init(kInvalidClientId, kLocation, kName, kNameSize);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, kNameSize));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderTest, BindAttribLocationBucket) {
  const uint32 kBucketId = 123;
  const GLint kLocation = 2;
  const char* kName = "testing";
  EXPECT_CALL(
      *gl_, BindAttribLocation(kServiceProgramId, kLocation, StrEq(kName)))
      .Times(1);
  SetBucketAsCString(kBucketId, kName);
  BindAttribLocationBucket cmd;
  cmd.Init(client_program_id_, kLocation, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, BindAttribLocationBucketInvalidArgs) {
  const uint32 kBucketId = 123;
  const GLint kLocation = 2;
  const char* kName = "testing";
  EXPECT_CALL(*gl_, BindAttribLocation(_, _, _)).Times(0);
  BindAttribLocationBucket cmd;
  // check bucket does not exist.
  cmd.Init(client_program_id_, kLocation, kBucketId);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  // check bucket is empty.
  SetBucketAsCString(kBucketId, NULL);
  cmd.Init(client_program_id_, kLocation, kBucketId);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  // Check bad program id
  SetBucketAsCString(kBucketId, kName);
  cmd.Init(kInvalidClientId, kLocation, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetAttribLocation) {
  const uint32 kNameSize = strlen(kAttrib2Name);
  const char* kNonExistentName = "foobar";
  const uint32 kNonExistentNameSize = strlen(kNonExistentName);
  typedef GetAttribLocation::Result Result;
  Result* result = GetSharedMemoryAs<Result*>();
  *result = -1;
  char* name = GetSharedMemoryAsWithOffset<char*>(sizeof(*result));
  const uint32 kNameOffset = kSharedMemoryOffset + sizeof(*result);
  memcpy(name, kAttrib2Name, kNameSize);
  GetAttribLocation cmd;
  cmd.Init(client_program_id_,
           kSharedMemoryId, kNameOffset,
           kSharedMemoryId, kSharedMemoryOffset,
           kNameSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(kAttrib2Location, *result);
  *result = -1;
  memcpy(name, kNonExistentName, kNonExistentNameSize);
  cmd.Init(client_program_id_,
           kSharedMemoryId, kNameOffset,
           kSharedMemoryId, kSharedMemoryOffset,
           kNonExistentNameSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(-1, *result);
}

TEST_F(GLES2DecoderWithShaderTest, GetAttribLocationInvalidArgs) {
  const uint32 kNameSize = strlen(kAttrib2Name);
  typedef GetAttribLocation::Result Result;
  Result* result = GetSharedMemoryAs<Result*>();
  *result = -1;
  char* name = GetSharedMemoryAsWithOffset<char*>(sizeof(*result));
  const uint32 kNameOffset = kSharedMemoryOffset + sizeof(*result);
  memcpy(name, kAttrib2Name, kNameSize);
  GetAttribLocation cmd;
  cmd.Init(kInvalidClientId,
           kSharedMemoryId, kNameOffset,
           kSharedMemoryId, kSharedMemoryOffset,
           kNameSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(-1, *result);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  *result = -1;
  cmd.Init(client_program_id_,
           kInvalidSharedMemoryId, kNameOffset,
           kSharedMemoryId, kSharedMemoryOffset,
           kNameSize);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(-1, *result);
  cmd.Init(client_program_id_,
           kSharedMemoryId, kInvalidSharedMemoryOffset,
           kSharedMemoryId, kSharedMemoryOffset,
           kNameSize);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(-1, *result);
  cmd.Init(client_program_id_,
           kSharedMemoryId, kNameOffset,
           kInvalidSharedMemoryId, kSharedMemoryOffset,
           kNameSize);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(-1, *result);
  cmd.Init(client_program_id_,
           kSharedMemoryId, kNameOffset,
           kSharedMemoryId, kInvalidSharedMemoryOffset,
           kNameSize);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(-1, *result);
  cmd.Init(client_program_id_,
           kSharedMemoryId, kNameOffset,
           kSharedMemoryId, kSharedMemoryOffset,
           kSharedBufferSize);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(-1, *result);
}

TEST_F(GLES2DecoderWithShaderTest, GetAttribLocationImmediate) {
  const uint32 kNameSize = strlen(kAttrib2Name);
  const char* kNonExistentName = "foobar";
  const uint32 kNonExistentNameSize = strlen(kNonExistentName);
  typedef GetAttribLocationImmediate::Result Result;
  Result* result = GetSharedMemoryAs<Result*>();
  *result = -1;
  GetAttribLocationImmediate& cmd =
      *GetImmediateAs<GetAttribLocationImmediate>();
  cmd.Init(client_program_id_, kAttrib2Name,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, kNameSize));
  EXPECT_EQ(kAttrib2Location, *result);
  *result = -1;
  cmd.Init(client_program_id_, kNonExistentName,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, kNonExistentNameSize));
  EXPECT_EQ(-1, *result);
}

TEST_F(GLES2DecoderWithShaderTest, GetAttribLocationImmediateInvalidArgs) {
  const uint32 kNameSize = strlen(kAttrib2Name);
  typedef GetAttribLocationImmediate::Result Result;
  Result* result = GetSharedMemoryAs<Result*>();
  *result = -1;
  GetAttribLocationImmediate& cmd =
      *GetImmediateAs<GetAttribLocationImmediate>();
  cmd.Init(kInvalidClientId, kAttrib2Name,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, kNameSize));
  EXPECT_EQ(-1, *result);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  *result = -1;
  cmd.Init(client_program_id_, kAttrib2Name,
           kInvalidSharedMemoryId, kSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteImmediateCmd(cmd, kNameSize));
  EXPECT_EQ(-1, *result);
  cmd.Init(client_program_id_, kAttrib2Name,
           kSharedMemoryId, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteImmediateCmd(cmd, kNameSize));
  EXPECT_EQ(-1, *result);
}

TEST_F(GLES2DecoderWithShaderTest, GetAttribLocationBucket) {
  const uint32 kBucketId = 123;
  const char* kNonExistentName = "foobar";
  typedef GetAttribLocationBucket::Result Result;
  Result* result = GetSharedMemoryAs<Result*>();
  SetBucketAsCString(kBucketId, kAttrib2Name);
  *result = -1;
  GetAttribLocationBucket cmd;
  cmd.Init(client_program_id_, kBucketId,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(kAttrib2Location, *result);
  SetBucketAsCString(kBucketId, kNonExistentName);
  *result = -1;
  cmd.Init(client_program_id_, kBucketId,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(-1, *result);
}

TEST_F(GLES2DecoderWithShaderTest, GetAttribLocationBucketInvalidArgs) {
  const uint32 kBucketId = 123;
  typedef GetAttribLocationBucket::Result Result;
  Result* result = GetSharedMemoryAs<Result*>();
  *result = -1;
  GetAttribLocationBucket cmd;
  // Check no bucket
  cmd.Init(client_program_id_, kBucketId,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(-1, *result);
  // Check bad program id.
  SetBucketAsCString(kBucketId, kAttrib2Name);
  cmd.Init(kInvalidClientId, kBucketId,
           kSharedMemoryId, kSharedMemoryOffset);
  *result = -1;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(-1, *result);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  // Check bad memory
  cmd.Init(client_program_id_, kBucketId,
           kInvalidSharedMemoryId, kSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, kBucketId,
           kSharedMemoryId, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformLocation) {
  const uint32 kNameSize = strlen(kUniform2Name);
  const char* kNonExistentName = "foobar";
  const uint32 kNonExistentNameSize = strlen(kNonExistentName);
  typedef GetUniformLocation::Result Result;
  Result* result = GetSharedMemoryAs<Result*>();
  *result = -1;
  char* name = GetSharedMemoryAsWithOffset<char*>(sizeof(*result));
  const uint32 kNameOffset = kSharedMemoryOffset + sizeof(*result);
  memcpy(name, kUniform2Name, kNameSize);
  GetUniformLocation cmd;
  cmd.Init(client_program_id_,
           kSharedMemoryId, kNameOffset,
           kSharedMemoryId, kSharedMemoryOffset,
           kNameSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(kUniform2Location, *result);
  memcpy(name, kNonExistentName, kNonExistentNameSize);
  *result = -1;
  cmd.Init(client_program_id_,
           kSharedMemoryId, kNameOffset,
           kSharedMemoryId, kSharedMemoryOffset,
           kNonExistentNameSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(-1, *result);
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformLocationInvalidArgs) {
  const uint32 kNameSize = strlen(kUniform2Name);
  typedef GetUniformLocation::Result Result;
  Result* result = GetSharedMemoryAs<Result*>();
  *result = -1;
  char* name = GetSharedMemoryAsWithOffset<char*>(sizeof(*result));
  const uint32 kNameOffset = kSharedMemoryOffset + sizeof(*result);
  memcpy(name, kUniform2Name, kNameSize);
  GetUniformLocation cmd;
  cmd.Init(kInvalidClientId,
           kSharedMemoryId, kNameOffset,
           kSharedMemoryId, kSharedMemoryOffset,
           kNameSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(-1, *result);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  *result = -1;
  cmd.Init(client_program_id_,
           kInvalidSharedMemoryId, kNameOffset,
           kSharedMemoryId, kSharedMemoryOffset,
           kNameSize);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(-1, *result);
  cmd.Init(client_program_id_,
           kSharedMemoryId, kInvalidSharedMemoryOffset,
           kSharedMemoryId, kSharedMemoryOffset,
           kNameSize);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(-1, *result);
  cmd.Init(client_program_id_,
           kSharedMemoryId, kNameOffset,
           kInvalidSharedMemoryId, kSharedMemoryOffset,
           kNameSize);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(-1, *result);
  cmd.Init(client_program_id_,
           kSharedMemoryId, kNameOffset,
           kSharedMemoryId, kInvalidSharedMemoryOffset,
           kNameSize);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(-1, *result);
  cmd.Init(client_program_id_,
           kSharedMemoryId, kNameOffset,
           kSharedMemoryId, kSharedMemoryOffset,
           kSharedBufferSize);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(-1, *result);
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformLocationImmediate) {
  const uint32 kNameSize = strlen(kUniform2Name);
  const char* kNonExistentName = "foobar";
  const uint32 kNonExistentNameSize = strlen(kNonExistentName);
  typedef GetUniformLocationImmediate::Result Result;
  Result* result = GetSharedMemoryAs<Result*>();
  *result = -1;
  GetUniformLocationImmediate& cmd =
      *GetImmediateAs<GetUniformLocationImmediate>();
  cmd.Init(client_program_id_, kUniform2Name,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, kNameSize));
  EXPECT_EQ(kUniform2Location, *result);
  *result = -1;
  cmd.Init(client_program_id_, kNonExistentName,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, kNonExistentNameSize));
  EXPECT_EQ(-1, *result);
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformLocationImmediateInvalidArgs) {
  const uint32 kNameSize = strlen(kUniform2Name);
  typedef GetUniformLocationImmediate::Result Result;
  Result* result = GetSharedMemoryAs<Result*>();
  *result = -1;
  GetUniformLocationImmediate& cmd =
      *GetImmediateAs<GetUniformLocationImmediate>();
  cmd.Init(kInvalidClientId, kUniform2Name,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, kNameSize));
  EXPECT_EQ(-1, *result);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  *result = -1;
  cmd.Init(client_program_id_, kUniform2Name,
           kInvalidSharedMemoryId, kSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteImmediateCmd(cmd, kNameSize));
  EXPECT_EQ(-1, *result);
  cmd.Init(client_program_id_, kUniform2Name,
           kSharedMemoryId, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteImmediateCmd(cmd, kNameSize));
  EXPECT_EQ(-1, *result);
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformLocationBucket) {
  const uint32 kBucketId = 123;
  const char* kNonExistentName = "foobar";
  typedef GetUniformLocationBucket::Result Result;
  Result* result = GetSharedMemoryAs<Result*>();
  SetBucketAsCString(kBucketId, kUniform2Name);
  *result = -1;
  GetUniformLocationBucket cmd;
  cmd.Init(client_program_id_, kBucketId,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(kUniform2Location, *result);
  SetBucketAsCString(kBucketId, kNonExistentName);
  *result = -1;
  cmd.Init(client_program_id_, kBucketId,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(-1, *result);
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformLocationBucketInvalidArgs) {
  const uint32 kBucketId = 123;
  typedef GetUniformLocationBucket::Result Result;
  Result* result = GetSharedMemoryAs<Result*>();
  *result = -1;
  GetUniformLocationBucket cmd;
  // Check no bucket
  cmd.Init(client_program_id_, kBucketId,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(-1, *result);
  // Check bad program id.
  SetBucketAsCString(kBucketId, kUniform2Name);
  cmd.Init(kInvalidClientId, kBucketId,
           kSharedMemoryId, kSharedMemoryOffset);
  *result = -1;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(-1, *result);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  // Check bad memory
  cmd.Init(client_program_id_, kBucketId,
           kInvalidSharedMemoryId, kSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, kBucketId,
           kSharedMemoryId, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, GetMaxValueInBufferCHROMIUM) {
  SetupIndexBuffer();
  GetMaxValueInBufferCHROMIUM::Result* result =
      static_cast<GetMaxValueInBufferCHROMIUM::Result*>(shared_memory_address_);
  *result = 0;

  GetMaxValueInBufferCHROMIUM cmd;
  cmd.Init(client_element_buffer_id_, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2, kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(7u, *result);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  cmd.Init(client_element_buffer_id_, kValidIndexRangeCount + 1,
           GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2, kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(100u, *result);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  cmd.Init(kInvalidClientId, kValidIndexRangeCount,
           GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2, kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(client_element_buffer_id_, kOutOfRangeIndexRangeEnd,
           GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2, kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  cmd.Init(client_element_buffer_id_, kValidIndexRangeCount + 1,
           GL_UNSIGNED_SHORT,
           kOutOfRangeIndexRangeEnd * 2, kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  cmd.Init(client_element_buffer_id_, kValidIndexRangeCount + 1,
           GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2, kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_buffer_id_, kValidIndexRangeCount + 1,
           GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2, kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  cmd.Init(client_element_buffer_id_, kValidIndexRangeCount + 1,
           GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2,
           kInvalidSharedMemoryId, kSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_element_buffer_id_, kValidIndexRangeCount + 1,
           GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2,
           kSharedMemoryId, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, SharedIds) {
  GenSharedIdsCHROMIUM gen_cmd;
  RegisterSharedIdsCHROMIUM reg_cmd;
  DeleteSharedIdsCHROMIUM del_cmd;

  const GLuint kNamespaceId = id_namespaces::kTextures;
  const GLuint kExpectedId1 = 1;
  const GLuint kExpectedId2 = 2;
  const GLuint kExpectedId3 = 4;
  const GLuint kRegisterId = 3;
  GLuint* ids = GetSharedMemoryAs<GLuint*>();
  gen_cmd.Init(kNamespaceId, 0, 2, kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(gen_cmd));
  IdAllocator* id_allocator = GetIdAllocator(kNamespaceId);
  ASSERT_TRUE(id_allocator != NULL);
  // This check is implementation dependant but it's kind of hard to check
  // otherwise.
  EXPECT_EQ(kExpectedId1, ids[0]);
  EXPECT_EQ(kExpectedId2, ids[1]);
  EXPECT_TRUE(id_allocator->InUse(kExpectedId1));
  EXPECT_TRUE(id_allocator->InUse(kExpectedId2));
  EXPECT_FALSE(id_allocator->InUse(kRegisterId));
  EXPECT_FALSE(id_allocator->InUse(kExpectedId3));

  ClearSharedMemory();
  ids[0] = kRegisterId;
  reg_cmd.Init(kNamespaceId, 1, kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(reg_cmd));
  EXPECT_TRUE(id_allocator->InUse(kExpectedId1));
  EXPECT_TRUE(id_allocator->InUse(kExpectedId2));
  EXPECT_TRUE(id_allocator->InUse(kRegisterId));
  EXPECT_FALSE(id_allocator->InUse(kExpectedId3));

  ClearSharedMemory();
  gen_cmd.Init(kNamespaceId, 0, 1, kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(gen_cmd));
  EXPECT_EQ(kExpectedId3, ids[0]);
  EXPECT_TRUE(id_allocator->InUse(kExpectedId1));
  EXPECT_TRUE(id_allocator->InUse(kExpectedId2));
  EXPECT_TRUE(id_allocator->InUse(kRegisterId));
  EXPECT_TRUE(id_allocator->InUse(kExpectedId3));

  ClearSharedMemory();
  ids[0] = kExpectedId1;
  ids[1] = kRegisterId;
  del_cmd.Init(kNamespaceId, 2, kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(del_cmd));
  EXPECT_FALSE(id_allocator->InUse(kExpectedId1));
  EXPECT_TRUE(id_allocator->InUse(kExpectedId2));
  EXPECT_FALSE(id_allocator->InUse(kRegisterId));
  EXPECT_TRUE(id_allocator->InUse(kExpectedId3));

  ClearSharedMemory();
  ids[0] = kExpectedId3;
  ids[1] = kExpectedId2;
  del_cmd.Init(kNamespaceId, 2, kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(del_cmd));
  EXPECT_FALSE(id_allocator->InUse(kExpectedId1));
  EXPECT_FALSE(id_allocator->InUse(kExpectedId2));
  EXPECT_FALSE(id_allocator->InUse(kRegisterId));
  EXPECT_FALSE(id_allocator->InUse(kExpectedId3));

  // Check passing in an id_offset.
  ClearSharedMemory();
  const GLuint kOffset = 0xABCDEF;
  gen_cmd.Init(kNamespaceId, kOffset, 2, kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(gen_cmd));
  EXPECT_EQ(kOffset, ids[0]);
  EXPECT_EQ(kOffset + 1, ids[1]);
}

TEST_F(GLES2DecoderTest, GenSharedIdsCHROMIUMBadArgs) {
  const GLuint kNamespaceId = id_namespaces::kTextures;
  GenSharedIdsCHROMIUM cmd;
  cmd.Init(kNamespaceId, 0, -1, kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(kNamespaceId, 0, 1, kInvalidSharedMemoryId, kSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(kNamespaceId, 0, 1, kSharedMemoryId, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, RegisterSharedIdsCHROMIUMBadArgs) {
  const GLuint kNamespaceId = id_namespaces::kTextures;
  RegisterSharedIdsCHROMIUM cmd;
  cmd.Init(kNamespaceId, -1, kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(kNamespaceId, 1, kInvalidSharedMemoryId, kSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(kNamespaceId, 1, kSharedMemoryId, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, RegisterSharedIdsCHROMIUMDuplicateIds) {
  const GLuint kNamespaceId = id_namespaces::kTextures;
  const GLuint kRegisterId = 3;
  RegisterSharedIdsCHROMIUM cmd;
  GLuint* ids = GetSharedMemoryAs<GLuint*>();
  ids[0] = kRegisterId;
  cmd.Init(kNamespaceId, 1, kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(kNamespaceId, 1, kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderTest, DeleteSharedIdsCHROMIUMBadArgs) {
  const GLuint kNamespaceId = id_namespaces::kTextures;
  DeleteSharedIdsCHROMIUM cmd;
  cmd.Init(kNamespaceId, -1, kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(kNamespaceId, 1, kInvalidSharedMemoryId, kSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(kNamespaceId, 1, kSharedMemoryId, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, TexSubImage2DValidArgs) {
  const int kWidth = 16;
  const int kHeight = 8;
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 1, GL_RGBA, kWidth, kHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
      0, 0);
  EXPECT_CALL(*gl_, TexSubImage2D(
      GL_TEXTURE_2D, 1, 0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
      shared_memory_address_))
      .Times(1)
      .RetiresOnSaturation();
  TexSubImage2D cmd;
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest, TexSubImage2DBadArgs) {
  const int kWidth = 16;
  const int kHeight = 8;
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 1, GL_RGBA, kWidth, kHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
      0, 0);
  TexSubImage2D cmd;
  cmd.Init(GL_TEXTURE0, 1, 0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, kWidth, kHeight, GL_TRUE, GL_UNSIGNED_BYTE,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_INT,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, -1, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 1, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, -1, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, 1, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, kWidth + 1, kHeight, GL_RGBA,
           GL_UNSIGNED_BYTE, kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, kWidth, kHeight + 1, GL_RGBA,
           GL_UNSIGNED_BYTE, kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, kWidth, kHeight, GL_RGB, GL_UNSIGNED_BYTE,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, kWidth, kHeight, GL_RGBA,
           GL_UNSIGNED_SHORT_4_4_4_4, kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
           kInvalidSharedMemoryId, kSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
           kSharedMemoryId, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, CopyTexSubImage2DValidArgs) {
  const int kWidth = 16;
  const int kHeight = 8;
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 1, GL_RGBA, kWidth, kHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
      0, 0);
  EXPECT_CALL(*gl_, CopyTexSubImage2D(
      GL_TEXTURE_2D, 1, 0, 0, 0, 0, kWidth, kHeight))
      .Times(1)
      .RetiresOnSaturation();
  CopyTexSubImage2D cmd;
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, 0, 0, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest, CopyTexSubImage2DBadArgs) {
  const int kWidth = 16;
  const int kHeight = 8;
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 1, GL_RGBA, kWidth, kHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
      0, 0);
  CopyTexSubImage2D cmd;
  cmd.Init(GL_TEXTURE0, 1, 0, 0, 0, 0, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, -1, 0, 0, 0, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 1, 0, 0, 0, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, -1, 0, 0, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, 1, 0, 0, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, 0, 0, kWidth + 1, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, 0, 0, kWidth, kHeight + 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

// Check that if a renderbuffer is attached and GL returns
// GL_FRAMEBUFFER_COMPLETE that the buffer is cleared and state is restored.
TEST_F(GLES2DecoderTest, FramebufferRenderbufferClearColor) {
  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
  ClearColor color_cmd;
  ColorMask color_mask_cmd;
  Enable enable_cmd;
  FramebufferRenderbuffer cmd;
  color_cmd.Init(0.1f, 0.2f, 0.3f, 0.4f);
  color_mask_cmd.Init(0, 1, 0, 1);
  enable_cmd.Init(GL_SCISSOR_TEST);
  cmd.Init(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
      client_renderbuffer_id_);
  InSequence sequence;
  EXPECT_CALL(*gl_, ClearColor(0.1f, 0.2f, 0.3f, 0.4f))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, ColorMask(0, 1, 0, 1))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, Enable(GL_SCISSOR_TEST))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, FramebufferRenderbufferEXT(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
      kServiceRenderbufferId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  SetupExpectationsForFramebufferAttachment(
      GL_COLOR_BUFFER_BIT,     // clear bits
      0.1f, 0.2f, 0.3f, 0.4f,  // color
      0x0101,                  // color bits
      0,                       // stencil
      -1,                      // stencil mask back
      -1,                      // stencil mask front
      1.0f,                    // depth
      1,                       // depth mask
      true);                   // scissor test
  EXPECT_EQ(error::kNoError, ExecuteCmd(color_cmd));
  EXPECT_EQ(error::kNoError, ExecuteCmd(color_mask_cmd));
  EXPECT_EQ(error::kNoError, ExecuteCmd(enable_cmd));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, FramebufferRenderbufferClearDepth) {
  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
  ClearDepthf depth_cmd;
  DepthMask depth_mask_cmd;
  FramebufferRenderbuffer cmd;
  depth_cmd.Init(0.5f);
  depth_mask_cmd.Init(false);
  cmd.Init(
      GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
      client_renderbuffer_id_);
  InSequence sequence;
  EXPECT_CALL(*gl_, ClearDepth(0.5f))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, DepthMask(0))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, FramebufferRenderbufferEXT(
      GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
      kServiceRenderbufferId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  SetupExpectationsForFramebufferAttachment(
      GL_DEPTH_BUFFER_BIT,    // clear bits
      0, 0, 0, 0,             // color
      0x1111,                 // color bits
      0,                      // stencil
      -1,                     // stencil mask back,
      -1,                     // stencil mask front,
      0.5f,                   // depth
      0,                      // depth mask
      false);                 // scissor test
  EXPECT_EQ(error::kNoError, ExecuteCmd(depth_cmd));
  EXPECT_EQ(error::kNoError, ExecuteCmd(depth_mask_cmd));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, FramebufferRenderbufferClearStencil) {
  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
  ClearStencil stencil_cmd;
  StencilMaskSeparate stencil_mask_separate_cmd;
  FramebufferRenderbuffer cmd;
  stencil_cmd.Init(123);
  stencil_mask_separate_cmd.Init(GL_BACK, 0x1234u);
  cmd.Init(
      GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
      client_renderbuffer_id_);
  InSequence sequence;
  EXPECT_CALL(*gl_, ClearStencil(123))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, StencilMaskSeparate(GL_BACK, 0x1234u))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, FramebufferRenderbufferEXT(
      GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
      kServiceRenderbufferId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  SetupExpectationsForFramebufferAttachment(
      GL_STENCIL_BUFFER_BIT,  // clear bits
      0, 0, 0, 0,             // color
      0x1111,                 // color bits
      123,                    // stencil
      -1,                     // stencil mask back,
      0x1234u,                // stencil mask front,
      1.0f,                   // depth
      1,                      // depth mask
      false);                 // scissor test
  EXPECT_EQ(error::kNoError, ExecuteCmd(stencil_cmd));
  EXPECT_EQ(error::kNoError, ExecuteCmd(stencil_mask_separate_cmd));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, IsBuffer) {
  EXPECT_FALSE(DoIsBuffer(client_buffer_id_));
  DoBindBuffer(GL_ARRAY_BUFFER, client_buffer_id_, kServiceBufferId);
  EXPECT_TRUE(DoIsBuffer(client_buffer_id_));
  DoDeleteBuffer(client_buffer_id_, kServiceBufferId);
  EXPECT_FALSE(DoIsBuffer(client_buffer_id_));
}

TEST_F(GLES2DecoderTest, IsFramebuffer) {
  EXPECT_FALSE(DoIsFramebuffer(client_framebuffer_id_));
  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
  EXPECT_TRUE(DoIsFramebuffer(client_framebuffer_id_));
  DoDeleteFramebuffer(client_framebuffer_id_, kServiceFramebufferId);
  EXPECT_FALSE(DoIsFramebuffer(client_framebuffer_id_));
}

TEST_F(GLES2DecoderTest, IsProgram) {
  // IsProgram is true as soon as the program is created.
  EXPECT_TRUE(DoIsProgram(client_program_id_));
  DoDeleteProgram(client_program_id_, kServiceProgramId);
  EXPECT_FALSE(DoIsProgram(client_program_id_));
}

TEST_F(GLES2DecoderTest, IsRenderbuffer) {
  EXPECT_FALSE(DoIsRenderbuffer(client_renderbuffer_id_));
  DoBindRenderbuffer(GL_RENDERBUFFER, client_renderbuffer_id_,
                    kServiceRenderbufferId);
  EXPECT_TRUE(DoIsRenderbuffer(client_renderbuffer_id_));
  DoDeleteRenderbuffer(client_renderbuffer_id_, kServiceRenderbufferId);
  EXPECT_FALSE(DoIsRenderbuffer(client_renderbuffer_id_));
}

TEST_F(GLES2DecoderTest, IsShader) {
  // IsShader is true as soon as the program is created.
  EXPECT_TRUE(DoIsShader(client_shader_id_));
  DoDeleteShader(client_shader_id_, kServiceShaderId);
  EXPECT_FALSE(DoIsShader(client_shader_id_));
}

TEST_F(GLES2DecoderTest, IsTexture) {
  EXPECT_FALSE(DoIsTexture(client_texture_id_));
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  EXPECT_TRUE(DoIsTexture(client_texture_id_));
  DoDeleteTexture(client_texture_id_, kServiceTextureId);
  EXPECT_FALSE(DoIsTexture(client_texture_id_));
}

#if 0  // Turn this test on once we allow GL_DEPTH_STENCIL_ATTACHMENT
TEST_F(GLES2DecoderTest, FramebufferRenderbufferClearDepthStencil) {
  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
  ClearDepthf depth_cmd;
  ClearStencil stencil_cmd;
  FramebufferRenderbuffer cmd;
  depth_cmd.Init(0.5f);
  stencil_cmd.Init(123);
  cmd.Init(
      GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
      client_renderbuffer_id_);
  InSequence sequence;
  EXPECT_CALL(*gl_, ClearDepth(0.5f))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, ClearStencil(123))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, FramebufferRenderbufferEXT(
      GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
      kServiceRenderbufferId))
      .Times(1)
      .RetiresOnSaturation();
  SetupExpectationsForFramebufferAttachment(
      GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,    // clear bits
      0, 0, 0, 0,             // color
      0x1111,                 // color bits
      123,                    // stencil
      -1,                     // stencil mask back,
      -1,                     // stencil mask front,
      0.5f,                   // depth
      1,                      // depth mask
      false);                 // scissor test
  EXPECT_EQ(error::kNoError, ExecuteCmd(depth_cmd));
  EXPECT_EQ(error::kNoError, ExecuteCmd(stencil_cmd));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}
#endif

TEST_F(GLES2DecoderWithShaderTest, VertexAttribPointer) {
  SetupVertexBuffer();
  static const GLenum types[] = {
    GL_BYTE,
    GL_UNSIGNED_BYTE,
    GL_SHORT,
    GL_UNSIGNED_SHORT,
    GL_FLOAT,
    GL_FIXED,
    GL_INT,
    GL_UNSIGNED_INT,
  };
  static const GLsizei sizes[] = {
    1,
    1,
    2,
    2,
    4,
    4,
    4,
    4,
  };
  static const GLuint indices[] = {
    0,
    1,
    kNumVertexAttribs - 1,
    kNumVertexAttribs,
  };
  static const GLsizei offset_mult[] = {
    0,
    0,
    1,
    1,
    2,
    1000,
  };
  static const GLsizei offset_offset[] = {
    0,
    1,
    0,
    1,
    0,
    0,
  };
  static const GLsizei stride_mult[] = {
    -1,
    0,
    0,
    1,
    1,
    2,
    1000,
  };
  static const GLsizei stride_offset[] = {
    0,
    0,
    1,
    0,
    1,
    0,
    0,
  };
  for (size_t tt = 0; tt < arraysize(types); ++tt) {
    GLenum type = types[tt];
    GLsizei num_bytes = sizes[tt];
    for (size_t ii = 0; ii < arraysize(indices); ++ii) {
      GLuint index = indices[ii];
      for (GLint size = 0; size < 5; ++size) {
        for (size_t oo = 0; oo < arraysize(offset_mult); ++oo) {
          GLuint offset = num_bytes * offset_mult[oo] + offset_offset[oo];
          for (size_t ss = 0; ss <= arraysize(stride_mult); ++ss) {
            GLsizei stride = num_bytes * stride_mult[ss] + stride_offset[ss];
            for (int normalize = 0; normalize < 2; ++normalize) {
              bool index_good = index < static_cast<GLuint>(kNumVertexAttribs);
              bool size_good = (size > 0 && size < 5);
              bool offset_good = (offset % num_bytes == 0);
              bool stride_good = (stride % num_bytes == 0) && stride >= 0 &&
                                 stride <= 255;
              bool type_good = (type != GL_INT && type != GL_UNSIGNED_INT &&
                                type != GL_FIXED);
              bool good = size_good && offset_good && stride_good &&
                          type_good && index_good;
              bool call = good && (type != GL_FIXED);
              if (call) {
                EXPECT_CALL(*gl_, VertexAttribPointer(
                    index, size, type, normalize, stride,
                    BufferOffset(offset)));
              }
              VertexAttribPointer cmd;
              cmd.Init(index, size, type, normalize, stride, offset);
              EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
              if (good) {
                EXPECT_EQ(GL_NO_ERROR, GetGLError());
              } else if (size_good &&
                         offset_good &&
                         stride_good &&
                         type_good &&
                         !index_good) {
                EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
              } else if (size_good &&
                         offset_good &&
                         stride_good &&
                         !type_good &&
                         index_good) {
                EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
              } else if (size_good &&
                         offset_good &&
                         !stride_good &&
                         type_good &&
                         index_good) {
                if (stride < 0 || stride > 255) {
                  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
                } else {
                  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
                }
              } else if (size_good &&
                         !offset_good &&
                         stride_good &&
                         type_good &&
                         index_good) {
                EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
              } else if (!size_good &&
                         offset_good &&
                         stride_good &&
                         type_good &&
                         index_good) {
                EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
              } else {
                EXPECT_NE(GL_NO_ERROR, GetGLError());
              }
            }
          }
        }
      }
    }
  }
}

// TODO(gman): BufferData

// TODO(gman): BufferDataImmediate

// TODO(gman): BufferSubData

// TODO(gman): BufferSubDataImmediate

// TODO(gman): CompressedTexImage2D

// TODO(gman): CompressedTexImage2DImmediate

// TODO(gman): CompressedTexSubImage2DImmediate

// TODO(gman): DeleteProgram

// TODO(gman): DeleteShader

// TODO(gman): PixelStorei

// TODO(gman): TexImage2D

// TODO(gman): TexImage2DImmediate

// TODO(gman): TexSubImage2DImmediate

// TODO(gman): UseProgram

// TODO(gman): SwapBuffers

}  // namespace gles2
}  // namespace gpu
