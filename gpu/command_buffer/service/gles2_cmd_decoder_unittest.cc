// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

#include "base/atomicops.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/id_allocator.h"
#include "gpu/command_buffer/service/async_pixel_transfer_delegate_mock.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gl_surface_mock.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest_base.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/mocks.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/stream_texture_manager_mock.h"
#include "gpu/command_buffer/service/stream_texture_mock.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gl_surface_stub.h"


#if !defined(GL_DEPTH24_STENCIL8)
#define GL_DEPTH24_STENCIL8 0x88F0
#endif

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
using ::testing::SetArgPointee;
using ::testing::StrEq;
using ::testing::StrictMock;

namespace gpu {
namespace gles2 {

using namespace cmds;

class GLES2DecoderTest : public GLES2DecoderTestBase {
 public:
  GLES2DecoderTest() { }

 protected:
  void CheckReadPixelsOutOfRange(
      GLint in_read_x, GLint in_read_y,
      GLsizei in_read_width, GLsizei in_read_height,
      bool init);
};

class GLES2DecoderANGLETest : public GLES2DecoderTestBase {
 public:
  GLES2DecoderANGLETest()
      : GLES2DecoderTestBase() {
    GLES2Decoder::set_testing_force_is_angle(true);
  }

  virtual ~GLES2DecoderANGLETest() {
    GLES2Decoder::set_testing_force_is_angle(false);
  }
};

class GLES2DecoderWithShaderTest : public GLES2DecoderWithShaderTestBase {
 public:
  GLES2DecoderWithShaderTest()
      : GLES2DecoderWithShaderTestBase() {
  }

  void CheckTextureChangesMarkFBOAsNotComplete(bool bound_fbo);
  void CheckRenderbufferChangesMarkFBOAsNotComplete(bool bound_fbo);
};

class GLES2DecoderGeometryInstancingTest : public GLES2DecoderWithShaderTest {
 public:
  GLES2DecoderGeometryInstancingTest()
      : GLES2DecoderWithShaderTest() {
  }

  virtual void SetUp() {
    InitDecoder(
        "GL_ANGLE_instanced_arrays", // extensions
        true,                        // has alpha
        true,                        // has depth
        false,                       // has stencil
        true,                        // request alpha
        true,                        // request depth
        false,                       // request stencil
        true);                       // bind generates resource
    SetupDefaultProgram();
  }
};

class GLES2DecoderRGBBackbufferTest : public GLES2DecoderWithShaderTest {
 public:
  GLES2DecoderRGBBackbufferTest() { }

  virtual void SetUp() {
    InitDecoder(
        "",     // extensions
        false,  // has alpha
        false,  // has depth
        false,  // has stencil
        false,  // request alpha
        false,  // request depth
        false,  // request stencil
        true);   // bind generates resource
    SetupDefaultProgram();
  }
};

class GLES2DecoderManualInitTest : public GLES2DecoderWithShaderTest {
 public:
  GLES2DecoderManualInitTest() { }

  // Override default setup so nothing gets setup.
  virtual void SetUp() {
  }
};

class GLES2DecoderANGLEManualInitTest : public GLES2DecoderANGLETest {
 public:
  // Override default setup so nothing gets setup.
  virtual void SetUp() {
  }
};

TEST_F(GLES2DecoderWithShaderTest, DrawArraysNoAttributesSucceeds) {
  SetupTexture();
  AddExpectationsForSimulatedAttrib0(kNumVertices, 0);
  SetupExpectationsForApplyingDefaultDirtyState();

  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

// Tests when the math overflows (0x40000000 * sizeof GLfloat)
TEST_F(GLES2DecoderWithShaderTest, DrawArraysSimulatedAttrib0OverflowFails) {
  const GLsizei kLargeCount = 0x40000000;
  SetupTexture();
  EXPECT_CALL(*gl_, DrawArrays(_, _, _))
      .Times(0)
      .RetiresOnSaturation();
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kLargeCount);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
}

// Tests when the math overflows (0x7FFFFFFF + 1 = 0x8000000 verts)
TEST_F(GLES2DecoderWithShaderTest, DrawArraysSimulatedAttrib0PosToNegFails) {
  const GLsizei kLargeCount = 0x7FFFFFFF;
  SetupTexture();
  EXPECT_CALL(*gl_, DrawArrays(_, _, _))
      .Times(0)
      .RetiresOnSaturation();
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kLargeCount);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
}

// Tests when the driver returns an error
TEST_F(GLES2DecoderWithShaderTest, DrawArraysSimulatedAttrib0OOMFails) {
  const GLsizei kFakeLargeCount = 0x1234;
  SetupTexture();
  AddExpectationsForSimulatedAttrib0WithError(
      kFakeLargeCount, 0, GL_OUT_OF_MEMORY);
  EXPECT_CALL(*gl_, DrawArrays(_, _, _))
      .Times(0)
      .RetiresOnSaturation();
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kFakeLargeCount);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawArraysBadTextureUsesBlack) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  // This is an NPOT texture. As the default filtering requires mips
  // this should trigger replacing with black textures before rendering.
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 3, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               kSharedMemoryId, kSharedMemoryOffset);
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
  SetupExpectationsForApplyingDefaultDirtyState();
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
  DoEnableVertexAttribArray(1);
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);
  AddExpectationsForSimulatedAttrib0(kNumVertices, kServiceBufferId);
  SetupExpectationsForApplyingDefaultDirtyState();

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
  SetupExpectationsForApplyingDefaultDirtyState();
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

TEST_F(GLES2DecoderWithShaderTest, DrawArraysInstancedANGLEFails) {
  SetupTexture();
  SetupVertexBuffer();
  DoEnableVertexAttribArray(1);
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  EXPECT_CALL(*gl_, DrawArraysInstancedANGLE(_, _, _, _))
      .Times(0)
      .RetiresOnSaturation();
  DrawArraysInstancedANGLE cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderGeometryInstancingTest,
       DrawArraysInstancedANGLENoAttributesFails) {
  SetupTexture();

  EXPECT_CALL(*gl_, DrawArraysInstancedANGLE(_, _, _, _))
      .Times(0)
      .RetiresOnSaturation();
  DrawArraysInstancedANGLE cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderGeometryInstancingTest,
       DrawArraysInstancedANGLESimulatedAttrib0) {
  SetupTexture();
  SetupVertexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  AddExpectationsForSimulatedAttrib0(kNumVertices, kServiceBufferId);
  SetupExpectationsForApplyingDefaultDirtyState();

  DoVertexAttribDivisorANGLE(0, 1);
  EXPECT_CALL(*gl_, DrawArraysInstancedANGLE(GL_TRIANGLES, 0, kNumVertices, 3))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, VertexAttribDivisorANGLE(0, 0))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, VertexAttribDivisorANGLE(0, 1))
      .Times(1)
      .RetiresOnSaturation();
  DrawArraysInstancedANGLE cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderGeometryInstancingTest,
       DrawArraysInstancedANGLEMissingAttributesFails) {
  DoEnableVertexAttribArray(1);

  EXPECT_CALL(*gl_, DrawArraysInstancedANGLE(_, _, _, _))
      .Times(0);
  DrawArraysInstancedANGLE cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderGeometryInstancingTest,
       DrawArraysInstancedANGLEMissingAttributesZeroCountSucceeds) {
  DoEnableVertexAttribArray(1);

  EXPECT_CALL(*gl_, DrawArraysInstancedANGLE(_, _, _, _))
      .Times(0);
  DrawArraysInstancedANGLE cmd;
  cmd.Init(GL_TRIANGLES, 0, 0, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderGeometryInstancingTest,
       DrawArraysInstancedANGLEValidAttributesSucceeds) {
  SetupTexture();
  SetupVertexBuffer();
  DoEnableVertexAttribArray(1);
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);
  AddExpectationsForSimulatedAttrib0(kNumVertices, kServiceBufferId);
  SetupExpectationsForApplyingDefaultDirtyState();

  EXPECT_CALL(*gl_, DrawArraysInstancedANGLE(GL_TRIANGLES, 0, kNumVertices, 1))
      .Times(1)
      .RetiresOnSaturation();
  DrawArraysInstancedANGLE cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderGeometryInstancingTest,
       DrawArraysInstancedANGLEWithInvalidModeFails) {
  SetupVertexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  EXPECT_CALL(*gl_, DrawArraysInstancedANGLE(_, _, _, _))
      .Times(0);
  DrawArraysInstancedANGLE cmd;
  cmd.Init(GL_QUADS, 0, 1, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  cmd.Init(GL_POLYGON, 0, 1, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderGeometryInstancingTest,
       DrawArraysInstancedANGLEInvalidPrimcountFails) {
  SetupVertexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  EXPECT_CALL(*gl_, DrawArraysInstancedANGLE(_, _, _, _))
      .Times(0);
  DrawArraysInstancedANGLE cmd;
  cmd.Init(GL_TRIANGLES, 0, 1, -1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

// Per-instance data is twice as large, but number of instances is half
TEST_F(GLES2DecoderGeometryInstancingTest,
       DrawArraysInstancedANGLELargeInstanceSucceeds) {
  SetupTexture();
  SetupVertexBuffer();
  SetupExpectationsForApplyingDefaultDirtyState();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  DoEnableVertexAttribArray(0);
  DoVertexAttribPointer(0, 4, GL_FLOAT, 0, 0);
  DoVertexAttribDivisorANGLE(0, 1);
  EXPECT_CALL(*gl_, DrawArraysInstancedANGLE(GL_TRIANGLES, 0, kNumVertices,
                                             kNumVertices / 2))
      .Times(1)
      .RetiresOnSaturation();
  DrawArraysInstancedANGLE cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices, kNumVertices / 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

// Per-instance data is twice as large, but divisor is twice
TEST_F(GLES2DecoderGeometryInstancingTest,
       DrawArraysInstancedANGLELargeDivisorSucceeds) {
  SetupTexture();
  SetupVertexBuffer();
  SetupExpectationsForApplyingDefaultDirtyState();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  DoEnableVertexAttribArray(0);
  DoVertexAttribPointer(0, 4, GL_FLOAT, 0, 0);
  DoVertexAttribDivisorANGLE(0, 2);
  EXPECT_CALL(*gl_, DrawArraysInstancedANGLE(GL_TRIANGLES, 0, kNumVertices,
                                             kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  DrawArraysInstancedANGLE cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderGeometryInstancingTest, DrawArraysInstancedANGLELargeFails) {
  SetupTexture();
  SetupVertexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  DoEnableVertexAttribArray(0);
  DoVertexAttribPointer(0, 2, GL_FLOAT, 0, 0);
  DoVertexAttribDivisorANGLE(0, 1);
  EXPECT_CALL(*gl_, DrawArraysInstancedANGLE(_, _, _, _))
      .Times(0)
      .RetiresOnSaturation();
  DrawArraysInstancedANGLE cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices, kNumVertices + 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  EXPECT_CALL(*gl_, DrawArraysInstancedANGLE(_, _, _, _))
      .Times(0)
      .RetiresOnSaturation();
  cmd.Init(GL_TRIANGLES, 0, kNumVertices + 1, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

// Per-index data is twice as large, but number of indices is half
TEST_F(GLES2DecoderGeometryInstancingTest,
       DrawArraysInstancedANGLELargeIndexSucceeds) {
  SetupTexture();
  SetupVertexBuffer();
  SetupExpectationsForApplyingDefaultDirtyState();
  DoVertexAttribPointer(1, 4, GL_FLOAT, 0, 0);

  DoEnableVertexAttribArray(0);
  DoVertexAttribPointer(0, 2, GL_FLOAT, 0, 0);
  DoVertexAttribDivisorANGLE(0, 1);
  EXPECT_CALL(*gl_, DrawArraysInstancedANGLE(GL_TRIANGLES, 0, kNumVertices / 2,
                                             kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  DrawArraysInstancedANGLE cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices / 2, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderGeometryInstancingTest,
       DrawArraysInstancedANGLENoDivisor0Fails) {
  SetupTexture();
  SetupVertexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  DoEnableVertexAttribArray(0);
  DoVertexAttribPointer(0, 2, GL_FLOAT, 0, 0);
  DoVertexAttribDivisorANGLE(0, 1);
  DoVertexAttribDivisorANGLE(1, 1);
  EXPECT_CALL(*gl_, DrawArraysInstancedANGLE(_, _, _, _))
      .Times(0)
      .RetiresOnSaturation();
  DrawArraysInstancedANGLE cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsNoAttributesSucceeds) {
  SetupTexture();
  SetupIndexBuffer();
  AddExpectationsForSimulatedAttrib0(kMaxValidIndex + 1, 0);
  SetupExpectationsForApplyingDefaultDirtyState();
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
  SetupExpectationsForApplyingDefaultDirtyState();

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
  SetupExpectationsForApplyingDefaultDirtyState();
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

TEST_F(GLES2DecoderWithShaderTest, DrawElementsInstancedANGLEFails) {
  SetupTexture();
  SetupVertexBuffer();
  SetupIndexBuffer();
  DoEnableVertexAttribArray(1);
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  EXPECT_CALL(*gl_, DrawElementsInstancedANGLE(_, _, _, _, _))
      .Times(0)
      .RetiresOnSaturation();
  DrawElementsInstancedANGLE cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderGeometryInstancingTest,
       DrawElementsInstancedANGLENoAttributesFails) {
  SetupTexture();
  SetupIndexBuffer();

  EXPECT_CALL(*gl_, DrawElementsInstancedANGLE(_, _, _, _, _))
      .Times(0)
      .RetiresOnSaturation();
  DrawElementsInstancedANGLE cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderGeometryInstancingTest,
       DrawElementsInstancedANGLESimulatedAttrib0) {
  SetupTexture();
  SetupVertexBuffer();
  SetupIndexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  AddExpectationsForSimulatedAttrib0(kMaxValidIndex + 1, kServiceBufferId);
  SetupExpectationsForApplyingDefaultDirtyState();

  DoVertexAttribDivisorANGLE(0, 1);
  EXPECT_CALL(*gl_, DrawElementsInstancedANGLE(
                        GL_TRIANGLES,
                        kValidIndexRangeCount,
                        GL_UNSIGNED_SHORT,
                        BufferOffset(kValidIndexRangeStart * 2),
                        3))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, VertexAttribDivisorANGLE(0, 0))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, VertexAttribDivisorANGLE(0, 1))
      .Times(1)
      .RetiresOnSaturation();
  DrawElementsInstancedANGLE cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderGeometryInstancingTest,
       DrawElementsInstancedANGLEMissingAttributesFails) {
  SetupIndexBuffer();
  DoEnableVertexAttribArray(1);

  EXPECT_CALL(*gl_, DrawElementsInstancedANGLE(_, _, _, _, _))
      .Times(0);
  DrawElementsInstancedANGLE cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderGeometryInstancingTest,
       DrawElementsInstancedANGLEMissingAttributesZeroCountSucceeds) {
  SetupIndexBuffer();
  DoEnableVertexAttribArray(1);

  EXPECT_CALL(*gl_, DrawElementsInstancedANGLE(_, _, _, _, _))
      .Times(0);
  DrawElementsInstancedANGLE cmd;
  cmd.Init(GL_TRIANGLES, 0, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderGeometryInstancingTest,
       DrawElementsInstancedANGLEValidAttributesSucceeds) {
  SetupIndexBuffer();
  SetupTexture();
  SetupVertexBuffer();
  DoEnableVertexAttribArray(1);
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);
  AddExpectationsForSimulatedAttrib0(kMaxValidIndex + 1, kServiceBufferId);
  SetupExpectationsForApplyingDefaultDirtyState();

  EXPECT_CALL(*gl_, DrawElementsInstancedANGLE(
                        GL_TRIANGLES,
                        kValidIndexRangeCount,
                        GL_UNSIGNED_SHORT,
                        BufferOffset(kValidIndexRangeStart * 2),
                        1))
      .Times(1)
      .RetiresOnSaturation();
  DrawElementsInstancedANGLE cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderGeometryInstancingTest,
       DrawElementsInstancedANGLEWithInvalidModeFails) {
  SetupIndexBuffer();
  SetupVertexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  EXPECT_CALL(*gl_, DrawElementsInstancedANGLE(_, _, _, _, _))
      .Times(0);
  DrawElementsInstancedANGLE cmd;
  cmd.Init(GL_QUADS, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  cmd.Init(GL_INVALID_ENUM, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

// Per-instance data is twice as large, but number of instances is half
TEST_F(GLES2DecoderGeometryInstancingTest,
       DrawElementsInstancedANGLELargeInstanceSucceeds) {
  SetupTexture();
  SetupIndexBuffer();
  SetupVertexBuffer();
  SetupExpectationsForApplyingDefaultDirtyState();
  //Add offset so we're sure we're accessing data near the end of the buffer.
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0,
                        (kNumVertices - kMaxValidIndex - 1) * 2 *
                            sizeof(GLfloat));

  DoEnableVertexAttribArray(0);
  DoVertexAttribPointer(0, 4, GL_FLOAT, 0, 0);
  DoVertexAttribDivisorANGLE(0, 1);
  EXPECT_CALL(*gl_, DrawElementsInstancedANGLE(
                        GL_TRIANGLES,
                        kValidIndexRangeCount,
                        GL_UNSIGNED_SHORT,
                        BufferOffset(kValidIndexRangeStart * 2),
                        kNumVertices / 2))
      .Times(1)
      .RetiresOnSaturation();
  DrawElementsInstancedANGLE cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2, kNumVertices / 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

// Per-instance data is twice as large, but divisor is twice
TEST_F(GLES2DecoderGeometryInstancingTest,
       DrawElementsInstancedANGLELargeDivisorSucceeds) {
  SetupTexture();
  SetupIndexBuffer();
  SetupVertexBuffer();
  SetupExpectationsForApplyingDefaultDirtyState();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  DoEnableVertexAttribArray(0);
  DoVertexAttribPointer(0, 4, GL_FLOAT, 0, 0);
  DoVertexAttribDivisorANGLE(0, 2);
  EXPECT_CALL(*gl_, DrawElementsInstancedANGLE(
                        GL_TRIANGLES,
                        kValidIndexRangeCount,
                        GL_UNSIGNED_SHORT,
                        BufferOffset(kValidIndexRangeStart * 2),
                        kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  DrawElementsInstancedANGLE cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderGeometryInstancingTest,
       DrawElementsInstancedANGLELargeFails) {
  SetupTexture();
  SetupIndexBuffer();
  SetupVertexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  DoEnableVertexAttribArray(0);
  DoVertexAttribPointer(0, 2, GL_FLOAT, 0, 0);
  DoVertexAttribDivisorANGLE(0, 1);
  EXPECT_CALL(*gl_, DrawElementsInstancedANGLE(_, _, _, _, _))
      .Times(0)
      .RetiresOnSaturation();
  DrawElementsInstancedANGLE cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2, kNumVertices + 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  EXPECT_CALL(*gl_, DrawElementsInstancedANGLE(_, _, _, _, _))
      .Times(0)
      .RetiresOnSaturation();
  cmd.Init(GL_TRIANGLES, kInvalidIndexRangeCount, GL_UNSIGNED_SHORT,
           kInvalidIndexRangeStart * 2, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderGeometryInstancingTest,
       DrawElementsInstancedANGLEInvalidPrimcountFails) {
  SetupTexture();
  SetupIndexBuffer();
  SetupVertexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  DoEnableVertexAttribArray(0);
  DoVertexAttribPointer(0, 2, GL_FLOAT, 0, 0);
  DoVertexAttribDivisorANGLE(0, 1);
  EXPECT_CALL(*gl_, DrawElementsInstancedANGLE(_, _, _, _, _))
      .Times(0)
      .RetiresOnSaturation();
  DrawElementsInstancedANGLE cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2, -1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

// Per-index data is twice as large, but values of indices are smaller
TEST_F(GLES2DecoderGeometryInstancingTest,
       DrawElementsInstancedANGLELargeIndexSucceeds) {
  SetupTexture();
  SetupIndexBuffer();
  SetupVertexBuffer();
  SetupExpectationsForApplyingDefaultDirtyState();
  DoVertexAttribPointer(1, 4, GL_FLOAT, 0, 0);

  DoEnableVertexAttribArray(0);
  DoVertexAttribPointer(0, 2, GL_FLOAT, 0, 0);
  DoVertexAttribDivisorANGLE(0, 1);
  EXPECT_CALL(*gl_, DrawElementsInstancedANGLE(
                        GL_TRIANGLES,
                        kValidIndexRangeCount,
                        GL_UNSIGNED_SHORT,
                        BufferOffset(kValidIndexRangeStart * 2),
                        kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  DrawElementsInstancedANGLE cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderGeometryInstancingTest,
       DrawElementsInstancedANGLENoDivisor0Fails) {
  SetupTexture();
  SetupIndexBuffer();
  SetupVertexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  DoEnableVertexAttribArray(0);
  DoVertexAttribPointer(0, 2, GL_FLOAT, 0, 0);
  DoVertexAttribDivisorANGLE(0, 1);
  DoVertexAttribDivisorANGLE(1, 1);
  EXPECT_CALL(*gl_, DrawElementsInstancedANGLE(_, _, _, _, _))
      .Times(0)
      .RetiresOnSaturation();
  DrawElementsInstancedANGLE cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2, kNumVertices);
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
  cmd.Init(client_program_id_,
           kUniform2FakeLocation,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformiv(kServiceProgramId, kUniform2RealLocation, _))
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
  cmd.Init(client_program_id_,
           kUniform2ElementFakeLocation,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_,
              GetUniformiv(kServiceProgramId, kUniform2ElementRealLocation, _))
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
  cmd.Init(kInvalidClientId,
           kUniform2FakeLocation,
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
  cmd.Init(client_shader_id_,
           kUniform2FakeLocation,
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
  cmd.Init(kNewClientId,
           kUniform2FakeLocation,
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
  cmd.Init(client_program_id_,
           kUniform2FakeLocation,
           kInvalidSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformiv(_, _, _))
      .Times(0);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, kUniform2FakeLocation,
           kSharedMemoryId, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
};

TEST_F(GLES2DecoderWithShaderTest, GetUniformfvSucceeds) {
  GetUniformfv::Result* result =
      static_cast<GetUniformfv::Result*>(shared_memory_address_);
  result->size = 0;
  GetUniformfv cmd;
  cmd.Init(client_program_id_,
           kUniform2FakeLocation,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformfv(kServiceProgramId, kUniform2RealLocation, _))
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
  cmd.Init(client_program_id_,
           kUniform2ElementFakeLocation,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_,
              GetUniformfv(kServiceProgramId, kUniform2ElementRealLocation, _))
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
  cmd.Init(kInvalidClientId,
           kUniform2FakeLocation,
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
  cmd.Init(client_shader_id_,
           kUniform2FakeLocation,
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
  cmd.Init(kNewClientId,
           kUniform2FakeLocation,
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
  cmd.Init(client_program_id_,
           kUniform2FakeLocation,
           kInvalidSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformfv(_, _, _))
      .Times(0);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, kUniform2FakeLocation,
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
  EXPECT_EQ(127, result->min_range);
  EXPECT_EQ(127, result->max_range);
  EXPECT_EQ(23, result->precision);
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

TEST_F(GLES2DecoderTest, GetIntegervCached) {
  struct TestInfo {
    GLenum pname;
    GLint expected;
  };
  TestInfo tests[] = {
    { GL_MAX_TEXTURE_SIZE, TestHelper::kMaxTextureSize, },
    { GL_MAX_CUBE_MAP_TEXTURE_SIZE, TestHelper::kMaxCubeMapTextureSize, },
    { GL_MAX_RENDERBUFFER_SIZE, TestHelper::kMaxRenderbufferSize, },
  };
  typedef GetIntegerv::Result Result;
  for (size_t ii = 0; ii < sizeof(tests) / sizeof(tests[0]); ++ii) {
    const TestInfo& test = tests[ii];
    Result* result = static_cast<Result*>(shared_memory_address_);
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, GetIntegerv(test.pname, _))
        .Times(0);
    result->size = 0;
    GetIntegerv cmd2;
    cmd2.Init(test.pname, shared_memory_id_, shared_memory_offset_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
    EXPECT_EQ(
        decoder_->GetGLES2Util()->GLGetNumValuesReturned(test.pname),
        result->GetNumResults());
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
    EXPECT_EQ(test.expected, result->GetData()[0]);
  }
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

TEST_F(GLES2DecoderTest, ShaderSourceStripComments) {
  const uint32 kInBucketId = 123;
  const char kSource[] = "hello/*te\ast*/world//a\ab";
  SetBucketAsCString(kInBucketId, kSource);
  ShaderSourceBucket cmd;
  cmd.Init(client_shader_id_, kInBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
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

TEST_F(GLES2DecoderTest, GenerateMipmapHandlesOutOfMemory) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  TextureManager* manager = group().texture_manager();
  Texture* info =
      manager->GetTexture(client_texture_id_);
  ASSERT_TRUE(info != NULL);
  GLint width = 0;
  GLint height = 0;
  EXPECT_FALSE(info->GetLevelSize(GL_TEXTURE_2D, 2, &width, &height));
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE,
      kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, TexParameteri(
      GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GenerateMipmapEXT(GL_TEXTURE_2D))
      .Times(1);
  EXPECT_CALL(*gl_, TexParameteri(
      GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_OUT_OF_MEMORY))
      .RetiresOnSaturation();
  GenerateMipmap cmd;
  cmd.Init(GL_TEXTURE_2D);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
  EXPECT_FALSE(info->GetLevelSize(GL_TEXTURE_2D, 2, &width, &height));
}

TEST_F(GLES2DecoderTest, GenerateMipmapClearsUnclearedTexture) {
  EXPECT_CALL(*gl_, GenerateMipmapEXT(_))
       .Times(0);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               0, 0);
  SetupClearTextureExpections(
      kServiceTextureId, kServiceTextureId, GL_TEXTURE_2D, GL_TEXTURE_2D,
      0, GL_RGBA, GL_UNSIGNED_BYTE, 2, 2);
  EXPECT_CALL(*gl_, TexParameteri(
      GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GenerateMipmapEXT(GL_TEXTURE_2D));
  EXPECT_CALL(*gl_, TexParameteri(
      GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  GenerateMipmap cmd;
  cmd.Init(GL_TEXTURE_2D);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, Uniform1iValidArgs) {
  EXPECT_CALL(*gl_, Uniform1i(kUniform1RealLocation, 2));
  Uniform1i cmd;
  cmd.Init(kUniform1FakeLocation, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, Uniform1ivValidArgs) {
  EXPECT_CALL(
      *gl_, Uniform1iv(kUniform1RealLocation, 1,
          reinterpret_cast<const GLint*>(shared_memory_address_)));
  Uniform1iv cmd;
  cmd.Init(kUniform1FakeLocation,
           1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, Uniform1ivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Uniform1iv(_, _, _)).Times(0);
  Uniform1iv cmd;
  cmd.Init(kUniform1FakeLocation,
           1, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, Uniform1ivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, Uniform1iv(_, _, _)).Times(0);
  Uniform1iv cmd;
  cmd.Init(kUniform1FakeLocation,
           1, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, Uniform1ivImmediateValidArgs) {
  Uniform1ivImmediate& cmd = *GetImmediateAs<Uniform1ivImmediate>();
  EXPECT_CALL(
      *gl_,
      Uniform1iv(kUniform1RealLocation, 1,
          reinterpret_cast<GLint*>(ImmediateDataAddress(&cmd))));
  GLint temp[1 * 2] = { 0, };
  cmd.Init(kUniform1FakeLocation, 1,
           &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderWithShaderTest, Uniform1ivInvalidValidArgs) {
  EXPECT_CALL(*gl_, Uniform1iv(_, _, _)).Times(0);
  Uniform1iv cmd;
  cmd.Init(kUniform1FakeLocation,
           2, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, Uniform1ivZeroCount) {
  EXPECT_CALL(*gl_, Uniform1iv(_, _, _)).Times(0);
  Uniform1iv cmd;
  cmd.Init(kUniform1FakeLocation,
           0, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, Uniform1iSamplerIsLmited) {
  EXPECT_CALL(*gl_, Uniform1i(_, _)).Times(0);
  Uniform1i cmd;
  cmd.Init(
      kUniform1FakeLocation,
      kNumTextureUnits);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, Uniform1ivSamplerIsLimited) {
  EXPECT_CALL(*gl_, Uniform1iv(_, _, _)).Times(0);
  Uniform1ivImmediate& cmd = *GetImmediateAs<Uniform1ivImmediate>();
  GLint temp[] = { kNumTextureUnits };
  cmd.Init(kUniform1FakeLocation, 1,
           &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
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

TEST_F(GLES2DecoderWithShaderTest, BindAndDeleteFramebuffer) {
  SetupTexture();
  AddExpectationsForSimulatedAttrib0(kNumVertices, 0);
  SetupExpectationsForApplyingDefaultDirtyState();
  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
  DoDeleteFramebuffer(
      client_framebuffer_id_, kServiceFramebufferId,
      true, GL_FRAMEBUFFER, 0,
      true, GL_FRAMEBUFFER, 0);
  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
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
                     const void* src_pixels, const void* expected_pixels,
                     GLint pack_alignment)
      : width_(width),
        height_(height),
        pack_alignment_(pack_alignment),
        bytes_per_pixel_(bytes_per_pixel),
        src_pixels_(reinterpret_cast<const int8*>(src_pixels)),
        expected_pixels_(reinterpret_cast<const int8*>(expected_pixels)) {
  }

  void ReadPixels(
      GLint x, GLint y, GLsizei width, GLsizei height,
      GLenum format, GLenum type, void* pixels) const {
    DCHECK_GE(x, 0);
    DCHECK_GE(y, 0);
    DCHECK_LE(x + width, width_);
    DCHECK_LE(y + height, height_);
    for (GLint yy = 0; yy < height; ++yy) {
      const int8* src = GetPixelAddress(src_pixels_, x, y + yy);
      const void* dst = ComputePackAlignmentAddress(0, yy, width, pixels);
      memcpy(const_cast<void*>(dst), src, width * bytes_per_pixel_);
    }
  }

  bool CompareRowSegment(
      GLint x, GLint y, GLsizei width, const void* data) const {
    DCHECK(x + width <= width_ || width == 0);
    return memcmp(data, GetPixelAddress(expected_pixels_, x, y),
                  width * bytes_per_pixel_) == 0;
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
      GLint temp = row_size + pack_alignment_ - 1;
      GLint padded_row_size = (temp / pack_alignment_) * pack_alignment_;
      GLint size_of_all_but_last_row = (height - 1) * padded_row_size;
      return size_of_all_but_last_row + row_size;
    } else {
      return height * row_size;
    }
  }

 private:
  const int8* GetPixelAddress(const int8* base, GLint x, GLint y) const {
    return base + (width_ * y + x) * bytes_per_pixel_;
  }

  GLsizei width_;
  GLsizei height_;
  GLint pack_alignment_;
  GLint bytes_per_pixel_;
  const int8* src_pixels_;
  const int8* expected_pixels_;
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
    DoTexImage2D(
        GL_TEXTURE_2D, 0, kFormat, kWidth, kHeight, 0,
        kFormat, GL_UNSIGNED_BYTE, kSharedMemoryId,
        kSharedMemoryOffset);
    DoBindFramebuffer(
        GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
    DoFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
        client_texture_id_, kServiceTextureId, 0, GL_NO_ERROR);
    EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_FRAMEBUFFER))
        .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
        .RetiresOnSaturation();
  }

  ReadPixelsEmulator emu(
      kWidth, kHeight, kBytesPerPixel, kSrcPixels, kSrcPixels, kPackAlignment);
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

  surface_->SetSize(gfx::Size(INT_MAX, INT_MAX));

  ReadPixelsEmulator emu(
      kWidth, kHeight, kBytesPerPixel, kSrcPixels, kSrcPixels, kPackAlignment);
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

TEST_F(GLES2DecoderRGBBackbufferTest, ReadPixelsNoAlphaBackbuffer) {
  const GLsizei kWidth = 3;
  const GLsizei kHeight = 3;
  const GLint kBytesPerPixel = 4;
  const GLint kPackAlignment = 4;
  static const uint8 kExpectedPixels[kWidth * kHeight * kBytesPerPixel] = {
    12, 13, 14, 255, 19, 18, 19, 255, 13, 14, 18, 255,
    29, 28, 23, 255, 21, 22, 21, 255, 28, 23, 22, 255,
    31, 34, 39, 255, 32, 37, 32, 255, 34, 39, 37, 255,
  };
  static const uint8 kSrcPixels[kWidth * kHeight * kBytesPerPixel] = {
    12, 13, 14, 18, 19, 18, 19, 12, 13, 14, 18, 19,
    29, 28, 23, 22, 21, 22, 21, 29, 28, 23, 22, 21,
    31, 34, 39, 37, 32, 37, 32, 31, 34, 39, 37, 32,
  };

  surface_->SetSize(gfx::Size(INT_MAX, INT_MAX));

  ReadPixelsEmulator emu(
      kWidth, kHeight, kBytesPerPixel, kSrcPixels, kExpectedPixels,
      kPackAlignment);
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
      *gl_, ReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, _))
      .WillOnce(Invoke(&emu, &ReadPixelsEmulator::ReadPixels));
  ReadPixels cmd;
  cmd.Init(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
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
  const char* kBadName = "test\aing";
  const uint32 kNameSize = strlen(kName);
  const uint32 kBadNameSize = strlen(kBadName);
  EXPECT_CALL(*gl_, BindAttribLocation(_, _, _)).Times(0);
  memcpy(shared_memory_address_, kName, kNameSize);
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
  memcpy(shared_memory_address_, kBadName, kBadNameSize);
  cmd.Init(client_program_id_, kLocation,
           kSharedMemoryId, kSharedMemoryOffset, kBadNameSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
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
  const char* kBadName = "foo\abar";
  const uint32 kBadNameSize = strlen(kBadName);
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
  memcpy(name, kBadName, kBadNameSize);
  cmd.Init(client_program_id_,
           kSharedMemoryId, kNameOffset,
           kSharedMemoryId, kSharedMemoryOffset,
           kBadNameSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
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
  EXPECT_EQ(kUniform2FakeLocation, *result);
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
  const char* kBadName = "foo\abar";
  const uint32 kBadNameSize = strlen(kBadName);
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
  memcpy(name, kBadName, kBadNameSize);
  cmd.Init(client_program_id_,
           kSharedMemoryId, kNameOffset,
           kSharedMemoryId, kSharedMemoryOffset,
           kBadNameSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
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
  EXPECT_EQ(kUniform2FakeLocation, *result);
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
  EXPECT_EQ(kUniform2FakeLocation, *result);
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
  IdAllocatorInterface* id_allocator = GetIdAllocator(kNamespaceId);
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
      kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, TexSubImage2D(
      GL_TEXTURE_2D, 1, 1, 0, kWidth - 1, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
      shared_memory_address_))
      .Times(1)
      .RetiresOnSaturation();
  TexSubImage2D cmd;
  cmd.Init(
      GL_TEXTURE_2D, 1, 1, 0, kWidth - 1, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
      kSharedMemoryId, kSharedMemoryOffset, GL_FALSE);
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
           kSharedMemoryId, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, kWidth, kHeight, GL_TRUE, GL_UNSIGNED_BYTE,
           kSharedMemoryId, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_INT,
           kSharedMemoryId, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, -1, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
           kSharedMemoryId, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 1, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
           kSharedMemoryId, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, -1, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
           kSharedMemoryId, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, 1, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
           kSharedMemoryId, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, kWidth + 1, kHeight, GL_RGBA,
           GL_UNSIGNED_BYTE, kSharedMemoryId, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, kWidth, kHeight + 1, GL_RGBA,
           GL_UNSIGNED_BYTE, kSharedMemoryId, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, kWidth, kHeight, GL_RGB, GL_UNSIGNED_BYTE,
           kSharedMemoryId, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, kWidth, kHeight, GL_RGBA,
           GL_UNSIGNED_SHORT_4_4_4_4, kSharedMemoryId, kSharedMemoryOffset,
           GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
           kInvalidSharedMemoryId, kSharedMemoryOffset, GL_FALSE);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
           kSharedMemoryId, kInvalidSharedMemoryOffset, GL_FALSE);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, CopyTexSubImage2DValidArgs) {
  const int kWidth = 16;
  const int kHeight = 8;
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 1, GL_RGBA, kWidth, kHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
      kSharedMemoryId, kSharedMemoryOffset);
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
  DoDeleteFramebuffer(
      client_framebuffer_id_, kServiceFramebufferId,
      true, GL_FRAMEBUFFER, 0,
      true, GL_FRAMEBUFFER, 0);
  EXPECT_FALSE(DoIsFramebuffer(client_framebuffer_id_));
}

TEST_F(GLES2DecoderTest, IsProgram) {
  // IsProgram is true as soon as the program is created.
  EXPECT_TRUE(DoIsProgram(client_program_id_));
  EXPECT_CALL(*gl_, DeleteProgram(kServiceProgramId))
      .Times(1)
      .RetiresOnSaturation();
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
          for (size_t ss = 0; ss < arraysize(stride_mult); ++ss) {
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

// Test that with an RGB backbuffer if we set the color mask to 1,1,1,1 it is
// set to 1,1,1,0 at Draw time but is 1,1,1,1 at query time.
TEST_F(GLES2DecoderRGBBackbufferTest, RGBBackbufferColorMask) {
  ColorMask cmd;
  cmd.Init(true, true, true, true);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  SetupTexture();
  AddExpectationsForSimulatedAttrib0(kNumVertices, 0);
  SetupExpectationsForApplyingDirtyState(
      true,    // Framebuffer is RGB
      false,   // Framebuffer has depth
      false,   // Framebuffer has stencil
      0x1110,  // color bits
      false,   // depth mask
      false,   // depth enabled
      0,       // front stencil mask
      0,       // back stencil mask
      false,   // stencil enabled
      false,   // cull_face_enabled
      false,   // scissor_test_enabled
      false);  // blend_enabled

  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  DrawArrays draw_cmd;
  draw_cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(draw_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  typedef GetIntegerv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_COLOR_WRITEMASK, result->GetData()))
      .Times(0);
  result->size = 0;
  GetIntegerv cmd2;
  cmd2.Init(GL_COLOR_WRITEMASK, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(
      decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_COLOR_WRITEMASK),
      result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(1, result->GetData()[0]);
  EXPECT_EQ(1, result->GetData()[1]);
  EXPECT_EQ(1, result->GetData()[2]);
  EXPECT_EQ(1, result->GetData()[3]);
}

// Test that with no depth if we set DepthMask true that it's set to false at
// draw time but querying it returns true.
TEST_F(GLES2DecoderRGBBackbufferTest, RGBBackbufferDepthMask) {
  EXPECT_CALL(*gl_, DepthMask(true))
      .Times(0)
      .RetiresOnSaturation();
  DepthMask cmd;
  cmd.Init(true);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  SetupTexture();
  AddExpectationsForSimulatedAttrib0(kNumVertices, 0);
  SetupExpectationsForApplyingDirtyState(
      true,    // Framebuffer is RGB
      false,   // Framebuffer has depth
      false,   // Framebuffer has stencil
      0x1110,  // color bits
      false,   // depth mask
      false,   // depth enabled
      0,       // front stencil mask
      0,       // back stencil mask
      false,   // stencil enabled
      false,   // cull_face_enabled
      false,   // scissor_test_enabled
      false);  // blend_enabled

  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  DrawArrays draw_cmd;
  draw_cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(draw_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  typedef GetIntegerv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_DEPTH_WRITEMASK, result->GetData()))
      .Times(0);
  result->size = 0;
  GetIntegerv cmd2;
  cmd2.Init(GL_DEPTH_WRITEMASK, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(
      decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_DEPTH_WRITEMASK),
      result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(1, result->GetData()[0]);
}

// Test that with no stencil if we set the stencil mask it's still set to 0 at
// draw time but gets our value if we query.
TEST_F(GLES2DecoderRGBBackbufferTest, RGBBackbufferStencilMask) {
  const GLint kMask = 123;
  EXPECT_CALL(*gl_, StencilMask(kMask))
      .Times(0)
      .RetiresOnSaturation();
  StencilMask cmd;
  cmd.Init(kMask);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  SetupTexture();
  AddExpectationsForSimulatedAttrib0(kNumVertices, 0);
  SetupExpectationsForApplyingDirtyState(
      true,    // Framebuffer is RGB
      false,   // Framebuffer has depth
      false,   // Framebuffer has stencil
      0x1110,  // color bits
      false,   // depth mask
      false,   // depth enabled
      0,       // front stencil mask
      0,       // back stencil mask
      false,   // stencil enabled
      false,   // cull_face_enabled
      false,   // scissor_test_enabled
      false);  // blend_enabled

  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  DrawArrays draw_cmd;
  draw_cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(draw_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  typedef GetIntegerv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_STENCIL_WRITEMASK, result->GetData()))
      .Times(0);
  result->size = 0;
  GetIntegerv cmd2;
  cmd2.Init(GL_STENCIL_WRITEMASK, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(
      decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_STENCIL_WRITEMASK),
      result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(kMask, result->GetData()[0]);
}

// Test that if an FBO is bound we get the correct masks.
TEST_F(GLES2DecoderRGBBackbufferTest, RGBBackbufferColorMaskFBO) {
  ColorMask cmd;
  cmd.Init(true, true, true, true);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  SetupTexture();
  SetupVertexBuffer();
  DoEnableVertexAttribArray(0);
  DoVertexAttribPointer(0, 2, GL_FLOAT, 0, 0);
  DoEnableVertexAttribArray(1);
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);
  DoEnableVertexAttribArray(2);
  DoVertexAttribPointer(2, 2, GL_FLOAT, 0, 0);
  SetupExpectationsForApplyingDirtyState(
      true,    // Framebuffer is RGB
      false,   // Framebuffer has depth
      false,   // Framebuffer has stencil
      0x1110,  // color bits
      false,   // depth mask
      false,   // depth enabled
      0,       // front stencil mask
      0,       // back stencil mask
      false,   // stencil enabled
      false,   // cull_face_enabled
      false,   // scissor_test_enabled
      false);  // blend_enabled

  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  DrawArrays draw_cmd;
  draw_cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(draw_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Check that no extra calls are made on the next draw.
  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(draw_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Setup Frame buffer.
  // needs to be 1x1 or else it's not renderable.
  const GLsizei kWidth = 1;
  const GLsizei kHeight = 1;
  const GLenum kFormat = GL_RGB;
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  // Pass some data so the texture will be marked as cleared.
  DoTexImage2D(
      GL_TEXTURE_2D, 0, kFormat, kWidth, kHeight, 0,
      kFormat, GL_UNSIGNED_BYTE, kSharedMemoryId, kSharedMemoryOffset);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoFramebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
      client_texture_id_, kServiceTextureId, 0, GL_NO_ERROR);
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_FRAMEBUFFER))
      .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
      .RetiresOnSaturation();

  // This time state needs to be set.
  SetupExpectationsForApplyingDirtyState(
      false,    // Framebuffer is RGB
      false,   // Framebuffer has depth
      false,   // Framebuffer has stencil
      0x1110,  // color bits
      false,   // depth mask
      false,   // depth enabled
      0,       // front stencil mask
      0,       // back stencil mask
      false,   // stencil enabled
      false,   // cull_face_enabled
      false,   // scissor_test_enabled
      false);  // blend_enabled

  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(draw_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Check that no extra calls are made on the next draw.
  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(draw_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Unbind
  DoBindFramebuffer(GL_FRAMEBUFFER, 0, 0);

  SetupExpectationsForApplyingDirtyState(
      true,    // Framebuffer is RGB
      false,   // Framebuffer has depth
      false,   // Framebuffer has stencil
      0x1110,  // color bits
      false,   // depth mask
      false,   // depth enabled
      0,       // front stencil mask
      0,       // back stencil mask
      false,   // stencil enabled
      false,   // cull_face_enabled
      false,   // scissor_test_enabled
      false);  // blend_enabled

  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(draw_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderManualInitTest, ActualAlphaMatchesRequestedAlpha) {
  InitDecoder(
      "",      // extensions
      true,    // has alpha
      false,   // has depth
      false,   // has stencil
      true,    // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  typedef GetIntegerv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_ALPHA_BITS, _))
      .WillOnce(SetArgumentPointee<1>(8))
      .RetiresOnSaturation();
  result->size = 0;
  GetIntegerv cmd2;
  cmd2.Init(GL_ALPHA_BITS, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(
      decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_ALPHA_BITS),
      result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(8, result->GetData()[0]);
}

TEST_F(GLES2DecoderManualInitTest, ActualAlphaDoesNotMatchRequestedAlpha) {
  InitDecoder(
      "",      // extensions
      true,    // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  typedef GetIntegerv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_ALPHA_BITS, _))
      .WillOnce(SetArgumentPointee<1>(8))
      .RetiresOnSaturation();
  result->size = 0;
  GetIntegerv cmd2;
  cmd2.Init(GL_ALPHA_BITS, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(
      decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_ALPHA_BITS),
      result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(0, result->GetData()[0]);
}

TEST_F(GLES2DecoderManualInitTest, ActualDepthMatchesRequestedDepth) {
  InitDecoder(
      "",      // extensions
      false,   // has alpha
      true,    // has depth
      false,   // has stencil
      false,   // request alpha
      true,    // request depth
      false,   // request stencil
      true);   // bind generates resource

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  typedef GetIntegerv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_DEPTH_BITS, _))
      .WillOnce(SetArgumentPointee<1>(24))
      .RetiresOnSaturation();
  result->size = 0;
  GetIntegerv cmd2;
  cmd2.Init(GL_DEPTH_BITS, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(
      decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_DEPTH_BITS),
      result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(24, result->GetData()[0]);
}

TEST_F(GLES2DecoderManualInitTest, ActualDepthDoesNotMatchRequestedDepth) {
  InitDecoder(
      "",      // extensions
      false,   // has alpha
      true,    // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  typedef GetIntegerv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_DEPTH_BITS, _))
      .WillOnce(SetArgumentPointee<1>(24))
      .RetiresOnSaturation();
  result->size = 0;
  GetIntegerv cmd2;
  cmd2.Init(GL_DEPTH_BITS, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(
      decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_DEPTH_BITS),
      result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(0, result->GetData()[0]);
}

TEST_F(GLES2DecoderManualInitTest, ActualStencilMatchesRequestedStencil) {
  InitDecoder(
      "",      // extensions
      false,   // has alpha
      false,   // has depth
      true,    // has stencil
      false,   // request alpha
      false,   // request depth
      true,    // request stencil
      true);   // bind generates resource

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  typedef GetIntegerv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_STENCIL_BITS, _))
      .WillOnce(SetArgumentPointee<1>(8))
      .RetiresOnSaturation();
  result->size = 0;
  GetIntegerv cmd2;
  cmd2.Init(GL_STENCIL_BITS, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(
      decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_STENCIL_BITS),
      result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(8, result->GetData()[0]);
}

TEST_F(GLES2DecoderManualInitTest, ActualStencilDoesNotMatchRequestedStencil) {
  InitDecoder(
      "",      // extensions
      false,   // has alpha
      false,   // has depth
      true,    // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  typedef GetIntegerv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_STENCIL_BITS, _))
      .WillOnce(SetArgumentPointee<1>(8))
      .RetiresOnSaturation();
  result->size = 0;
  GetIntegerv cmd2;
  cmd2.Init(GL_STENCIL_BITS, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(
      decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_STENCIL_BITS),
      result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(0, result->GetData()[0]);
}

TEST_F(GLES2DecoderManualInitTest, DepthEnableWithDepth) {
  InitDecoder(
      "",      // extensions
      false,   // has alpha
      true,    // has depth
      false,   // has stencil
      false,   // request alpha
      true,    // request depth
      false,   // request stencil
      true);   // bind generates resource

  Enable cmd;
  cmd.Init(GL_DEPTH_TEST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  SetupDefaultProgram();
  SetupTexture();
  AddExpectationsForSimulatedAttrib0(kNumVertices, 0);
  SetupExpectationsForApplyingDirtyState(
      true,    // Framebuffer is RGB
      true,    // Framebuffer has depth
      false,   // Framebuffer has stencil
      0x1110,  // color bits
      true,    // depth mask
      true,    // depth enabled
      0,       // front stencil mask
      0,       // back stencil mask
      false,   // stencil enabled
      false,   // cull_face_enabled
      false,   // scissor_test_enabled
      false);  // blend_enabled


  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  DrawArrays draw_cmd;
  draw_cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(draw_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  typedef GetIntegerv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_DEPTH_TEST, _))
      .Times(0)
      .RetiresOnSaturation();
  result->size = 0;
  GetIntegerv cmd2;
  cmd2.Init(GL_DEPTH_TEST, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(
      decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_DEPTH_TEST),
      result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(1, result->GetData()[0]);
}

TEST_F(GLES2DecoderManualInitTest, DepthEnableWithoutRequestedDepth) {
  InitDecoder(
      "",      // extensions
      false,   // has alpha
      true,    // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  Enable cmd;
  cmd.Init(GL_DEPTH_TEST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  SetupDefaultProgram();
  SetupTexture();
  AddExpectationsForSimulatedAttrib0(kNumVertices, 0);
  SetupExpectationsForApplyingDirtyState(
      true,    // Framebuffer is RGB
      false,   // Framebuffer has depth
      false,   // Framebuffer has stencil
      0x1110,  // color bits
      false,   // depth mask
      false,   // depth enabled
      0,       // front stencil mask
      0,       // back stencil mask
      false,   // stencil enabled
      false,   // cull_face_enabled
      false,   // scissor_test_enabled
      false);  // blend_enabled

  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  DrawArrays draw_cmd;
  draw_cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(draw_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  typedef GetIntegerv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_DEPTH_TEST, _))
      .Times(0)
      .RetiresOnSaturation();
  result->size = 0;
  GetIntegerv cmd2;
  cmd2.Init(GL_DEPTH_TEST, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(
      decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_DEPTH_TEST),
      result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(1, result->GetData()[0]);
}

TEST_F(GLES2DecoderManualInitTest, StencilEnableWithStencil) {
  InitDecoder(
      "",      // extensions
      false,   // has alpha
      false,   // has depth
      true,    // has stencil
      false,   // request alpha
      false,   // request depth
      true,    // request stencil
      true);   // bind generates resource

  Enable cmd;
  cmd.Init(GL_STENCIL_TEST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  SetupDefaultProgram();
  SetupTexture();
  AddExpectationsForSimulatedAttrib0(kNumVertices, 0);
  SetupExpectationsForApplyingDirtyState(
      true,    // Framebuffer is RGB
      false,   // Framebuffer has depth
      true,    // Framebuffer has stencil
      0x1110,  // color bits
      false,   // depth mask
      false,   // depth enabled
      -1,      // front stencil mask
      -1,      // back stencil mask
      true,    // stencil enabled
      false,   // cull_face_enabled
      false,   // scissor_test_enabled
      false);  // blend_enabled

  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  DrawArrays draw_cmd;
  draw_cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(draw_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  typedef GetIntegerv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_STENCIL_TEST, _))
      .Times(0)
      .RetiresOnSaturation();
  result->size = 0;
  GetIntegerv cmd2;
  cmd2.Init(GL_STENCIL_TEST, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(
      decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_STENCIL_TEST),
      result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(1, result->GetData()[0]);
}

TEST_F(GLES2DecoderManualInitTest, StencilEnableWithoutRequestedStencil) {
  InitDecoder(
      "",      // extensions
      false,   // has alpha
      false,   // has depth
      true,    // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  Enable cmd;
  cmd.Init(GL_STENCIL_TEST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  SetupDefaultProgram();
  SetupTexture();
  AddExpectationsForSimulatedAttrib0(kNumVertices, 0);
  SetupExpectationsForApplyingDirtyState(
      true,    // Framebuffer is RGB
      false,   // Framebuffer has depth
      false,   // Framebuffer has stencil
      0x1110,  // color bits
      false,   // depth mask
      false,   // depth enabled
      0,       // front stencil mask
      0,       // back stencil mask
      false,   // stencil enabled
      false,   // cull_face_enabled
      false,   // scissor_test_enabled
      false);  // blend_enabled

  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  DrawArrays draw_cmd;
  draw_cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(draw_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  typedef GetIntegerv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_STENCIL_TEST, _))
      .Times(0)
      .RetiresOnSaturation();
  result->size = 0;
  GetIntegerv cmd2;
  cmd2.Init(GL_STENCIL_TEST, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(
      decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_STENCIL_TEST),
      result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(1, result->GetData()[0]);
}

TEST_F(GLES2DecoderManualInitTest, PackedDepthStencilReportsCorrectValues) {
  InitDecoder(
      "GL_OES_packed_depth_stencil",      // extensions
      false,   // has alpha
      true,    // has depth
      true,    // has stencil
      false,   // request alpha
      true,    // request depth
      true,    // request stencil
      true);   // bind generates resource

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  typedef GetIntegerv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->size = 0;
  GetIntegerv cmd2;
  cmd2.Init(GL_STENCIL_BITS, shared_memory_id_, shared_memory_offset_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_STENCIL_BITS, _))
      .WillOnce(SetArgumentPointee<1>(8))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(
      decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_STENCIL_BITS),
      result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(8, result->GetData()[0]);
  result->size = 0;
  cmd2.Init(GL_DEPTH_BITS, shared_memory_id_, shared_memory_offset_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_DEPTH_BITS, _))
      .WillOnce(SetArgumentPointee<1>(24))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(
      decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_DEPTH_BITS),
      result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(24, result->GetData()[0]);
}

TEST_F(GLES2DecoderManualInitTest, PackedDepthStencilNoRequestedStencil) {
  InitDecoder(
      "GL_OES_packed_depth_stencil",      // extensions
      false,   // has alpha
      true,    // has depth
      true,    // has stencil
      false,   // request alpha
      true,    // request depth
      false,   // request stencil
      true);   // bind generates resource

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  typedef GetIntegerv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->size = 0;
  GetIntegerv cmd2;
  cmd2.Init(GL_STENCIL_BITS, shared_memory_id_, shared_memory_offset_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_STENCIL_BITS, _))
      .WillOnce(SetArgumentPointee<1>(8))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(
      decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_STENCIL_BITS),
      result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(0, result->GetData()[0]);
  result->size = 0;
  cmd2.Init(GL_DEPTH_BITS, shared_memory_id_, shared_memory_offset_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_DEPTH_BITS, _))
      .WillOnce(SetArgumentPointee<1>(24))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(
      decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_DEPTH_BITS),
      result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(24, result->GetData()[0]);
}

TEST_F(GLES2DecoderManualInitTest, PackedDepthStencilRenderbufferDepth) {
  InitDecoder(
      "GL_OES_packed_depth_stencil",      // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource
  DoBindRenderbuffer(GL_RENDERBUFFER, client_renderbuffer_id_,
                    kServiceRenderbufferId);
  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))  // for RenderbufferStoage
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))  // for FramebufferRenderbuffer
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))  // for GetIntegerv
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))  // for GetIntegerv
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();

  EXPECT_CALL(*gl_, RenderbufferStorageEXT(
      GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 100, 50))
      .Times(1)
      .RetiresOnSaturation();
  RenderbufferStorage cmd;
  cmd.Init(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 100, 50);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_CALL(*gl_, FramebufferRenderbufferEXT(
      GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
      kServiceRenderbufferId))
      .Times(1)
      .RetiresOnSaturation();
  FramebufferRenderbuffer fbrb_cmd;
  fbrb_cmd.Init(
      GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
      client_renderbuffer_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(fbrb_cmd));

  typedef GetIntegerv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->size = 0;
  GetIntegerv cmd2;
  cmd2.Init(GL_STENCIL_BITS, shared_memory_id_, shared_memory_offset_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_STENCIL_BITS, _))
      .WillOnce(SetArgumentPointee<1>(8))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(
      decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_STENCIL_BITS),
      result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(0, result->GetData()[0]);
  result->size = 0;
  cmd2.Init(GL_DEPTH_BITS, shared_memory_id_, shared_memory_offset_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_DEPTH_BITS, _))
      .WillOnce(SetArgumentPointee<1>(24))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(
      decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_DEPTH_BITS),
      result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(24, result->GetData()[0]);
}

TEST_F(GLES2DecoderManualInitTest, PackedDepthStencilRenderbufferStencil) {
  InitDecoder(
      "GL_OES_packed_depth_stencil",      // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource
  DoBindRenderbuffer(GL_RENDERBUFFER, client_renderbuffer_id_,
                    kServiceRenderbufferId);
  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))  // for RenderbufferStoage
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))  // for FramebufferRenderbuffer
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))  // for GetIntegerv
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))  // for GetIntegerv
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();

  EXPECT_CALL(*gl_, RenderbufferStorageEXT(
      GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 100, 50))
      .Times(1)
      .RetiresOnSaturation();
  RenderbufferStorage cmd;
  cmd.Init(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 100, 50);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_CALL(*gl_, FramebufferRenderbufferEXT(
      GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
      kServiceRenderbufferId))
      .Times(1)
      .RetiresOnSaturation();
  FramebufferRenderbuffer fbrb_cmd;
  fbrb_cmd.Init(
      GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
      client_renderbuffer_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(fbrb_cmd));

  typedef GetIntegerv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->size = 0;
  GetIntegerv cmd2;
  cmd2.Init(GL_STENCIL_BITS, shared_memory_id_, shared_memory_offset_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_STENCIL_BITS, _))
      .WillOnce(SetArgumentPointee<1>(8))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(
      decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_STENCIL_BITS),
      result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(8, result->GetData()[0]);
  result->size = 0;
  cmd2.Init(GL_DEPTH_BITS, shared_memory_id_, shared_memory_offset_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_DEPTH_BITS, _))
      .WillOnce(SetArgumentPointee<1>(24))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(
      decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_DEPTH_BITS),
      result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(0, result->GetData()[0]);
}

TEST_F(GLES2DecoderTest, GetMultipleIntegervCHROMIUMValidArgs) {
  const GLsizei kCount = 3;
  GLenum* pnames = GetSharedMemoryAs<GLenum*>();
  pnames[0] = GL_DEPTH_WRITEMASK;
  pnames[1] = GL_COLOR_WRITEMASK;
  pnames[2] = GL_STENCIL_WRITEMASK;
  GLint* results =
      GetSharedMemoryAsWithOffset<GLint*>(sizeof(*pnames) * kCount);

  GLsizei num_results = 0;
  for (GLsizei ii = 0; ii < kCount; ++ii) {
    num_results += decoder_->GetGLES2Util()->GLGetNumValuesReturned(pnames[ii]);
  }
  const GLsizei result_size = num_results * sizeof(*results);
  memset(results,  0, result_size);

  const GLint kSentinel = 0x12345678;
  results[num_results] = kSentinel;

  GetMultipleIntegervCHROMIUM cmd;
  cmd.Init(
      kSharedMemoryId, kSharedMemoryOffset, kCount,
      kSharedMemoryId, kSharedMemoryOffset + sizeof(*pnames) * kCount,
      result_size);

  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(1, results[0]);  // Depth writemask
  EXPECT_EQ(1, results[1]);  // color writemask red
  EXPECT_EQ(1, results[2]);  // color writemask green
  EXPECT_EQ(1, results[3]);  // color writemask blue
  EXPECT_EQ(1, results[4]);  // color writemask alpha
  EXPECT_EQ(-1, results[5]);  // stencil writemask alpha
  EXPECT_EQ(kSentinel, results[num_results]);  // End of results
}

TEST_F(GLES2DecoderTest, GetMultipleIntegervCHROMIUMInvalidArgs) {
  const GLsizei kCount = 3;
  // Offset the pnames because GLGetError will use the first uint32.
  const uint32 kPnameOffset = sizeof(uint32);
  const uint32 kResultsOffset = kPnameOffset + sizeof(GLint) * kCount;
  GLenum* pnames = GetSharedMemoryAsWithOffset<GLenum*>(kPnameOffset);
  pnames[0] = GL_DEPTH_WRITEMASK;
  pnames[1] = GL_COLOR_WRITEMASK;
  pnames[2] = GL_STENCIL_WRITEMASK;
  GLint* results = GetSharedMemoryAsWithOffset<GLint*>(kResultsOffset);

  GLsizei num_results = 0;
  for (GLsizei ii = 0; ii < kCount; ++ii) {
    num_results += decoder_->GetGLES2Util()->GLGetNumValuesReturned(pnames[ii]);
  }
  const GLsizei result_size = num_results * sizeof(*results);
  memset(results,  0, result_size);

  const GLint kSentinel = 0x12345678;
  results[num_results] = kSentinel;

  GetMultipleIntegervCHROMIUM cmd;
  // Check bad pnames pointer.
  cmd.Init(
      kInvalidSharedMemoryId, kSharedMemoryOffset + kPnameOffset, kCount,
      kSharedMemoryId, kSharedMemoryOffset + kResultsOffset,
      result_size);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  // Check bad pnames pointer.
  cmd.Init(
      kSharedMemoryId, kInvalidSharedMemoryOffset, kCount,
      kSharedMemoryId, kSharedMemoryOffset + kResultsOffset,
      result_size);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  // Check bad count.
  cmd.Init(
      kSharedMemoryId, kSharedMemoryOffset + kPnameOffset, -1,
      kSharedMemoryId, kSharedMemoryOffset + kResultsOffset,
      result_size);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  // Check bad results pointer.
  cmd.Init(
      kSharedMemoryId, kSharedMemoryOffset + kPnameOffset, kCount,
      kInvalidSharedMemoryId, kSharedMemoryOffset + kResultsOffset,
      result_size);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  // Check bad results pointer.
  cmd.Init(
      kSharedMemoryId, kSharedMemoryOffset + kPnameOffset, kCount,
      kSharedMemoryId, kInvalidSharedMemoryOffset,
      result_size);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  // Check bad size.
  cmd.Init(
      kSharedMemoryId, kSharedMemoryOffset + kPnameOffset, kCount,
      kSharedMemoryId, kSharedMemoryOffset + kResultsOffset,
      result_size + 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  // Check bad size.
  cmd.Init(
      kSharedMemoryId, kSharedMemoryOffset + kPnameOffset, kCount,
      kSharedMemoryId, kSharedMemoryOffset + kResultsOffset,
      result_size - 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  // Check bad enum.
  cmd.Init(
      kSharedMemoryId, kSharedMemoryOffset + kPnameOffset, kCount,
      kSharedMemoryId, kSharedMemoryOffset + kResultsOffset,
      result_size);
  GLenum temp = pnames[2];
  pnames[2] = GL_TRUE;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  pnames[2] = temp;
  // Check results area has not been cleared by client.
  results[1] = 1;
  EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
  // Check buffer is what we expect
  EXPECT_EQ(0, results[0]);
  EXPECT_EQ(1, results[1]);
  EXPECT_EQ(0, results[2]);
  EXPECT_EQ(0, results[3]);
  EXPECT_EQ(0, results[4]);
  EXPECT_EQ(0, results[5]);
  EXPECT_EQ(kSentinel, results[num_results]);  // End of results
}

TEST_F(GLES2DecoderTest, TexImage2DRedefinitionSucceeds) {
  const int kWidth = 16;
  const int kHeight = 8;
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  EXPECT_CALL(*gl_, GetError())
      .WillRepeatedly(Return(GL_NO_ERROR));
  for (int ii = 0; ii < 2; ++ii) {
    TexImage2D cmd;
    if (ii == 0) {
      EXPECT_CALL(*gl_, TexImage2D(
          GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA,
          GL_UNSIGNED_BYTE, _))
          .Times(1)
          .RetiresOnSaturation();
      cmd.Init(
          GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA,
          GL_UNSIGNED_BYTE, kSharedMemoryId, kSharedMemoryOffset);
    } else {
      SetupClearTextureExpections(
          kServiceTextureId, kServiceTextureId, GL_TEXTURE_2D, GL_TEXTURE_2D,
          0, GL_RGBA, GL_UNSIGNED_BYTE, kWidth, kHeight);
      cmd.Init(
          GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA,
          GL_UNSIGNED_BYTE, 0, 0);
    }
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_CALL(*gl_, TexSubImage2D(
        GL_TEXTURE_2D, 0, 0, 0, kWidth, kHeight - 1, GL_RGBA, GL_UNSIGNED_BYTE,
        shared_memory_address_))
        .Times(1)
        .RetiresOnSaturation();
    // Consider this TexSubImage2D command part of the previous TexImage2D
    // (last GL_TRUE argument). It will be skipped if there are bugs in the
    // redefinition case.
    TexSubImage2D cmd2;
    cmd2.Init(
        GL_TEXTURE_2D, 0, 0, 0, kWidth, kHeight - 1, GL_RGBA, GL_UNSIGNED_BYTE,
        kSharedMemoryId, kSharedMemoryOffset, GL_TRUE);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  }
}

TEST_F(GLES2DecoderTest, TexImage2DGLError) {
  GLenum target = GL_TEXTURE_2D;
  GLint level = 0;
  GLenum internal_format = GL_RGBA;
  GLsizei width = 2;
  GLsizei height = 4;
  GLint border = 0;
  GLenum format = GL_RGBA;
  GLenum type = GL_UNSIGNED_BYTE;
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  TextureManager* manager = group().texture_manager();
  Texture* info =
      manager->GetTexture(client_texture_id_);
  ASSERT_TRUE(info != NULL);
  EXPECT_FALSE(info->GetLevelSize(GL_TEXTURE_2D, level, &width, &height));
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_OUT_OF_MEMORY))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, TexImage2D(target, level, internal_format,
                               width, height, border, format, type, _))
      .Times(1)
      .RetiresOnSaturation();
  TexImage2D cmd;
  cmd.Init(target, level, internal_format, width, height, border, format,
           type, kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
  EXPECT_FALSE(info->GetLevelSize(GL_TEXTURE_2D, level, &width, &height));
}

TEST_F(GLES2DecoderTest, BufferDataGLError) {
  GLenum target = GL_ARRAY_BUFFER;
  GLsizeiptr size = 4;
  DoBindBuffer(GL_ARRAY_BUFFER, client_buffer_id_, kServiceBufferId);
  BufferManager* manager = group().buffer_manager();
  Buffer* info =
      manager->GetBuffer(client_buffer_id_);
  ASSERT_TRUE(info != NULL);
  EXPECT_EQ(0, info->size());
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_OUT_OF_MEMORY))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BufferData(target, size, _, GL_STREAM_DRAW))
      .Times(1)
      .RetiresOnSaturation();
  BufferData cmd;
  cmd.Init(target, size, 0, 0, GL_STREAM_DRAW);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
  EXPECT_EQ(0, info->size());
}

TEST_F(GLES2DecoderTest, CopyTexImage2DGLError) {
  GLenum target = GL_TEXTURE_2D;
  GLint level = 0;
  GLenum internal_format = GL_RGBA;
  GLsizei width = 2;
  GLsizei height = 4;
  GLint border = 0;
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  TextureManager* manager = group().texture_manager();
  Texture* info =
      manager->GetTexture(client_texture_id_);
  ASSERT_TRUE(info != NULL);
  EXPECT_FALSE(info->GetLevelSize(GL_TEXTURE_2D, level, &width, &height));
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_OUT_OF_MEMORY))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, CopyTexImage2D(
      target, level, internal_format, 0, 0, width, height, border))
      .Times(1)
      .RetiresOnSaturation();
  CopyTexImage2D cmd;
  cmd.Init(target, level, internal_format, 0, 0, width, height, border);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
  EXPECT_FALSE(info->GetLevelSize(GL_TEXTURE_2D, level, &width, &height));
}

TEST_F(GLES2DecoderTest, FramebufferRenderbufferGLError) {
  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_OUT_OF_MEMORY))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, FramebufferRenderbufferEXT(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
      kServiceRenderbufferId))
      .Times(1)
      .RetiresOnSaturation();
  FramebufferRenderbuffer cmd;
  cmd.Init(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
      client_renderbuffer_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
}

TEST_F(GLES2DecoderTest, FramebufferTexture2DGLError) {
  const GLsizei kWidth = 5;
  const GLsizei kHeight = 3;
  const GLenum kFormat = GL_RGB;
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, kFormat, kWidth, kHeight, 0,
               kFormat, GL_UNSIGNED_BYTE, 0, 0);
  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_OUT_OF_MEMORY))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, FramebufferTexture2DEXT(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
      kServiceTextureId, 0))
      .Times(1)
      .RetiresOnSaturation();
  FramebufferTexture2D fbtex_cmd;
  fbtex_cmd.Init(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, client_texture_id_,
      0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(fbtex_cmd));
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
}

TEST_F(GLES2DecoderTest, RenderbufferStorageGLError) {
  DoBindRenderbuffer(GL_RENDERBUFFER, client_renderbuffer_id_,
                    kServiceRenderbufferId);
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_OUT_OF_MEMORY))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, RenderbufferStorageEXT(
      GL_RENDERBUFFER, GL_RGBA, 100, 50))
      .Times(1)
      .RetiresOnSaturation();
  RenderbufferStorage cmd;
  cmd.Init(GL_RENDERBUFFER, GL_RGBA4, 100, 50);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
}

TEST_F(GLES2DecoderTest, RenderbufferStorageBadArgs) {
  DoBindRenderbuffer(GL_RENDERBUFFER, client_renderbuffer_id_,
                    kServiceRenderbufferId);
  EXPECT_CALL(*gl_, RenderbufferStorageEXT(_, _, _, _))
      .Times(0)
      .RetiresOnSaturation();
  RenderbufferStorage cmd;
  cmd.Init(GL_RENDERBUFFER, GL_RGBA4, TestHelper::kMaxRenderbufferSize + 1, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_RENDERBUFFER, GL_RGBA4, 1, TestHelper::kMaxRenderbufferSize + 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderManualInitTest, RenderbufferStorageMultisampleGLError) {
  InitDecoder(
      "GL_EXT_framebuffer_multisample",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource
  DoBindRenderbuffer(GL_RENDERBUFFER, client_renderbuffer_id_,
                    kServiceRenderbufferId);
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_OUT_OF_MEMORY))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, RenderbufferStorageMultisampleEXT(
      GL_RENDERBUFFER, 1, GL_RGBA, 100, 50))
      .Times(1)
      .RetiresOnSaturation();
  RenderbufferStorageMultisampleEXT cmd;
  cmd.Init(GL_RENDERBUFFER, 1, GL_RGBA4, 100, 50);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
}

TEST_F(GLES2DecoderManualInitTest, RenderbufferStorageMultisampleBadArgs) {
  InitDecoder(
      "GL_EXT_framebuffer_multisample",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource
  DoBindRenderbuffer(GL_RENDERBUFFER, client_renderbuffer_id_,
                    kServiceRenderbufferId);
  EXPECT_CALL(*gl_, RenderbufferStorageMultisampleEXT(_, _, _, _, _))
      .Times(0)
      .RetiresOnSaturation();
  RenderbufferStorageMultisampleEXT cmd;
  cmd.Init(GL_RENDERBUFFER, TestHelper::kMaxSamples + 1,
           GL_RGBA4, TestHelper::kMaxRenderbufferSize, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_RENDERBUFFER, TestHelper::kMaxSamples,
           GL_RGBA4, TestHelper::kMaxRenderbufferSize + 1, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_RENDERBUFFER, TestHelper::kMaxSamples,
           GL_RGBA4, 1, TestHelper::kMaxRenderbufferSize + 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderTest, ReadPixelsGLError) {
  GLenum kFormat = GL_RGBA;
  GLint x = 0;
  GLint y = 0;
  GLsizei width = 2;
  GLsizei height = 4;
  typedef ReadPixels::Result Result;
  Result* result = GetSharedMemoryAs<Result*>();
  uint32 result_shm_id = kSharedMemoryId;
  uint32 result_shm_offset = kSharedMemoryOffset;
  uint32 pixels_shm_id = kSharedMemoryId;
  uint32 pixels_shm_offset = kSharedMemoryOffset + sizeof(*result);
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_OUT_OF_MEMORY))
      .RetiresOnSaturation();
  EXPECT_CALL(
      *gl_, ReadPixels(x, y, width, height, kFormat, GL_UNSIGNED_BYTE, _))
      .Times(1)
      .RetiresOnSaturation();
  ReadPixels cmd;
  cmd.Init(x, y, width, height, kFormat, GL_UNSIGNED_BYTE,
           pixels_shm_id, pixels_shm_offset,
           result_shm_id, result_shm_offset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
}

static bool ValueInArray(GLint value, GLint* array, GLint count) {
  for (GLint ii = 0; ii < count; ++ii) {
    if (array[ii] == value) {
      return true;
    }
  }
  return false;
}

TEST_F(GLES2DecoderManualInitTest, GetCompressedTextureFormats) {
  InitDecoder(
      "GL_EXT_texture_compression_s3tc",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();

  typedef GetIntegerv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  GetIntegerv cmd;
  result->size = 0;
  EXPECT_CALL(*gl_, GetIntegerv(_, _))
      .Times(0)
      .RetiresOnSaturation();
  cmd.Init(
      GL_NUM_COMPRESSED_TEXTURE_FORMATS,
      shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(1, result->GetNumResults());
  GLint num_formats = result->GetData()[0];
  EXPECT_EQ(4, num_formats);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  result->size = 0;
  cmd.Init(
      GL_COMPRESSED_TEXTURE_FORMATS,
      shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(num_formats, result->GetNumResults());

  EXPECT_TRUE(ValueInArray(
      GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
      result->GetData(), result->GetNumResults()));
  EXPECT_TRUE(ValueInArray(
      GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
      result->GetData(), result->GetNumResults()));
  EXPECT_TRUE(ValueInArray(
      GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,
      result->GetData(), result->GetNumResults()));
  EXPECT_TRUE(ValueInArray(
      GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
      result->GetData(), result->GetNumResults()));

  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderManualInitTest, GetNoCompressedTextureFormats) {
  InitDecoder(
      "",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();

  typedef GetIntegerv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  GetIntegerv cmd;
  result->size = 0;
  EXPECT_CALL(*gl_, GetIntegerv(_, _))
      .Times(0)
      .RetiresOnSaturation();
  cmd.Init(
      GL_NUM_COMPRESSED_TEXTURE_FORMATS,
      shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(1, result->GetNumResults());
  GLint num_formats = result->GetData()[0];
  EXPECT_EQ(0, num_formats);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  result->size = 0;
  cmd.Init(
      GL_COMPRESSED_TEXTURE_FORMATS,
      shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(num_formats, result->GetNumResults());

  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderManualInitTest, CompressedTexImage2DBucketBadBucket) {
  InitDecoder(
      "GL_EXT_texture_compression_s3tc",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  const uint32 kBadBucketId = 123;
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  CompressedTexImage2DBucket cmd;
  cmd.Init(
      GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 4, 4, 0,
      kBadBucketId);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  CompressedTexSubImage2DBucket cmd2;
  cmd2.Init(
      GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
      kBadBucketId);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

namespace {

struct S3TCTestData {
  GLenum format;
  size_t block_size;
};

}  // anonymous namespace.

TEST_F(GLES2DecoderManualInitTest, CompressedTexImage2DS3TC) {
  InitDecoder(
      "GL_EXT_texture_compression_s3tc",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource
  const uint32 kBucketId = 123;
  CommonDecoder::Bucket* bucket = decoder_->CreateBucket(kBucketId);
  ASSERT_TRUE(bucket != NULL);

  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);

  static const S3TCTestData test_data[] = {
    { GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 8, },
    { GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 8, },
    { GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 16, },
    { GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 16, },
  };

  for (size_t ii = 0; ii < arraysize(test_data); ++ii) {
    const S3TCTestData& test = test_data[ii];
    CompressedTexImage2DBucket cmd;
    // test small width.
    DoCompressedTexImage2D(
        GL_TEXTURE_2D, 0, test.format, 2, 4, 0, test.block_size,
        kBucketId);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    // test bad width.
    cmd.Init(
        GL_TEXTURE_2D, 0, test.format, 5, 4, 0,
        kBucketId);
    bucket->SetSize(test.block_size * 2);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

    // test small height.
    DoCompressedTexImage2D(
        GL_TEXTURE_2D, 0, test.format, 4, 2, 0, test.block_size,
        kBucketId);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    // test too bad height.
    cmd.Init(
        GL_TEXTURE_2D, 0, test.format, 4, 5, 0,
        kBucketId);
    bucket->SetSize(test.block_size * 2);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

    // test small for level 0.
    DoCompressedTexImage2D(
        GL_TEXTURE_2D, 0, test.format, 1, 1, 0, test.block_size,
        kBucketId);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    // test small for level 0.
    DoCompressedTexImage2D(
        GL_TEXTURE_2D, 0, test.format, 2, 2, 0, test.block_size,
        kBucketId);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    // test size too large.
    cmd.Init(
        GL_TEXTURE_2D, 0, test.format, 4, 4, 0,
        kBucketId);
    bucket->SetSize(test.block_size * 2);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_INVALID_VALUE, GetGLError());

    // test size too small.
    cmd.Init(
        GL_TEXTURE_2D, 0, test.format, 4, 4, 0,
        kBucketId);
    bucket->SetSize(test.block_size / 2);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_INVALID_VALUE, GetGLError());

    // test with 3 mips.
    DoCompressedTexImage2D(
        GL_TEXTURE_2D, 0, test.format, 4, 4, 0, test.block_size, kBucketId);
    DoCompressedTexImage2D(
        GL_TEXTURE_2D, 1, test.format, 2, 2, 0, test.block_size, kBucketId);
    DoCompressedTexImage2D(
        GL_TEXTURE_2D, 2, test.format, 1, 1, 0, test.block_size, kBucketId);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    // Test a 16x16
    DoCompressedTexImage2D(
        GL_TEXTURE_2D, 0, test.format, 16, 16, 0, test.block_size * 4 * 4,
        kBucketId);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    CompressedTexSubImage2DBucket sub_cmd;
    bucket->SetSize(test.block_size);
    // Test sub image bad xoffset
    sub_cmd.Init(
        GL_TEXTURE_2D, 0, 1, 0, 4, 4, test.format, kBucketId);
    EXPECT_EQ(error::kNoError, ExecuteCmd(sub_cmd));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

    // Test sub image bad yoffset
    sub_cmd.Init(
        GL_TEXTURE_2D, 0, 0, 2, 4, 4, test.format, kBucketId);
    EXPECT_EQ(error::kNoError, ExecuteCmd(sub_cmd));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

    // Test sub image bad width
    bucket->SetSize(test.block_size * 2);
    sub_cmd.Init(
        GL_TEXTURE_2D, 0, 0, 0, 5, 4, test.format, kBucketId);
    EXPECT_EQ(error::kNoError, ExecuteCmd(sub_cmd));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

    // Test sub image bad height
    sub_cmd.Init(
        GL_TEXTURE_2D, 0, 0, 0, 4, 5, test.format, kBucketId);
    EXPECT_EQ(error::kNoError, ExecuteCmd(sub_cmd));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

    // Test sub image bad size
    bucket->SetSize(test.block_size + 1);
    sub_cmd.Init(
        GL_TEXTURE_2D, 0, 0, 0, 4, 4, test.format, kBucketId);
    EXPECT_EQ(error::kNoError, ExecuteCmd(sub_cmd));
    EXPECT_EQ(GL_INVALID_VALUE, GetGLError());

    for (GLint yoffset = 0; yoffset <= 8; yoffset += 4) {
      for (GLint xoffset = 0; xoffset <= 8; xoffset += 4) {
        for (GLsizei height = 4; height <= 8; height +=4 ) {
          for (GLsizei width = 4; width <= 8; width += 4) {
            GLsizei size = test.block_size * (width / 4) * (height / 4);
            bucket->SetSize(size);
            EXPECT_CALL(*gl_, CompressedTexSubImage2D(
                GL_TEXTURE_2D, 0, xoffset, yoffset, width, height, test.format,
                size, _))
                .Times(1)
                .RetiresOnSaturation();
            sub_cmd.Init(
                GL_TEXTURE_2D, 0, xoffset, yoffset, width, height, test.format,
                kBucketId);
            EXPECT_EQ(error::kNoError, ExecuteCmd(sub_cmd));
            EXPECT_EQ(GL_NO_ERROR, GetGLError());
          }
        }
      }
    }
  }
}

TEST_F(GLES2DecoderManualInitTest, CompressedTexImage2DETC1) {
  InitDecoder(
      "GL_OES_compressed_ETC1_RGB8_texture",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource
  const uint32 kBucketId = 123;
  CommonDecoder::Bucket* bucket = decoder_->CreateBucket(kBucketId);
  ASSERT_TRUE(bucket != NULL);

  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);

  const GLenum kFormat = GL_ETC1_RGB8_OES;
  const size_t kBlockSize = 8;

  CompressedTexImage2DBucket cmd;
  // test small width.
  DoCompressedTexImage2D(GL_TEXTURE_2D, 0, kFormat, 4, 8, 0, 16, kBucketId);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // test small height.
  DoCompressedTexImage2D(GL_TEXTURE_2D, 0, kFormat, 8, 4, 0, 16, kBucketId);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // test size too large.
  cmd.Init(GL_TEXTURE_2D, 0, kFormat, 4, 4, 0, kBucketId);
  bucket->SetSize(kBlockSize * 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());

  // test size too small.
  cmd.Init(GL_TEXTURE_2D, 0, kFormat, 4, 4, 0, kBucketId);
  bucket->SetSize(kBlockSize / 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());

  // Test a 16x16
  DoCompressedTexImage2D(
      GL_TEXTURE_2D, 0, kFormat, 16, 16, 0, kBlockSize * 16, kBucketId);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Test CompressedTexSubImage not allowed
  CompressedTexSubImage2DBucket sub_cmd;
  bucket->SetSize(kBlockSize);
  sub_cmd.Init(GL_TEXTURE_2D, 0, 0, 0, 4, 4, kFormat, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(sub_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // Test TexSubImage not allowed for ETC1 compressed texture
  Texture* info = GetTexture(client_texture_id_);
  ASSERT_TRUE(info != NULL);
  GLenum type, internal_format;
  EXPECT_TRUE(info->GetLevelType(GL_TEXTURE_2D, 0, &type, &internal_format));
  EXPECT_EQ(kFormat, internal_format);
  TexSubImage2D texsub_cmd;
  texsub_cmd.Init(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE,
           kSharedMemoryId, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(texsub_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // Test CopyTexSubImage not allowed for ETC1 compressed texture
  CopyTexSubImage2D copy_cmd;
  copy_cmd.Init(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 4, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(copy_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderManualInitTest, GetCompressedTextureFormatsETC1) {
  InitDecoder(
      "GL_OES_compressed_ETC1_RGB8_texture",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();

  typedef GetIntegerv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  GetIntegerv cmd;
  result->size = 0;
  EXPECT_CALL(*gl_, GetIntegerv(_, _))
      .Times(0)
      .RetiresOnSaturation();
  cmd.Init(
      GL_NUM_COMPRESSED_TEXTURE_FORMATS,
      shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(1, result->GetNumResults());
  GLint num_formats = result->GetData()[0];
  EXPECT_EQ(1, num_formats);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  result->size = 0;
  cmd.Init(
      GL_COMPRESSED_TEXTURE_FORMATS,
      shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(num_formats, result->GetNumResults());

  EXPECT_TRUE(ValueInArray(
      GL_ETC1_RGB8_OES,
      result->GetData(), result->GetNumResults()));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetProgramInfoCHROMIUMValidArgs) {
  const uint32 kBucketId = 123;
  GetProgramInfoCHROMIUM cmd;
  cmd.Init(client_program_id_, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  EXPECT_GT(bucket->size(), 0u);
}

TEST_F(GLES2DecoderWithShaderTest, GetProgramInfoCHROMIUMInvalidArgs) {
  const uint32 kBucketId = 123;
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  EXPECT_TRUE(bucket == NULL);
  GetProgramInfoCHROMIUM cmd;
  cmd.Init(kInvalidClientId, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  bucket = decoder_->GetBucket(kBucketId);
  ASSERT_TRUE(bucket != NULL);
  EXPECT_EQ(sizeof(ProgramInfoHeader), bucket->size());
  ProgramInfoHeader* info = bucket->GetDataAs<ProgramInfoHeader*>(
      0, sizeof(ProgramInfoHeader));
  ASSERT_TRUE(info != 0);
  EXPECT_EQ(0u, info->link_status);
  EXPECT_EQ(0u, info->num_attribs);
  EXPECT_EQ(0u, info->num_uniforms);
}

TEST_F(GLES2DecoderManualInitTest, EGLImageExternalBindTexture) {
  InitDecoder(
      "GL_OES_EGL_image_external",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource
  EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_EXTERNAL_OES, kNewServiceId));
  EXPECT_CALL(*gl_, GenTextures(1, _))
      .WillOnce(SetArgumentPointee<1>(kNewServiceId));
  BindTexture cmd;
  cmd.Init(GL_TEXTURE_EXTERNAL_OES, kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  Texture* info = GetTexture(kNewClientId);
  EXPECT_TRUE(info != NULL);
  EXPECT_TRUE(info->target() == GL_TEXTURE_EXTERNAL_OES);
}

TEST_F(GLES2DecoderManualInitTest, EGLImageExternalGetBinding) {
  InitDecoder(
      "GL_OES_EGL_image_external",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource
  DoBindTexture(GL_TEXTURE_EXTERNAL_OES, client_texture_id_, kServiceTextureId);

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  typedef GetIntegerv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES,
                                result->GetData()))
      .Times(0);
  result->size = 0;
  GetIntegerv cmd;
  cmd.Init(GL_TEXTURE_BINDING_EXTERNAL_OES,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(
      GL_TEXTURE_BINDING_EXTERNAL_OES), result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(client_texture_id_, (uint32)result->GetData()[0]);
}

TEST_F(GLES2DecoderManualInitTest, EGLImageExternalTextureDefaults) {
  InitDecoder(
      "GL_OES_EGL_image_external",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource
  DoBindTexture(GL_TEXTURE_EXTERNAL_OES, client_texture_id_, kServiceTextureId);

  Texture* info = GetTexture(client_texture_id_);
  EXPECT_TRUE(info != NULL);
  EXPECT_TRUE(info->target() == GL_TEXTURE_EXTERNAL_OES);
  EXPECT_TRUE(info->min_filter() == GL_LINEAR);
  EXPECT_TRUE(info->wrap_s() == GL_CLAMP_TO_EDGE);
  EXPECT_TRUE(info->wrap_t() == GL_CLAMP_TO_EDGE);
}

TEST_F(GLES2DecoderManualInitTest, EGLImageExternalTextureParam) {
  InitDecoder(
      "GL_OES_EGL_image_external",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  DoBindTexture(GL_TEXTURE_EXTERNAL_OES, client_texture_id_, kServiceTextureId);

  EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_EXTERNAL_OES,
                                  GL_TEXTURE_MIN_FILTER,
                                  GL_NEAREST));
  EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_EXTERNAL_OES,
                                  GL_TEXTURE_MIN_FILTER,
                                  GL_LINEAR));
  EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_EXTERNAL_OES,
                                  GL_TEXTURE_WRAP_S,
                                  GL_CLAMP_TO_EDGE));
  EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_EXTERNAL_OES,
                                  GL_TEXTURE_WRAP_T,
                                  GL_CLAMP_TO_EDGE));
  TexParameteri cmd;
  cmd.Init(GL_TEXTURE_EXTERNAL_OES,
           GL_TEXTURE_MIN_FILTER,
           GL_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  cmd.Init(GL_TEXTURE_EXTERNAL_OES,
           GL_TEXTURE_MIN_FILTER,
           GL_LINEAR);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  cmd.Init(GL_TEXTURE_EXTERNAL_OES,
           GL_TEXTURE_WRAP_S,
           GL_CLAMP_TO_EDGE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  cmd.Init(GL_TEXTURE_EXTERNAL_OES,
           GL_TEXTURE_WRAP_T,
           GL_CLAMP_TO_EDGE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  Texture* info = GetTexture(client_texture_id_);
  EXPECT_TRUE(info != NULL);
  EXPECT_TRUE(info->target() == GL_TEXTURE_EXTERNAL_OES);
  EXPECT_TRUE(info->min_filter() == GL_LINEAR);
  EXPECT_TRUE(info->wrap_s() == GL_CLAMP_TO_EDGE);
  EXPECT_TRUE(info->wrap_t() == GL_CLAMP_TO_EDGE);
}

TEST_F(GLES2DecoderManualInitTest, EGLImageExternalTextureParamInvalid) {
  InitDecoder(
      "GL_OES_EGL_image_external",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  DoBindTexture(GL_TEXTURE_EXTERNAL_OES, client_texture_id_, kServiceTextureId);

  TexParameteri cmd;
  cmd.Init(GL_TEXTURE_EXTERNAL_OES,
           GL_TEXTURE_MIN_FILTER,
           GL_NEAREST_MIPMAP_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());

  cmd.Init(GL_TEXTURE_EXTERNAL_OES,
           GL_TEXTURE_WRAP_S,
           GL_REPEAT);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());

  cmd.Init(GL_TEXTURE_EXTERNAL_OES,
           GL_TEXTURE_WRAP_T,
           GL_REPEAT);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());

  Texture* info = GetTexture(client_texture_id_);
  EXPECT_TRUE(info != NULL);
  EXPECT_TRUE(info->target() == GL_TEXTURE_EXTERNAL_OES);
  EXPECT_TRUE(info->min_filter() == GL_LINEAR);
  EXPECT_TRUE(info->wrap_s() == GL_CLAMP_TO_EDGE);
  EXPECT_TRUE(info->wrap_t() == GL_CLAMP_TO_EDGE);
}

TEST_F(GLES2DecoderManualInitTest, EGLImageExternalTexImage2DError) {
  InitDecoder(
      "GL_OES_EGL_image_external",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  GLenum target = GL_TEXTURE_EXTERNAL_OES;
  GLint level = 0;
  GLenum internal_format = GL_RGBA;
  GLsizei width = 2;
  GLsizei height = 4;
  GLint border = 0;
  GLenum format = GL_RGBA;
  GLenum type = GL_UNSIGNED_BYTE;
  DoBindTexture(GL_TEXTURE_EXTERNAL_OES, client_texture_id_, kServiceTextureId);
  ASSERT_TRUE(GetTexture(client_texture_id_) != NULL);
  TexImage2D cmd;
  cmd.Init(target, level, internal_format, width, height, border, format,
           type, kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

  // TexImage2D is not allowed with GL_TEXTURE_EXTERNAL_OES targets.
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderManualInitTest, BindGeneratesResourceFalse) {
  InitDecoder(
      "",      // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      false);  // bind generates resource

  BindTexture cmd1;
  cmd1.Init(GL_TEXTURE_2D, kInvalidClientId);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd1));

  BindBuffer cmd2;
  cmd2.Init(GL_ARRAY_BUFFER, kInvalidClientId);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd2));

  BindFramebuffer cmd3;
  cmd3.Init(GL_FRAMEBUFFER, kInvalidClientId);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd3));

  BindRenderbuffer cmd4;
  cmd4.Init(GL_RENDERBUFFER, kInvalidClientId);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd4));
}

TEST_F(GLES2DecoderManualInitTest, CreateStreamTextureCHROMIUM) {
  const GLuint kObjectId = 123;
  InitDecoder(
      "GL_CHROMIUM_stream_texture",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  StrictMock<MockStreamTextureManager> manager;
  decoder_->SetStreamTextureManager(&manager);

  EXPECT_CALL(manager, CreateStreamTexture(kServiceTextureId,
                                           client_texture_id_))
      .WillOnce(Return(kObjectId))
      .RetiresOnSaturation();

  CreateStreamTextureCHROMIUM cmd;
  CreateStreamTextureCHROMIUM::Result* result =
      static_cast<CreateStreamTextureCHROMIUM::Result*>(shared_memory_address_);
  cmd.Init(client_texture_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(kObjectId, *result);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  Texture* info = GetTexture(client_texture_id_);
  EXPECT_TRUE(info != NULL);
  EXPECT_TRUE(info->IsStreamTexture());
}

TEST_F(GLES2DecoderManualInitTest, CreateStreamTextureCHROMIUMBadId) {
  InitDecoder(
      "GL_CHROMIUM_stream_texture",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  CreateStreamTextureCHROMIUM cmd;
  CreateStreamTextureCHROMIUM::Result* result =
      static_cast<CreateStreamTextureCHROMIUM::Result*>(shared_memory_address_);
  cmd.Init(kNewClientId, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(static_cast<GLuint>(GL_ZERO), *result);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderManualInitTest, CreateStreamTextureCHROMIUMAlreadyBound) {
  InitDecoder(
      "GL_CHROMIUM_stream_texture",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);

  CreateStreamTextureCHROMIUM cmd;
  CreateStreamTextureCHROMIUM::Result* result =
      static_cast<CreateStreamTextureCHROMIUM::Result*>(shared_memory_address_);
  cmd.Init(client_texture_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(static_cast<GLuint>(GL_ZERO), *result);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderManualInitTest, CreateStreamTextureCHROMIUMAlreadySet) {
  InitDecoder(
      "GL_CHROMIUM_stream_texture",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  Texture* info = GetTexture(client_texture_id_);
  info->SetStreamTexture(true);

  CreateStreamTextureCHROMIUM cmd;
  cmd.Init(client_texture_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderManualInitTest, BindStreamTextureCHROMIUM) {
  InitDecoder(
      "GL_CHROMIUM_stream_texture GL_OES_EGL_image_external",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  StrictMock<MockStreamTextureManager> manager;
  StrictMock<MockStreamTexture> texture;
  decoder_->SetStreamTextureManager(&manager);

  Texture* info = GetTexture(client_texture_id_);
  info->SetStreamTexture(true);

  EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_EXTERNAL_OES, kServiceTextureId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(manager, LookupStreamTexture(kServiceTextureId))
      .WillOnce(Return(&texture))
      .RetiresOnSaturation();
  EXPECT_CALL(texture, Update())
      .Times(1)
      .RetiresOnSaturation();

  BindTexture cmd;
  cmd.Init(GL_TEXTURE_EXTERNAL_OES, client_texture_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderManualInitTest, BindStreamTextureCHROMIUMInvalid) {
  InitDecoder(
      "GL_CHROMIUM_stream_texture",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  Texture* info = GetTexture(client_texture_id_);
  info->SetStreamTexture(true);

  BindTexture cmd;
  cmd.Init(GL_TEXTURE_2D, client_texture_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  BindTexture cmd2;
  cmd2.Init(GL_TEXTURE_CUBE_MAP, client_texture_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderManualInitTest, DestroyStreamTextureCHROMIUM) {
  InitDecoder(
      "GL_CHROMIUM_stream_texture",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  StrictMock<MockStreamTextureManager> manager;
  decoder_->SetStreamTextureManager(&manager);

  Texture* info = GetTexture(client_texture_id_);
  info->SetStreamTexture(true);

  EXPECT_CALL(manager, DestroyStreamTexture(kServiceTextureId))
      .Times(1)
      .RetiresOnSaturation();

  DestroyStreamTextureCHROMIUM cmd;
  cmd.Init(client_texture_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_FALSE(info->IsStreamTexture());
  EXPECT_EQ(0U, info->target());
}

TEST_F(GLES2DecoderManualInitTest, DestroyStreamTextureCHROMIUMInvalid) {
  InitDecoder(
      "GL_CHROMIUM_stream_texture",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  Texture* info = GetTexture(client_texture_id_);
  info->SetStreamTexture(false);

  DestroyStreamTextureCHROMIUM cmd;
  cmd.Init(client_texture_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderManualInitTest, DestroyStreamTextureCHROMIUMBadId) {
  InitDecoder(
      "GL_CHROMIUM_stream_texture",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  DestroyStreamTextureCHROMIUM cmd;
  cmd.Init(GL_ZERO);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderManualInitTest, StreamTextureCHROMIUMNullMgr) {
  InitDecoder(
      "GL_CHROMIUM_stream_texture",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  CreateStreamTextureCHROMIUM cmd;
  cmd.Init(client_texture_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
  GetGLError(); // ignore internal error

  Texture* info = GetTexture(client_texture_id_);
  info->SetStreamTexture(true);

  DestroyStreamTextureCHROMIUM cmd2;
  cmd2.Init(client_texture_id_);
  EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd2));
  GetGLError(); // ignore internal error
}

TEST_F(GLES2DecoderManualInitTest, ReCreateStreamTextureCHROMIUM) {
  const GLuint kObjectId = 123;
  InitDecoder(
      "GL_CHROMIUM_stream_texture GL_OES_EGL_image_external",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  StrictMock<MockStreamTextureManager> manager;
  StrictMock<MockStreamTexture> texture;
  decoder_->SetStreamTextureManager(&manager);

  EXPECT_CALL(manager, LookupStreamTexture(kServiceTextureId))
      .WillOnce(Return(&texture))
      .RetiresOnSaturation();
  EXPECT_CALL(texture, Update())
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(manager, DestroyStreamTexture(kServiceTextureId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(manager, CreateStreamTexture(kServiceTextureId,
                                           client_texture_id_))
      .WillOnce(Return(kObjectId))
      .RetiresOnSaturation();

  Texture* info = GetTexture(client_texture_id_);
  info->SetStreamTexture(true);

  DoBindTexture(GL_TEXTURE_EXTERNAL_OES, client_texture_id_, kServiceTextureId);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  DestroyStreamTextureCHROMIUM cmd;
  cmd.Init(client_texture_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_FALSE(info->IsStreamTexture());

  CreateStreamTextureCHROMIUM cmd2;
  cmd2.Init(client_texture_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(info->IsStreamTexture());
}

TEST_F(GLES2DecoderManualInitTest, ARBTextureRectangleBindTexture) {
  InitDecoder(
      "GL_ARB_texture_rectangle",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource
  EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_RECTANGLE_ARB, kNewServiceId));
  EXPECT_CALL(*gl_, GenTextures(1, _))
     .WillOnce(SetArgumentPointee<1>(kNewServiceId));
  BindTexture cmd;
  cmd.Init(GL_TEXTURE_RECTANGLE_ARB, kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  Texture* info = GetTexture(kNewClientId);
  EXPECT_TRUE(info != NULL);
  EXPECT_TRUE(info->target() == GL_TEXTURE_RECTANGLE_ARB);
}

TEST_F(GLES2DecoderManualInitTest, ARBTextureRectangleGetBinding) {
  InitDecoder(
      "GL_ARB_texture_rectangle",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource
  DoBindTexture(
      GL_TEXTURE_RECTANGLE_ARB, client_texture_id_, kServiceTextureId);

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  typedef GetIntegerv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_TEXTURE_BINDING_RECTANGLE_ARB,
                                result->GetData()))
      .Times(0);
  result->size = 0;
  GetIntegerv cmd;
  cmd.Init(GL_TEXTURE_BINDING_RECTANGLE_ARB,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(
      GL_TEXTURE_BINDING_RECTANGLE_ARB), result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(client_texture_id_, (uint32)result->GetData()[0]);
}

TEST_F(GLES2DecoderManualInitTest, ARBTextureRectangleTextureDefaults) {
  InitDecoder(
      "GL_ARB_texture_rectangle",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource
  DoBindTexture(
      GL_TEXTURE_RECTANGLE_ARB, client_texture_id_, kServiceTextureId);

  Texture* info = GetTexture(client_texture_id_);
  EXPECT_TRUE(info != NULL);
  EXPECT_TRUE(info->target() == GL_TEXTURE_RECTANGLE_ARB);
  EXPECT_TRUE(info->min_filter() == GL_LINEAR);
  EXPECT_TRUE(info->wrap_s() == GL_CLAMP_TO_EDGE);
  EXPECT_TRUE(info->wrap_t() == GL_CLAMP_TO_EDGE);
}

TEST_F(GLES2DecoderManualInitTest, ARBTextureRectangleTextureParam) {
  InitDecoder(
      "GL_ARB_texture_rectangle",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  DoBindTexture(
      GL_TEXTURE_RECTANGLE_ARB, client_texture_id_, kServiceTextureId);

  EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_RECTANGLE_ARB,
                                  GL_TEXTURE_MIN_FILTER,
                                  GL_NEAREST));
  EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_RECTANGLE_ARB,
                                  GL_TEXTURE_MIN_FILTER,
                                  GL_LINEAR));
  EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_RECTANGLE_ARB,
                                  GL_TEXTURE_WRAP_S,
                                  GL_CLAMP_TO_EDGE));
  EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_RECTANGLE_ARB,
                                  GL_TEXTURE_WRAP_T,
                                  GL_CLAMP_TO_EDGE));
  TexParameteri cmd;
  cmd.Init(GL_TEXTURE_RECTANGLE_ARB,
           GL_TEXTURE_MIN_FILTER,
           GL_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  cmd.Init(GL_TEXTURE_RECTANGLE_ARB,
           GL_TEXTURE_MIN_FILTER,
           GL_LINEAR);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  cmd.Init(GL_TEXTURE_RECTANGLE_ARB,
           GL_TEXTURE_WRAP_S,
           GL_CLAMP_TO_EDGE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  cmd.Init(GL_TEXTURE_RECTANGLE_ARB,
           GL_TEXTURE_WRAP_T,
           GL_CLAMP_TO_EDGE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  Texture* info = GetTexture(client_texture_id_);
  EXPECT_TRUE(info != NULL);
  EXPECT_TRUE(info->target() == GL_TEXTURE_RECTANGLE_ARB);
  EXPECT_TRUE(info->min_filter() == GL_LINEAR);
  EXPECT_TRUE(info->wrap_s() == GL_CLAMP_TO_EDGE);
  EXPECT_TRUE(info->wrap_t() == GL_CLAMP_TO_EDGE);
}

TEST_F(GLES2DecoderManualInitTest, ARBTextureRectangleTextureParamInvalid) {
  InitDecoder(
      "GL_ARB_texture_rectangle",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  DoBindTexture(
      GL_TEXTURE_RECTANGLE_ARB, client_texture_id_, kServiceTextureId);

  TexParameteri cmd;
  cmd.Init(GL_TEXTURE_RECTANGLE_ARB,
           GL_TEXTURE_MIN_FILTER,
           GL_NEAREST_MIPMAP_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());

  cmd.Init(GL_TEXTURE_RECTANGLE_ARB,
           GL_TEXTURE_WRAP_S,
           GL_REPEAT);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());

  cmd.Init(GL_TEXTURE_RECTANGLE_ARB,
           GL_TEXTURE_WRAP_T,
           GL_REPEAT);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());

  Texture* info = GetTexture(client_texture_id_);
  EXPECT_TRUE(info != NULL);
  EXPECT_TRUE(info->target() == GL_TEXTURE_RECTANGLE_ARB);
  EXPECT_TRUE(info->min_filter() == GL_LINEAR);
  EXPECT_TRUE(info->wrap_s() == GL_CLAMP_TO_EDGE);
  EXPECT_TRUE(info->wrap_t() == GL_CLAMP_TO_EDGE);
}

TEST_F(GLES2DecoderManualInitTest, ARBTextureRectangleTexImage2DError) {
  InitDecoder(
      "GL_ARB_texture_rectangle",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  GLenum target = GL_TEXTURE_RECTANGLE_ARB;
  GLint level = 0;
  GLenum internal_format = GL_RGBA;
  GLsizei width = 2;
  GLsizei height = 4;
  GLint border = 0;
  GLenum format = GL_RGBA;
  GLenum type = GL_UNSIGNED_BYTE;
  DoBindTexture(
      GL_TEXTURE_RECTANGLE_ARB, client_texture_id_, kServiceTextureId);
  ASSERT_TRUE(GetTexture(client_texture_id_) != NULL);
  TexImage2D cmd;
  cmd.Init(target, level, internal_format, width, height, border, format,
           type, kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

  // TexImage2D is not allowed with GL_TEXTURE_RECTANGLE_ARB targets.
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest, EnableFeatureCHROMIUMBadBucket) {
  const uint32 kBadBucketId = 123;
  EnableFeatureCHROMIUM cmd;
  cmd.Init(kBadBucketId, shared_memory_id_, shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, RequestExtensionCHROMIUMBadBucket) {
  const uint32 kBadBucketId = 123;
  RequestExtensionCHROMIUM cmd;
  cmd.Init(kBadBucketId);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, TexSubImage2DClearsAfterTexImage2DNULL) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               0, 0);
  SetupClearTextureExpections(
      kServiceTextureId, kServiceTextureId, GL_TEXTURE_2D, GL_TEXTURE_2D,
      0, GL_RGBA, GL_UNSIGNED_BYTE, 2, 2);
  EXPECT_CALL(*gl_, TexSubImage2D(
      GL_TEXTURE_2D, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
      shared_memory_address_))
      .Times(1)
      .RetiresOnSaturation();
  TexSubImage2D cmd;
  cmd.Init(
      GL_TEXTURE_2D, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
      kSharedMemoryId, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  // Test if we call it again it does not clear.
  EXPECT_CALL(*gl_, TexSubImage2D(
      GL_TEXTURE_2D, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
      shared_memory_address_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, TexSubImage2DDoesNotClearAfterTexImage2DNULLThenData) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               0, 0);
  DoTexImage2DSameSize(
      GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
      kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, TexSubImage2D(
      GL_TEXTURE_2D, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
      shared_memory_address_))
      .Times(1)
      .RetiresOnSaturation();
  TexSubImage2D cmd;
  cmd.Init(
      GL_TEXTURE_2D, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
      kSharedMemoryId, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  // Test if we call it again it does not clear.
  EXPECT_CALL(*gl_, TexSubImage2D(
      GL_TEXTURE_2D, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
      shared_memory_address_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderANGLETest,
       TexSubImage2DDoesNotClearAfterTexImage2DNULLThenData) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               0, 0);
  DoTexImage2DSameSize(
      GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
      kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, TexSubImage2D(
      GL_TEXTURE_2D, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
      shared_memory_address_))
      .Times(1)
      .RetiresOnSaturation();
  TexSubImage2D cmd;
  cmd.Init(
      GL_TEXTURE_2D, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
      kSharedMemoryId, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  // Test if we call it again it does not clear.
  EXPECT_CALL(*gl_, TexSubImage2D(
      GL_TEXTURE_2D, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
      shared_memory_address_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, TexSubImage2DClearsAfterTexImage2DWithDataThenNULL) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  // Put in data (so it should be marked as cleared)
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               kSharedMemoryId, kSharedMemoryOffset);
  // Put in no data.
  TexImage2D tex_cmd;
  tex_cmd.Init(
      GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
  // It won't actually call TexImage2D, just mark it as uncleared.
  EXPECT_EQ(error::kNoError, ExecuteCmd(tex_cmd));
  // Next call to TexSubImage2d should clear.
  SetupClearTextureExpections(
      kServiceTextureId, kServiceTextureId, GL_TEXTURE_2D, GL_TEXTURE_2D,
      0, GL_RGBA, GL_UNSIGNED_BYTE, 2, 2);
  EXPECT_CALL(*gl_, TexSubImage2D(
      GL_TEXTURE_2D, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
      shared_memory_address_))
      .Times(1)
      .RetiresOnSaturation();
  TexSubImage2D cmd;
  cmd.Init(
      GL_TEXTURE_2D, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
      kSharedMemoryId, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, DrawArraysClearsAfterTexImage2DNULL) {
  SetupAllNeededVertexBuffers();
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  // Create an uncleared texture with 2 levels.
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               0, 0);
  DoTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               0, 0);
  // Expect 2 levels will be cleared.
  SetupClearTextureExpections(
      kServiceTextureId, kServiceTextureId, GL_TEXTURE_2D, GL_TEXTURE_2D,
      0, GL_RGBA, GL_UNSIGNED_BYTE, 2, 2);
  SetupClearTextureExpections(
      kServiceTextureId, kServiceTextureId, GL_TEXTURE_2D, GL_TEXTURE_2D,
      1, GL_RGBA, GL_UNSIGNED_BYTE, 1, 1);
  SetupExpectationsForApplyingDefaultDirtyState();
  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // But not again
  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsClearsAfterTexImage2DNULL) {
  SetupAllNeededVertexBuffers();
  SetupIndexBuffer();
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  // Create an uncleared texture with 2 levels.
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               0, 0);
  DoTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               0, 0);
  // Expect 2 levels will be cleared.
  SetupClearTextureExpections(
      kServiceTextureId, kServiceTextureId, GL_TEXTURE_2D, GL_TEXTURE_2D,
      0, GL_RGBA, GL_UNSIGNED_BYTE, 2, 2);
  SetupClearTextureExpections(
      kServiceTextureId, kServiceTextureId, GL_TEXTURE_2D, GL_TEXTURE_2D,
      1, GL_RGBA, GL_UNSIGNED_BYTE, 1, 1);
  SetupExpectationsForApplyingDefaultDirtyState();

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

  // But not again
  EXPECT_CALL(*gl_, DrawElements(GL_TRIANGLES, kValidIndexRangeCount,
                                 GL_UNSIGNED_SHORT,
                                 BufferOffset(kValidIndexRangeStart * 2)))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawClearsAfterTexImage2DNULLInFBO) {
  const GLuint kFBOClientTextureId = 4100;
  const GLuint kFBOServiceTextureId = 4101;

  SetupAllNeededVertexBuffers();
  // Register a texture id.
  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgumentPointee<1>(kFBOServiceTextureId))
      .RetiresOnSaturation();
  GenHelper<GenTexturesImmediate>(kFBOClientTextureId);

  // Setup "render to" texture.
  DoBindTexture(GL_TEXTURE_2D, kFBOClientTextureId, kFBOServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoFramebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
      kFBOClientTextureId, kFBOServiceTextureId, 0, GL_NO_ERROR);

  // Setup "render from" texture.
  SetupTexture();

  SetupExpectationsForFramebufferClearing(
      GL_FRAMEBUFFER,         // target
      GL_COLOR_BUFFER_BIT,    // clear bits
      0, 0, 0, 0,             // color
      0,                      // stencil
      1.0f,                   // depth
      false);                 // scissor test

  SetupExpectationsForApplyingDirtyState(
      false,   // Framebuffer is RGB
      false,   // Framebuffer has depth
      false,   // Framebuffer has stencil
      0x1111,  // color bits
      false,   // depth mask
      false,   // depth enabled
      0,       // front stencil mask
      0,       // back stencil mask
      false,   // stencil enabled
      false,   // cull_face_enabled
      false,   // scissor_test_enabled
      false);  // blend_enabled

  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // But not again.
  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawWitFBOThatCantClearDoesNotDraw) {
  const GLuint kFBOClientTextureId = 4100;
  const GLuint kFBOServiceTextureId = 4101;

  // Register a texture id.
  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgumentPointee<1>(kFBOServiceTextureId))
      .RetiresOnSaturation();
  GenHelper<GenTexturesImmediate>(kFBOClientTextureId);

  // Setup "render to" texture.
  DoBindTexture(GL_TEXTURE_2D, kFBOClientTextureId, kFBOServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoFramebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
      kFBOClientTextureId, kFBOServiceTextureId, 0, GL_NO_ERROR);

  // Setup "render from" texture.
  SetupTexture();

  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_FRAMEBUFFER))
      .WillOnce(Return(GL_FRAMEBUFFER_UNSUPPORTED))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, DrawArrays(_, _, _))
      .Times(0)
      .RetiresOnSaturation();
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_FRAMEBUFFER_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderTest, CopyTexImage2DMarksTextureAsCleared) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);

  TextureManager* manager = group().texture_manager();
  Texture* info =
      manager->GetTexture(client_texture_id_);

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, CopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 1, 1, 0))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  CopyTexImage2D cmd;
  cmd.Init(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 1, 1, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

  EXPECT_TRUE(info->SafeToRenderFrom());
}

TEST_F(GLES2DecoderTest, CopyTexSubImage2DClearsUnclearedTexture) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);

  SetupClearTextureExpections(
      kServiceTextureId, kServiceTextureId, GL_TEXTURE_2D, GL_TEXTURE_2D,
      0, GL_RGBA, GL_UNSIGNED_BYTE, 2, 2);
  EXPECT_CALL(*gl_, CopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 1, 1))
      .Times(1)
      .RetiresOnSaturation();
  CopyTexSubImage2D cmd;
  cmd.Init(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 1, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderManualInitTest, CompressedImage2DMarksTextureAsCleared) {
  InitDecoder(
      "GL_EXT_texture_compression_s3tc",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, CompressedTexImage2D(
      GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 0, 8, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  CompressedTexImage2D cmd;
  cmd.Init(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 0,
           8, kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  TextureManager* manager = group().texture_manager();
  Texture* info =
      manager->GetTexture(client_texture_id_);
  EXPECT_TRUE(info->SafeToRenderFrom());
}

TEST_F(GLES2DecoderWithShaderTest, UnClearedAttachmentsGetClearedOnClear) {
  const GLuint kFBOClientTextureId = 4100;
  const GLuint kFBOServiceTextureId = 4101;

  // Register a texture id.
  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgumentPointee<1>(kFBOServiceTextureId))
      .RetiresOnSaturation();
  GenHelper<GenTexturesImmediate>(kFBOClientTextureId);

  // Setup "render to" texture.
  DoBindTexture(GL_TEXTURE_2D, kFBOClientTextureId, kFBOServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoFramebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
      kFBOClientTextureId, kFBOServiceTextureId, 0, GL_NO_ERROR);

  // Setup "render from" texture.
  SetupTexture();

  SetupExpectationsForFramebufferClearing(
      GL_FRAMEBUFFER,         // target
      GL_COLOR_BUFFER_BIT,    // clear bits
      0, 0, 0, 0,             // color
      0,                      // stencil
      1.0f,                   // depth
      false);                 // scissor test
  SetupExpectationsForApplyingDirtyState(
      false,   // Framebuffer is RGB
      false,   // Framebuffer has depth
      false,   // Framebuffer has stencil
      0x1111,  // color bits
      false,   // depth mask
      false,   // depth enabled
      0,       // front stencil mask
      0,       // back stencil mask
      false,   // stencil enabled
      false,   // cull_face_enabled
      false,   // scissor_test_enabled
      false);  // blend_enabled

  EXPECT_CALL(*gl_, Clear(GL_COLOR_BUFFER_BIT))
      .Times(1)
      .RetiresOnSaturation();

  Clear cmd;
  cmd.Init(GL_COLOR_BUFFER_BIT);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, UnClearedAttachmentsGetClearedOnReadPixels) {
  const GLuint kFBOClientTextureId = 4100;
  const GLuint kFBOServiceTextureId = 4101;

  // Register a texture id.
  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgumentPointee<1>(kFBOServiceTextureId))
      .RetiresOnSaturation();
  GenHelper<GenTexturesImmediate>(kFBOClientTextureId);

  // Setup "render to" texture.
  DoBindTexture(GL_TEXTURE_2D, kFBOClientTextureId, kFBOServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoFramebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
      kFBOClientTextureId, kFBOServiceTextureId, 0, GL_NO_ERROR);

  // Setup "render from" texture.
  SetupTexture();

  SetupExpectationsForFramebufferClearing(
      GL_FRAMEBUFFER,         // target
      GL_COLOR_BUFFER_BIT,    // clear bits
      0, 0, 0, 0,             // color
      0,                      // stencil
      1.0f,                   // depth
      false);                 // scissor test

  EXPECT_CALL(*gl_, GetError())
     .WillOnce(Return(GL_NO_ERROR))
     .WillOnce(Return(GL_NO_ERROR))
     .RetiresOnSaturation();
  EXPECT_CALL(*gl_, ReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, _))
      .Times(1)
      .RetiresOnSaturation();
  typedef ReadPixels::Result Result;
  Result* result = GetSharedMemoryAs<Result*>();
  uint32 result_shm_id = kSharedMemoryId;
  uint32 result_shm_offset = kSharedMemoryOffset;
  uint32 pixels_shm_id = kSharedMemoryId;
  uint32 pixels_shm_offset = kSharedMemoryOffset + sizeof(*result);
  ReadPixels cmd;
  cmd.Init(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
         pixels_shm_id, pixels_shm_offset,
         result_shm_id, result_shm_offset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderManualInitTest,
       UnClearedAttachmentsGetClearedOnReadPixelsAndDrawBufferGetsRestored) {
  InitDecoder(
      "GL_EXT_framebuffer_multisample",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource
  const GLuint kFBOClientTextureId = 4100;
  const GLuint kFBOServiceTextureId = 4101;

  // Register a texture id.
  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgumentPointee<1>(kFBOServiceTextureId))
      .RetiresOnSaturation();
  GenHelper<GenTexturesImmediate>(kFBOClientTextureId);

  // Setup "render from" texture.
  DoBindTexture(GL_TEXTURE_2D, kFBOClientTextureId, kFBOServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
  DoBindFramebuffer(
      GL_READ_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoFramebufferTexture2D(
      GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
      kFBOClientTextureId, kFBOServiceTextureId, 0, GL_NO_ERROR);

  SetupExpectationsForFramebufferClearingMulti(
      kServiceFramebufferId,  // read framebuffer service id
      0,                      // backbuffer service id
      GL_READ_FRAMEBUFFER,    // target
      GL_COLOR_BUFFER_BIT,    // clear bits
      0, 0, 0, 0,             // color
      0,                      // stencil
      1.0f,                   // depth
      false);                 // scissor test

  EXPECT_CALL(*gl_, GetError())
     .WillOnce(Return(GL_NO_ERROR))
     .WillOnce(Return(GL_NO_ERROR))
     .RetiresOnSaturation();
  EXPECT_CALL(*gl_, ReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, _))
      .Times(1)
      .RetiresOnSaturation();
  typedef ReadPixels::Result Result;
  uint32 result_shm_id = kSharedMemoryId;
  uint32 result_shm_offset = kSharedMemoryOffset;
  uint32 pixels_shm_id = kSharedMemoryId;
  uint32 pixels_shm_offset = kSharedMemoryOffset + sizeof(Result);
  ReadPixels cmd;
  cmd.Init(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
         pixels_shm_id, pixels_shm_offset,
         result_shm_id, result_shm_offset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawClearsAfterRenderbufferStorageInFBO) {
  SetupTexture();
  DoBindRenderbuffer(GL_RENDERBUFFER, client_renderbuffer_id_,
                    kServiceRenderbufferId);
  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
  DoRenderbufferStorage(
      GL_RENDERBUFFER, GL_RGBA4, GL_RGBA, 100, 50, GL_NO_ERROR);
  DoFramebufferRenderbuffer(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
      client_renderbuffer_id_, kServiceRenderbufferId, GL_NO_ERROR);

  SetupExpectationsForFramebufferClearing(
      GL_FRAMEBUFFER,         // target
      GL_COLOR_BUFFER_BIT,    // clear bits
      0, 0, 0, 0,             // color
      0,                      // stencil
      1.0f,                   // depth
      false);                 // scissor test

  AddExpectationsForSimulatedAttrib0(kNumVertices, 0);
  SetupExpectationsForApplyingDirtyState(
      false,   // Framebuffer is RGB
      false,   // Framebuffer has depth
      false,   // Framebuffer has stencil
      0x1111,  // color bits
      false,   // depth mask
      false,   // depth enabled
      0,       // front stencil mask
      0,       // back stencil mask
      false,   // stencil enabled
      false,   // cull_face_enabled
      false,   // scissor_test_enabled
      false);  // blend_enabled

  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest, DrawArraysClearsAfterTexImage2DNULLCubemap) {
  static const GLenum faces[] = {
    GL_TEXTURE_CUBE_MAP_POSITIVE_X,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
    GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
    GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
  };
  SetupCubemapProgram();
  DoBindTexture(GL_TEXTURE_CUBE_MAP, client_texture_id_, kServiceTextureId);
  // Fill out all the faces for 2 levels, leave 2 uncleared.
  for (int ii = 0; ii < 6; ++ii) {
    GLenum face = faces[ii];
    int32 shm_id =
        (face == GL_TEXTURE_CUBE_MAP_NEGATIVE_Y) ? 0 : kSharedMemoryId;
    uint32 shm_offset =
        (face == GL_TEXTURE_CUBE_MAP_NEGATIVE_Y) ? 0 : kSharedMemoryOffset;
    DoTexImage2D(face, 0, GL_RGBA, 2, 2, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, shm_id, shm_offset);
    DoTexImage2D(face, 1, GL_RGBA, 1, 1, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, shm_id, shm_offset);
  }
  // Expect 2 levels will be cleared.
  SetupClearTextureExpections(
      kServiceTextureId, kServiceTextureId, GL_TEXTURE_CUBE_MAP,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGBA, GL_UNSIGNED_BYTE, 2, 2);
  SetupClearTextureExpections(
      kServiceTextureId, kServiceTextureId, GL_TEXTURE_CUBE_MAP,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 1, GL_RGBA, GL_UNSIGNED_BYTE, 1, 1);
  AddExpectationsForSimulatedAttrib0(kNumVertices, 0);
  SetupExpectationsForApplyingDefaultDirtyState();
  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, TextureUsageAngleExtNotEnabledByDefault) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);

  TexParameteri cmd;
  cmd.Init(GL_TEXTURE_2D,
           GL_TEXTURE_USAGE_ANGLE,
           GL_FRAMEBUFFER_ATTACHMENT_ANGLE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest,
       DrawClearsAfterRenderbuffersWithMultipleAttachments) {
  const GLuint kFBOClientTextureId = 4100;
  const GLuint kFBOServiceTextureId = 4101;

  // Register a texture id.
  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgumentPointee<1>(kFBOServiceTextureId))
      .RetiresOnSaturation();
  GenHelper<GenTexturesImmediate>(kFBOClientTextureId);

  // Setup "render to" texture.
  DoBindTexture(GL_TEXTURE_2D, kFBOClientTextureId, kFBOServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoFramebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
      kFBOClientTextureId, kFBOServiceTextureId, 0, GL_NO_ERROR);

  DoBindRenderbuffer(GL_RENDERBUFFER, client_renderbuffer_id_,
                    kServiceRenderbufferId);
  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
  DoRenderbufferStorage(
      GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT,
      1, 1, GL_NO_ERROR);
  DoFramebufferRenderbuffer(
      GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
      client_renderbuffer_id_, kServiceRenderbufferId, GL_NO_ERROR);

  SetupTexture();
  SetupExpectationsForFramebufferClearing(
      GL_FRAMEBUFFER,         // target
      GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,    // clear bits
      0, 0, 0, 0,             // color
      0,                      // stencil
      1.0f,                   // depth
      false);                 // scissor test

  AddExpectationsForSimulatedAttrib0(kNumVertices, 0);
  SetupExpectationsForApplyingDirtyState(
      false,   // Framebuffer is RGB
      true,    // Framebuffer has depth
      false,   // Framebuffer has stencil
      0x1111,  // color bits
      true,    // depth mask
      false,   // depth enabled
      0,       // front stencil mask
      0,       // back stencil mask
      false,   // stencil enabled
      false,   // cull_face_enabled
      false,   // scissor_test_enabled
      false);  // blend_enabled

  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, CopyTexImageWithInCompleteFBOFails) {
  GLenum target = GL_TEXTURE_2D;
  GLint level = 0;
  GLenum internal_format = GL_RGBA;
  GLsizei width = 2;
  GLsizei height = 4;
  GLint border = 0;
  SetupTexture();
  DoBindRenderbuffer(GL_RENDERBUFFER, client_renderbuffer_id_,
                    kServiceRenderbufferId);
  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
  DoRenderbufferStorage(
      GL_RENDERBUFFER, GL_RGBA4, GL_RGBA, 0, 0, GL_NO_ERROR);
  DoFramebufferRenderbuffer(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
      client_renderbuffer_id_, kServiceRenderbufferId, GL_NO_ERROR);

  EXPECT_CALL(*gl_, CopyTexImage2D(_, _, _, _, _, _, _, _))
      .Times(0)
      .RetiresOnSaturation();
  CopyTexImage2D cmd;
  cmd.Init(target, level, internal_format, 0, 0, width, height, border);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_FRAMEBUFFER_OPERATION, GetGLError());
}

void GLES2DecoderWithShaderTest::CheckRenderbufferChangesMarkFBOAsNotComplete(
    bool bound_fbo) {
  FramebufferManager* framebuffer_manager = group().framebuffer_manager();
  SetupTexture();
  DoBindRenderbuffer(GL_RENDERBUFFER, client_renderbuffer_id_,
                    kServiceRenderbufferId);
  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
  DoRenderbufferStorage(
      GL_RENDERBUFFER, GL_RGBA4, GL_RGBA, 1, 1, GL_NO_ERROR);
  DoFramebufferRenderbuffer(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
      client_renderbuffer_id_, kServiceRenderbufferId, GL_NO_ERROR);


  if (!bound_fbo) {
    DoBindFramebuffer(GL_FRAMEBUFFER, 0, 0);
  }

  Framebuffer* framebuffer =
      framebuffer_manager->GetFramebuffer(client_framebuffer_id_);
  ASSERT_TRUE(framebuffer != NULL);
  framebuffer_manager->MarkAsComplete(framebuffer);
  EXPECT_TRUE(framebuffer_manager->IsComplete(framebuffer));

  // Test that renderbufferStorage marks fbo as not complete.
  DoRenderbufferStorage(
      GL_RENDERBUFFER, GL_RGBA4, GL_RGBA, 1, 1, GL_NO_ERROR);
  EXPECT_FALSE(framebuffer_manager->IsComplete(framebuffer));
  framebuffer_manager->MarkAsComplete(framebuffer);
  EXPECT_TRUE(framebuffer_manager->IsComplete(framebuffer));

  // Test deleting renderbuffer marks fbo as not complete.
  DoDeleteRenderbuffer(client_renderbuffer_id_, kServiceRenderbufferId);
  if (bound_fbo) {
    EXPECT_FALSE(framebuffer_manager->IsComplete(framebuffer));
  } else {
    EXPECT_TRUE(framebuffer_manager->IsComplete(framebuffer));
  }
}

TEST_F(GLES2DecoderWithShaderTest,
       RenderbufferChangesMarkFBOAsNotCompleteBoundFBO) {
  CheckRenderbufferChangesMarkFBOAsNotComplete(true);
}

TEST_F(GLES2DecoderWithShaderTest,
       RenderbufferChangesMarkFBOAsNotCompleteUnboundFBO) {
  CheckRenderbufferChangesMarkFBOAsNotComplete(false);
}

void GLES2DecoderWithShaderTest::CheckTextureChangesMarkFBOAsNotComplete(
    bool bound_fbo) {
  FramebufferManager* framebuffer_manager = group().framebuffer_manager();
  const GLuint kFBOClientTextureId = 4100;
  const GLuint kFBOServiceTextureId = 4101;

  // Register a texture id.
  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgumentPointee<1>(kFBOServiceTextureId))
      .RetiresOnSaturation();
  GenHelper<GenTexturesImmediate>(kFBOClientTextureId);

  SetupTexture();

  // Setup "render to" texture.
  DoBindTexture(GL_TEXTURE_2D, kFBOClientTextureId, kFBOServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoFramebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
      kFBOClientTextureId, kFBOServiceTextureId, 0, GL_NO_ERROR);

  DoBindRenderbuffer(GL_RENDERBUFFER, client_renderbuffer_id_,
                    kServiceRenderbufferId);
  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
  DoRenderbufferStorage(
      GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT,
      1, 1, GL_NO_ERROR);
  DoFramebufferRenderbuffer(
      GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
      client_renderbuffer_id_, kServiceRenderbufferId, GL_NO_ERROR);

  if (!bound_fbo) {
    DoBindFramebuffer(GL_FRAMEBUFFER, 0, 0);
  }

  Framebuffer* framebuffer =
      framebuffer_manager->GetFramebuffer(client_framebuffer_id_);
  ASSERT_TRUE(framebuffer != NULL);
  framebuffer_manager->MarkAsComplete(framebuffer);
  EXPECT_TRUE(framebuffer_manager->IsComplete(framebuffer));

  // Test TexImage2D marks fbo as not complete.
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0, 0);
  EXPECT_FALSE(framebuffer_manager->IsComplete(framebuffer));
  framebuffer_manager->MarkAsComplete(framebuffer);
  EXPECT_TRUE(framebuffer_manager->IsComplete(framebuffer));

  // Test CopyImage2D marks fbo as not complete.
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, CopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 1, 1, 0))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  CopyTexImage2D cmd;
  cmd.Init(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 1, 1, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_FALSE(framebuffer_manager->IsComplete(framebuffer));

  // Test deleting texture marks fbo as not complete.
  framebuffer_manager->MarkAsComplete(framebuffer);
  EXPECT_TRUE(framebuffer_manager->IsComplete(framebuffer));
  DoDeleteTexture(kFBOClientTextureId, kFBOServiceTextureId);

  if (bound_fbo) {
    EXPECT_FALSE(framebuffer_manager->IsComplete(framebuffer));
  } else {
    EXPECT_TRUE(framebuffer_manager->IsComplete(framebuffer));
  }
}

TEST_F(GLES2DecoderWithShaderTest, TextureChangesMarkFBOAsNotCompleteBoundFBO) {
  CheckTextureChangesMarkFBOAsNotComplete(true);
}

TEST_F(GLES2DecoderWithShaderTest,
       TextureChangesMarkFBOAsNotCompleteUnboundFBO) {
  CheckTextureChangesMarkFBOAsNotComplete(false);
}

TEST_F(GLES2DecoderWithShaderTest,
       DrawingWithFBOTwiceChecksForFBOCompleteOnce) {
  const GLuint kFBOClientTextureId = 4100;
  const GLuint kFBOServiceTextureId = 4101;

  SetupAllNeededVertexBuffers();

  // Register a texture id.
  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgumentPointee<1>(kFBOServiceTextureId))
      .RetiresOnSaturation();
  GenHelper<GenTexturesImmediate>(kFBOClientTextureId);

  // Setup "render to" texture that is cleared.
  DoBindTexture(GL_TEXTURE_2D, kFBOClientTextureId, kFBOServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
      kSharedMemoryId, kSharedMemoryOffset);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoFramebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
      kFBOClientTextureId, kFBOServiceTextureId, 0, GL_NO_ERROR);

  // Setup "render from" texture.
  SetupTexture();

  // Make sure we check for framebuffer complete.
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_FRAMEBUFFER))
      .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
      .RetiresOnSaturation();

  SetupExpectationsForApplyingDirtyState(
      false,   // Framebuffer is RGB
      false,   // Framebuffer has depth
      false,   // Framebuffer has stencil
      0x1111,  // color bits
      false,   // depth mask
      false,   // depth enabled
      0,       // front stencil mask
      0,       // back stencil mask
      false,   // stencil enabled
      false,   // cull_face_enabled
      false,   // scissor_test_enabled
      false);  // blend_enabled

  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // But not again.
  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest, BeingQueryEXTDisabled) {
  // Test something fails if off.
}

TEST_F(GLES2DecoderManualInitTest, BeingEndQueryEXT) {
  InitDecoder(
      "GL_EXT_occlusion_query_boolean",      // extensions
      true,    // has alpha
      false,   // has depth
      false,   // has stencil
      true,    // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  // Test end fails if no begin.
  EndQueryEXT end_cmd;
  end_cmd.Init(GL_ANY_SAMPLES_PASSED_EXT, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(end_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  BeginQueryEXT begin_cmd;

  // Test id = 0 fails.
  begin_cmd.Init(
      GL_ANY_SAMPLES_PASSED_EXT, 0, kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(begin_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  GenHelper<GenQueriesEXTImmediate>(kNewClientId);

  // Test valid parameters work.
  EXPECT_CALL(*gl_, GenQueriesARB(1, _))
     .WillOnce(SetArgumentPointee<1>(kNewServiceId))
     .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BeginQueryARB(GL_ANY_SAMPLES_PASSED_EXT, kNewServiceId))
      .Times(1)
      .RetiresOnSaturation();
  begin_cmd.Init(
      GL_ANY_SAMPLES_PASSED_EXT, kNewClientId,
      kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(begin_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  QueryManager* query_manager = decoder_->GetQueryManager();
  ASSERT_TRUE(query_manager != NULL);
  QueryManager::Query* query = query_manager->GetQuery(kNewClientId);
  ASSERT_TRUE(query != NULL);
  EXPECT_FALSE(query->pending());

  // Test trying begin again fails
  EXPECT_EQ(error::kNoError, ExecuteCmd(begin_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // Test end fails with different target
  end_cmd.Init(GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(end_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // Test end succeeds
  EXPECT_CALL(*gl_, EndQueryARB(GL_ANY_SAMPLES_PASSED_EXT))
      .Times(1)
      .RetiresOnSaturation();
  end_cmd.Init(GL_ANY_SAMPLES_PASSED_EXT, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(end_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(query->pending());

  EXPECT_CALL(*gl_, DeleteQueriesARB(1, _))
      .Times(1)
      .RetiresOnSaturation();
}

static void CheckBeginEndQueryBadMemoryFails(
    GLES2DecoderTestBase* test,
    GLuint client_id,
    GLuint service_id,
    int32 shm_id,
    uint32 shm_offset) {
  ::testing::StrictMock< ::gfx::MockGLInterface>* gl = test->GetGLMock();

  BeginQueryEXT begin_cmd;

  test->GenHelper<GenQueriesEXTImmediate>(client_id);

  EXPECT_CALL(*gl, GenQueriesARB(1, _))
     .WillOnce(SetArgumentPointee<1>(service_id))
     .RetiresOnSaturation();
  EXPECT_CALL(*gl, BeginQueryARB(GL_ANY_SAMPLES_PASSED_EXT, service_id))
      .Times(1)
      .RetiresOnSaturation();

  // Test bad shared memory fails
  begin_cmd.Init(GL_ANY_SAMPLES_PASSED_EXT, client_id, shm_id, shm_offset);
  error::Error error1 = test->ExecuteCmd(begin_cmd);

  EXPECT_CALL(*gl, EndQueryARB(GL_ANY_SAMPLES_PASSED_EXT))
      .Times(1)
      .RetiresOnSaturation();

  EndQueryEXT end_cmd;
  end_cmd.Init(GL_ANY_SAMPLES_PASSED_EXT, 1);
  error::Error error2 = test->ExecuteCmd(end_cmd);

  EXPECT_CALL(*gl,
      GetQueryObjectuivARB(service_id, GL_QUERY_RESULT_AVAILABLE_EXT, _))
      .WillOnce(SetArgumentPointee<2>(1))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl,
      GetQueryObjectuivARB(service_id, GL_QUERY_RESULT_EXT, _))
      .WillOnce(SetArgumentPointee<2>(1))
      .RetiresOnSaturation();

  QueryManager* query_manager = test->GetDecoder()->GetQueryManager();
  ASSERT_TRUE(query_manager != NULL);
  bool process_success = query_manager->ProcessPendingQueries();

  EXPECT_TRUE(error1 != error::kNoError ||
              error2 != error::kNoError ||
              !process_success);

  EXPECT_CALL(*gl, DeleteQueriesARB(1, _))
      .Times(1)
      .RetiresOnSaturation();
}

TEST_F(GLES2DecoderManualInitTest, BeingEndQueryEXTBadMemoryIdFails) {
  InitDecoder(
      "GL_EXT_occlusion_query_boolean",      // extensions
      true,    // has alpha
      false,   // has depth
      false,   // has stencil
      true,    // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  CheckBeginEndQueryBadMemoryFails(
      this, kNewClientId, kNewServiceId,
      kInvalidSharedMemoryId, kSharedMemoryOffset);
}

TEST_F(GLES2DecoderManualInitTest, BeingEndQueryEXTBadMemoryOffsetFails) {
  InitDecoder(
      "GL_EXT_occlusion_query_boolean",      // extensions
      true,    // has alpha
      false,   // has depth
      false,   // has stencil
      true,    // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  CheckBeginEndQueryBadMemoryFails(
      this, kNewClientId, kNewServiceId,
      kSharedMemoryId, kInvalidSharedMemoryOffset);
}

TEST_F(GLES2DecoderTest, BeingEndQueryEXTCommandsIssuedCHROMIUM) {
  BeginQueryEXT begin_cmd;

  GenHelper<GenQueriesEXTImmediate>(kNewClientId);

  // Test valid parameters work.
  begin_cmd.Init(
      GL_COMMANDS_ISSUED_CHROMIUM, kNewClientId,
      kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(begin_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  QueryManager* query_manager = decoder_->GetQueryManager();
  ASSERT_TRUE(query_manager != NULL);
  QueryManager::Query* query = query_manager->GetQuery(kNewClientId);
  ASSERT_TRUE(query != NULL);
  EXPECT_FALSE(query->pending());

  // Test end succeeds
  EndQueryEXT end_cmd;
  end_cmd.Init(GL_COMMANDS_ISSUED_CHROMIUM, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(end_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_FALSE(query->pending());
}

TEST_F(GLES2DecoderTest, GenMailboxCHROMIUM) {
  const uint32 kBucketId = 123;

  GenMailboxCHROMIUM gen_mailbox_cmd;
  gen_mailbox_cmd.Init(kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(gen_mailbox_cmd));

  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  ASSERT_TRUE(bucket != NULL);
  ASSERT_EQ(static_cast<uint32>(GL_MAILBOX_SIZE_CHROMIUM), bucket->size());

  static const GLbyte zero[GL_MAILBOX_SIZE_CHROMIUM] = {
    0
  };
  EXPECT_NE(0, memcmp(zero,
                      bucket->GetData(0, GL_MAILBOX_SIZE_CHROMIUM),
                      sizeof(zero)));
}

TEST_F(GLES2DecoderTest, ProduceAndConsumeTextureCHROMIUM) {
  GLbyte mailbox[GL_MAILBOX_SIZE_CHROMIUM];
  group().mailbox_manager()->GenerateMailboxName(
      reinterpret_cast<MailboxName*>(mailbox));

  memcpy(shared_memory_address_, mailbox, sizeof(mailbox));

  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 3, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               0, 0);
  DoTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 2, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               0, 0);
  Texture* info =
      group().texture_manager()->GetTexture(client_texture_id_);
  EXPECT_EQ(kServiceTextureId, info->service_id());

  // Assigns and binds new service side texture ID.
  EXPECT_CALL(*gl_, GenTextures(1, _))
      .WillOnce(SetArgumentPointee<1>(kNewServiceId))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_2D, kNewServiceId))
      .Times(1)
      .RetiresOnSaturation();

  ProduceTextureCHROMIUM produce_cmd;
  produce_cmd.Init(GL_TEXTURE_2D, kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(produce_cmd));

  // Texture is zero-by-zero.
  GLsizei width;
  GLsizei height;
  GLenum type;
  GLenum internal_format;

  EXPECT_TRUE(info->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height));
  EXPECT_EQ(0, width);
  EXPECT_EQ(0, height);
  EXPECT_TRUE(info->GetLevelType(GL_TEXTURE_2D, 0, &type, &internal_format));
  EXPECT_EQ(static_cast<GLenum>(GL_RGBA), internal_format);
  EXPECT_EQ(static_cast<GLenum>(GL_UNSIGNED_BYTE), type);

  EXPECT_TRUE(info->GetLevelSize(GL_TEXTURE_2D, 1, &width, &height));
  EXPECT_EQ(0, width);
  EXPECT_EQ(0, height);
  EXPECT_TRUE(info->GetLevelType(GL_TEXTURE_2D, 1, &type, &internal_format));
  EXPECT_EQ(static_cast<GLenum>(GL_RGBA), internal_format);
  EXPECT_EQ(static_cast<GLenum>(GL_UNSIGNED_BYTE), type);

  // Service ID has changed.
  EXPECT_EQ(kNewServiceId, info->service_id());

  // Assigns and binds original service size texture ID.
  EXPECT_CALL(*gl_, DeleteTextures(1, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_2D, kServiceTextureId))
      .Times(1)
      .RetiresOnSaturation();

  ConsumeTextureCHROMIUM consume_cmd;
  consume_cmd.Init(GL_TEXTURE_2D, kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(consume_cmd));

  // Texture is redefined.
  EXPECT_TRUE(info->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height));
  EXPECT_EQ(3, width);
  EXPECT_EQ(1, height);
  EXPECT_TRUE(info->GetLevelType(GL_TEXTURE_2D, 0, &type, &internal_format));
  EXPECT_EQ(static_cast<GLenum>(GL_RGBA), internal_format);
  EXPECT_EQ(static_cast<GLenum>(GL_UNSIGNED_BYTE), type);

  EXPECT_TRUE(info->GetLevelSize(GL_TEXTURE_2D, 1, &width, &height));
  EXPECT_EQ(2, width);
  EXPECT_EQ(4, height);
  EXPECT_TRUE(info->GetLevelType(GL_TEXTURE_2D, 1, &type, &internal_format));
  EXPECT_EQ(static_cast<GLenum>(GL_RGBA), internal_format);
  EXPECT_EQ(static_cast<GLenum>(GL_UNSIGNED_BYTE), type);

  // Service ID is restored.
  EXPECT_EQ(kServiceTextureId, info->service_id());
}


TEST_F(GLES2DecoderTest, CanChangeSurface) {
  scoped_refptr<GLSurfaceMock> other_surface(new GLSurfaceMock);
  EXPECT_CALL(*other_surface.get(), GetBackingFrameBufferObject()).
      WillOnce(Return(7));
  EXPECT_CALL(*gl_, BindFramebufferEXT(GL_FRAMEBUFFER_EXT, 7));

  decoder_->SetSurface(other_surface);
}

TEST_F(GLES2DecoderTest, IsEnabledReturnsCachedValue) {
  // NOTE: There are no expectations because no GL functions should be
  // called for DEPTH_TEST or STENCIL_TEST
  static const GLenum kStates[] = {
    GL_DEPTH_TEST,
    GL_STENCIL_TEST,
  };
  for (size_t ii = 0; ii < arraysize(kStates); ++ii) {
    Enable enable_cmd;
    GLenum state = kStates[ii];
    enable_cmd.Init(state);
    EXPECT_EQ(error::kNoError, ExecuteCmd(enable_cmd));
    IsEnabled::Result* result =
        static_cast<IsEnabled::Result*>(shared_memory_address_);
    IsEnabled is_enabled_cmd;
    is_enabled_cmd.Init(state, shared_memory_id_, shared_memory_offset_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(is_enabled_cmd));
    EXPECT_NE(0u, *result);
    Disable disable_cmd;
    disable_cmd.Init(state);
    EXPECT_EQ(error::kNoError, ExecuteCmd(disable_cmd));
    EXPECT_EQ(error::kNoError, ExecuteCmd(is_enabled_cmd));
    EXPECT_EQ(0u, *result);
  }
}

TEST_F(GLES2DecoderManualInitTest, DepthTextureBadArgs) {
  InitDecoder(
      "GL_ANGLE_depth_texture",      // extensions
      false,   // has alpha
      true,    // has depth
      true,    // has stencil
      false,   // request alpha
      true,    // request depth
      true,    // request stencil
      true);   // bind generates resource

  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  // Check trying to upload data fails.
  TexImage2D tex_cmd;
  tex_cmd.Init(
      GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
      1, 1, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT,
      kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(tex_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  // Try level > 0.
  tex_cmd.Init(
      GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT,
      1, 1, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 0, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(tex_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  // Make a 1 pixel depth texture.
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
               1, 1, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 0, 0);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Check that trying to update it fails.
  TexSubImage2D tex_sub_cmd;
  tex_sub_cmd.Init(
      GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT,
      kSharedMemoryId, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(tex_sub_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // Check that trying to CopyTexImage2D fails
  CopyTexImage2D copy_tex_cmd;
  copy_tex_cmd.Init(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 0, 0, 1, 1, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(copy_tex_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // Check that trying to CopyTexSubImage2D fails
  CopyTexSubImage2D copy_sub_cmd;
  copy_sub_cmd.Init(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 1, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(copy_sub_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderManualInitTest, GenerateMipmapDepthTexture) {
  InitDecoder(
      "GL_ANGLE_depth_texture",      // extensions
      false,   // has alpha
      true,    // has depth
      true,    // has stencil
      false,   // request alpha
      true,    // request depth
      true,    // request stencil
      true);   // bind generates resource
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
               2, 2, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT,
               0, 0);
  GenerateMipmap cmd;
  cmd.Init(GL_TEXTURE_2D);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderANGLEManualInitTest, DrawClearsDepthTexture) {
  InitDecoder(
      "GL_ANGLE_depth_texture",      // extensions
      true,    // has alpha
      true,    // has depth
      false,   // has stencil
      true,    // request alpha
      true,    // request depth
      false,   // request stencil
      true);   // bind generates resource

  SetupDefaultProgram();
  SetupAllNeededVertexBuffers();
  const GLenum attachment = GL_DEPTH_ATTACHMENT;
  const GLenum target = GL_TEXTURE_2D;
  const GLint level = 0;
  DoBindTexture(target, client_texture_id_, kServiceTextureId);

  // Create a depth texture.
  DoTexImage2D(target, level, GL_DEPTH_COMPONENT, 1, 1, 0,
               GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 0, 0);

  EXPECT_CALL(*gl_, GenFramebuffersEXT(1, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, _))
      .Times(1)
      .RetiresOnSaturation();

  EXPECT_CALL(*gl_, FramebufferTexture2DEXT(
      GL_DRAW_FRAMEBUFFER_EXT, attachment, target, kServiceTextureId, level))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_DRAW_FRAMEBUFFER_EXT))
      .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
      .RetiresOnSaturation();

  EXPECT_CALL(*gl_, ClearStencil(0))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, StencilMask(-1))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, ClearDepth(1.0f))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, DepthMask(true))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, Disable(GL_SCISSOR_TEST))
      .Times(1)
      .RetiresOnSaturation();

  EXPECT_CALL(*gl_, Clear(GL_DEPTH_BUFFER_BIT))
      .Times(1)
      .RetiresOnSaturation();

  SetupExpectationsForRestoreClearState(
      0.0f, 0.0f, 0.0f, 0.0f, 0, 1.0f, false);

  EXPECT_CALL(*gl_, DeleteFramebuffersEXT(1, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, 0))
      .Times(1)
      .RetiresOnSaturation();

  SetupExpectationsForApplyingDefaultDirtyState();
  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, BindUniformLocationCHROMIUM) {
  const GLint kLocation = 2;
  const char* kName = "testing";
  const uint32 kNameSize = strlen(kName);
  const char* kBadName1 = "gl_testing";
  const uint32 kBadName1Size = strlen(kBadName1);
  const char* kBadName2 = "testing[1]";
  const uint32 kBadName2Size = strlen(kBadName2);
  memcpy(shared_memory_address_, kName, kNameSize);
  BindUniformLocationCHROMIUM cmd;
  cmd.Init(client_program_id_, kLocation, kSharedMemoryId, kSharedMemoryOffset,
           kNameSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  // check negative location
  memcpy(shared_memory_address_, kName, kNameSize);
  cmd.Init(client_program_id_, -1, kSharedMemoryId, kSharedMemoryOffset,
           kNameSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  // check highest location
  memcpy(shared_memory_address_, kName, kNameSize);
  GLint kMaxLocation =
      (kMaxFragmentUniformVectors + kMaxVertexUniformVectors) * 4 - 1;
  cmd.Init(client_program_id_, kMaxLocation, kSharedMemoryId,
           kSharedMemoryOffset, kNameSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  // check too high location
  memcpy(shared_memory_address_, kName, kNameSize);
  cmd.Init(client_program_id_, kMaxLocation + 1, kSharedMemoryId,
           kSharedMemoryOffset, kNameSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  // check bad name "gl_..."
  memcpy(shared_memory_address_, kBadName1, kBadName1Size);
  cmd.Init(client_program_id_, kLocation, kSharedMemoryId, kSharedMemoryOffset,
           kBadName1Size);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  // check bad name "name[1]" non zero
  memcpy(shared_memory_address_, kBadName2, kBadName2Size);
  cmd.Init(client_program_id_, kLocation, kSharedMemoryId, kSharedMemoryOffset,
           kBadName2Size);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

class GLES2DecoderVertexArraysOESTest : public GLES2DecoderWithShaderTest {
 public:
  GLES2DecoderVertexArraysOESTest() { }

  bool vertex_array_deleted_manually_;

  virtual void SetUp() {
    InitDecoder(
        "GL_OES_vertex_array_object",  // extensions
        false,  // has alpha
        false,  // has depth
        false,  // has stencil
        false,  // request alpha
        false,  // request depth
        false,  // request stencil
        true);  // bind generates resource
    SetupDefaultProgram();

    AddExpectationsForGenVertexArraysOES();
    GenHelper<GenVertexArraysOESImmediate>(client_vertexarray_id_);

    vertex_array_deleted_manually_ = false;
  }

  virtual void TearDown() {
    // This should only be set if the test handled deletion of the vertex array
    // itself. Necessary because vertex_array_objects are not sharable, and thus
    // not managed in the ContextGroup, meaning they will be destroyed during
    // test tear down
    if (!vertex_array_deleted_manually_) {
      AddExpectationsForDeleteVertexArraysOES();
    }

    GLES2DecoderWithShaderTest::TearDown();
  }

  void GenVertexArraysOESValidArgs() {
    AddExpectationsForGenVertexArraysOES();
    GetSharedMemoryAs<GLuint*>()[0] = kNewClientId;
    GenVertexArraysOES cmd;
    cmd.Init(1, shared_memory_id_, shared_memory_offset_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
    EXPECT_TRUE(GetVertexArrayInfo(kNewClientId) != NULL);
    AddExpectationsForDeleteVertexArraysOES();
  }

  void GenVertexArraysOESInvalidArgs() {
    EXPECT_CALL(*gl_, GenVertexArraysOES(_, _)).Times(0);
    GetSharedMemoryAs<GLuint*>()[0] = client_vertexarray_id_;
    GenVertexArraysOES cmd;
    cmd.Init(1, shared_memory_id_, shared_memory_offset_);
    EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
  }

  void GenVertexArraysOESImmediateValidArgs() {
    AddExpectationsForGenVertexArraysOES();
    GenVertexArraysOESImmediate* cmd =
        GetImmediateAs<GenVertexArraysOESImmediate>();
    GLuint temp = kNewClientId;
    cmd->Init(1, &temp);
    EXPECT_EQ(error::kNoError,
              ExecuteImmediateCmd(*cmd, sizeof(temp)));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
    EXPECT_TRUE(GetVertexArrayInfo(kNewClientId) != NULL);
    AddExpectationsForDeleteVertexArraysOES();
  }

  void GenVertexArraysOESImmediateInvalidArgs() {
    EXPECT_CALL(*gl_, GenVertexArraysOES(_, _)).Times(0);
    GenVertexArraysOESImmediate* cmd =
        GetImmediateAs<GenVertexArraysOESImmediate>();
    cmd->Init(1, &client_vertexarray_id_);
    EXPECT_EQ(error::kInvalidArguments,
              ExecuteImmediateCmd(*cmd, sizeof(&client_vertexarray_id_)));
  }

  void DeleteVertexArraysOESValidArgs() {
    AddExpectationsForDeleteVertexArraysOES();
    GetSharedMemoryAs<GLuint*>()[0] = client_vertexarray_id_;
    DeleteVertexArraysOES cmd;
    cmd.Init(1, shared_memory_id_, shared_memory_offset_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
    EXPECT_TRUE(
        GetVertexArrayInfo(client_vertexarray_id_) == NULL);
    vertex_array_deleted_manually_ = true;
  }

  void DeleteVertexArraysOESInvalidArgs() {
    GetSharedMemoryAs<GLuint*>()[0] = kInvalidClientId;
    DeleteVertexArraysOES cmd;
    cmd.Init(1, shared_memory_id_, shared_memory_offset_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }

  void DeleteVertexArraysOESImmediateValidArgs() {
    AddExpectationsForDeleteVertexArraysOES();
    DeleteVertexArraysOESImmediate& cmd =
        *GetImmediateAs<DeleteVertexArraysOESImmediate>();
    cmd.Init(1, &client_vertexarray_id_);
    EXPECT_EQ(error::kNoError,
              ExecuteImmediateCmd(cmd, sizeof(client_vertexarray_id_)));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
    EXPECT_TRUE(
        GetVertexArrayInfo(client_vertexarray_id_) == NULL);
    vertex_array_deleted_manually_ = true;
  }

  void DeleteVertexArraysOESImmediateInvalidArgs() {
    DeleteVertexArraysOESImmediate& cmd =
        *GetImmediateAs<DeleteVertexArraysOESImmediate>();
    GLuint temp = kInvalidClientId;
    cmd.Init(1, &temp);
    EXPECT_EQ(error::kNoError,
              ExecuteImmediateCmd(cmd, sizeof(temp)));
  }

  void IsVertexArrayOESValidArgs() {
    IsVertexArrayOES cmd;
    cmd.Init(client_vertexarray_id_, shared_memory_id_, shared_memory_offset_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }

  void IsVertexArrayOESInvalidArgsBadSharedMemoryId() {
    IsVertexArrayOES cmd;
    cmd.Init(
        client_vertexarray_id_, kInvalidSharedMemoryId, shared_memory_offset_);
    EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
    cmd.Init(
        client_vertexarray_id_, shared_memory_id_, kInvalidSharedMemoryOffset);
    EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  }

  void BindVertexArrayOESValidArgs() {
    AddExpectationsForBindVertexArrayOES();
    BindVertexArrayOES cmd;
    cmd.Init(client_vertexarray_id_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }

  void BindVertexArrayOESValidArgsNewId() {
    BindVertexArrayOES cmd;
    cmd.Init(kNewClientId);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  }
};

class GLES2DecoderEmulatedVertexArraysOESTest
    : public GLES2DecoderVertexArraysOESTest {
 public:
  GLES2DecoderEmulatedVertexArraysOESTest() { }

  virtual void SetUp() {
    InitDecoder(
        "",     // extensions
        false,  // has alpha
        false,  // has depth
        false,  // has stencil
        false,  // request alpha
        false,  // request depth
        false,  // request stencil
        true);  // bind generates resource
    SetupDefaultProgram();

    AddExpectationsForGenVertexArraysOES();
    GenHelper<GenVertexArraysOESImmediate>(client_vertexarray_id_);

    vertex_array_deleted_manually_ = false;
  }
};

// Test vertex array objects with native support
TEST_F(GLES2DecoderVertexArraysOESTest, GenVertexArraysOESValidArgs) {
  GenVertexArraysOESValidArgs();
}
TEST_F(GLES2DecoderEmulatedVertexArraysOESTest, GenVertexArraysOESValidArgs) {
  GenVertexArraysOESValidArgs();
}

TEST_F(GLES2DecoderVertexArraysOESTest, GenVertexArraysOESInvalidArgs) {
  GenVertexArraysOESInvalidArgs();
}
TEST_F(GLES2DecoderEmulatedVertexArraysOESTest, ) {
  GenVertexArraysOESInvalidArgs();
}

TEST_F(GLES2DecoderVertexArraysOESTest, GenVertexArraysOESImmediateValidArgs) {
  GenVertexArraysOESImmediateValidArgs();
}
TEST_F(GLES2DecoderEmulatedVertexArraysOESTest,
    GenVertexArraysOESImmediateValidArgs) {
  GenVertexArraysOESImmediateValidArgs();
}

TEST_F(GLES2DecoderVertexArraysOESTest,
    GenVertexArraysOESImmediateInvalidArgs) {
  GenVertexArraysOESImmediateInvalidArgs();
}
TEST_F(GLES2DecoderEmulatedVertexArraysOESTest,
    GenVertexArraysOESImmediateInvalidArgs) {
  GenVertexArraysOESImmediateInvalidArgs();
}

TEST_F(GLES2DecoderVertexArraysOESTest, DeleteVertexArraysOESValidArgs) {
  DeleteVertexArraysOESValidArgs();
}
TEST_F(GLES2DecoderEmulatedVertexArraysOESTest,
    DeleteVertexArraysOESValidArgs) {
  DeleteVertexArraysOESValidArgs();
}

TEST_F(GLES2DecoderVertexArraysOESTest, DeleteVertexArraysOESInvalidArgs) {
  DeleteVertexArraysOESInvalidArgs();
}
TEST_F(GLES2DecoderEmulatedVertexArraysOESTest,
    DeleteVertexArraysOESInvalidArgs) {
  DeleteVertexArraysOESInvalidArgs();
}

TEST_F(GLES2DecoderVertexArraysOESTest,
    DeleteVertexArraysOESImmediateValidArgs) {
  DeleteVertexArraysOESImmediateValidArgs();
}
TEST_F(GLES2DecoderEmulatedVertexArraysOESTest,
    DeleteVertexArraysOESImmediateValidArgs) {
  DeleteVertexArraysOESImmediateValidArgs();
}

TEST_F(GLES2DecoderVertexArraysOESTest,
    DeleteVertexArraysOESImmediateInvalidArgs) {
  DeleteVertexArraysOESImmediateInvalidArgs();
}
TEST_F(GLES2DecoderEmulatedVertexArraysOESTest,
    DeleteVertexArraysOESImmediateInvalidArgs) {
  DeleteVertexArraysOESImmediateInvalidArgs();
}

TEST_F(GLES2DecoderVertexArraysOESTest, IsVertexArrayOESValidArgs) {
  IsVertexArrayOESValidArgs();
}
TEST_F(GLES2DecoderEmulatedVertexArraysOESTest, IsVertexArrayOESValidArgs) {
  IsVertexArrayOESValidArgs();
}

TEST_F(GLES2DecoderVertexArraysOESTest,
    IsVertexArrayOESInvalidArgsBadSharedMemoryId) {
  IsVertexArrayOESInvalidArgsBadSharedMemoryId();
}
TEST_F(GLES2DecoderEmulatedVertexArraysOESTest,
    IsVertexArrayOESInvalidArgsBadSharedMemoryId) {
  IsVertexArrayOESInvalidArgsBadSharedMemoryId();
}

TEST_F(GLES2DecoderVertexArraysOESTest, BindVertexArrayOESValidArgs) {
  BindVertexArrayOESValidArgs();
}
TEST_F(GLES2DecoderEmulatedVertexArraysOESTest, BindVertexArrayOESValidArgs) {
  BindVertexArrayOESValidArgs();
}

TEST_F(GLES2DecoderVertexArraysOESTest, BindVertexArrayOESValidArgsNewId) {
  BindVertexArrayOESValidArgsNewId();
}
TEST_F(GLES2DecoderEmulatedVertexArraysOESTest,
    BindVertexArrayOESValidArgsNewId) {
  BindVertexArrayOESValidArgsNewId();
}

TEST_F(GLES2DecoderTest, BindTexImage2DCHROMIUM) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 3, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               0, 0);
  Texture* info =
      group().texture_manager()->GetTexture(client_texture_id_);
  EXPECT_EQ(kServiceTextureId, info->service_id());

  group().image_manager()->AddImage(gfx::GLImage::CreateGLImage(0), 1);
  EXPECT_FALSE(group().image_manager()->LookupImage(1) == NULL);

  GLsizei width;
  GLsizei height;
  GLenum type;
  GLenum internal_format;

  EXPECT_TRUE(info->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height));
  EXPECT_EQ(3, width);
  EXPECT_EQ(1, height);
  EXPECT_TRUE(info->GetLevelType(GL_TEXTURE_2D, 0, &type, &internal_format));
  EXPECT_EQ(static_cast<GLenum>(GL_RGBA), internal_format);
  EXPECT_EQ(static_cast<GLenum>(GL_UNSIGNED_BYTE), type);
  EXPECT_TRUE(info->GetLevelImage(GL_TEXTURE_2D, 0) == NULL);

  // Bind image to texture.
  BindTexImage2DCHROMIUM bind_tex_image_2d_cmd;
  bind_tex_image_2d_cmd.Init(GL_TEXTURE_2D, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(bind_tex_image_2d_cmd));
  EXPECT_TRUE(info->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height));
  // Image should now be set.
  EXPECT_FALSE(info->GetLevelImage(GL_TEXTURE_2D, 0) == NULL);

  // Define new texture image.
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 3, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               0, 0);
  EXPECT_TRUE(info->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height));
  // Image should no longer be set.
  EXPECT_TRUE(info->GetLevelImage(GL_TEXTURE_2D, 0) == NULL);
}

TEST_F(GLES2DecoderTest, ReleaseTexImage2DCHROMIUM) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 3, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               0, 0);
  Texture* info =
      group().texture_manager()->GetTexture(client_texture_id_);
  EXPECT_EQ(kServiceTextureId, info->service_id());

  group().image_manager()->AddImage(gfx::GLImage::CreateGLImage(0), 1);
  EXPECT_FALSE(group().image_manager()->LookupImage(1) == NULL);

  GLsizei width;
  GLsizei height;
  GLenum type;
  GLenum internal_format;

  EXPECT_TRUE(info->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height));
  EXPECT_EQ(3, width);
  EXPECT_EQ(1, height);
  EXPECT_TRUE(info->GetLevelType(GL_TEXTURE_2D, 0, &type, &internal_format));
  EXPECT_EQ(static_cast<GLenum>(GL_RGBA), internal_format);
  EXPECT_EQ(static_cast<GLenum>(GL_UNSIGNED_BYTE), type);
  EXPECT_TRUE(info->GetLevelImage(GL_TEXTURE_2D, 0) == NULL);

  // Bind image to texture.
  BindTexImage2DCHROMIUM bind_tex_image_2d_cmd;
  bind_tex_image_2d_cmd.Init(GL_TEXTURE_2D, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(bind_tex_image_2d_cmd));
  EXPECT_TRUE(info->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height));
  // Image should now be set.
  EXPECT_FALSE(info->GetLevelImage(GL_TEXTURE_2D, 0) == NULL);

  // Release image from texture.
  ReleaseTexImage2DCHROMIUM release_tex_image_2d_cmd;
  release_tex_image_2d_cmd.Init(GL_TEXTURE_2D, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(release_tex_image_2d_cmd));
  EXPECT_TRUE(info->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height));
  // Image should no longer be set.
  EXPECT_TRUE(info->GetLevelImage(GL_TEXTURE_2D, 0) == NULL);
}

TEST_F(GLES2DecoderManualInitTest, GpuMemoryManagerCHROMIUM) {
  InitDecoder(
      "GL_ARB_texture_rectangle",  // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource

  Texture* info = GetTexture(client_texture_id_);
  EXPECT_TRUE(info != NULL);
  EXPECT_TRUE(info->pool() == GL_TEXTURE_POOL_UNMANAGED_CHROMIUM);

  DoBindTexture(
      GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);

  TexParameteri cmd;
  cmd.Init(GL_TEXTURE_2D,
           GL_TEXTURE_POOL_CHROMIUM,
           GL_TEXTURE_POOL_UNMANAGED_CHROMIUM);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  cmd.Init(GL_TEXTURE_2D,
           GL_TEXTURE_POOL_CHROMIUM,
           GL_TEXTURE_POOL_MANAGED_CHROMIUM);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  EXPECT_TRUE(info->pool() == GL_TEXTURE_POOL_MANAGED_CHROMIUM);

  cmd.Init(GL_TEXTURE_2D,
           GL_TEXTURE_POOL_CHROMIUM,
           GL_NONE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderManualInitTest, AsyncPixelTransfers) {
  InitDecoder(
      "GL_CHROMIUM_async_pixel_transfers",  // extensions
      false, false, false,  // has alpha/depth/stencil
      false, false, false,  // request alpha/depth/stencil
      true);   // bind generates resource

  // Set up the texture.
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  Texture* info = GetTexture(client_texture_id_);

  // Set a mock Async delegate
  // Async state is returned as a scoped_ptr, but we keep a raw copy.
  StrictMock<gfx::MockAsyncPixelTransferDelegate>* delegate =
      new StrictMock<gfx::MockAsyncPixelTransferDelegate>;
  decoder_->SetAsyncPixelTransferDelegate(delegate);
  StrictMock<gfx::MockAsyncPixelTransferState>* state = NULL;

  // Tex(Sub)Image2D upload commands.
  AsyncTexImage2DCHROMIUM teximage_cmd;
  teximage_cmd.Init(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA,
                    GL_UNSIGNED_BYTE, kSharedMemoryId, kSharedMemoryOffset);
  AsyncTexSubImage2DCHROMIUM texsubimage_cmd;
  texsubimage_cmd.Init(GL_TEXTURE_2D, 0, 0, 0, 8, 8, GL_RGBA,
                      GL_UNSIGNED_BYTE, kSharedMemoryId, kSharedMemoryOffset);
  gfx::AsyncTexImage2DParams teximage_params =
      {GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE};

  // No transfer state exists initially.
  EXPECT_FALSE(info->GetAsyncTransferState());

  // AsyncTexImage2D
  {
    // Create transfer state since it doesn't exist.
    EXPECT_CALL(*delegate, CreateRawPixelTransferState(kServiceTextureId))
        .WillOnce(Return(
            state = new StrictMock<gfx::MockAsyncPixelTransferState>))
        .RetiresOnSaturation();
    EXPECT_CALL(*delegate, AsyncTexImage2D(state, _, _))
        .RetiresOnSaturation();
    // Command succeeds.
    EXPECT_EQ(error::kNoError, ExecuteCmd(teximage_cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
    EXPECT_TRUE(info->GetAsyncTransferState());
    EXPECT_TRUE(info->IsImmutable());
    // The texture is safe but the level has not been defined yet.
    EXPECT_TRUE(info->SafeToRenderFrom());
    GLsizei width, height;
    EXPECT_FALSE(info->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height));
  }
  {
    // Async redefinitions are not allowed!
    // Command fails.
    EXPECT_EQ(error::kNoError, ExecuteCmd(teximage_cmd));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
    EXPECT_TRUE(info->GetAsyncTransferState());
    EXPECT_TRUE(info->IsImmutable());
    EXPECT_TRUE(info->SafeToRenderFrom());
  }

  // Lazy binding/defining of the async transfer
  {
    // We the code should check that the transfer is done,
    // call bind transfer on it, and update the texture info.
    InSequence scoped_in_sequence;
    EXPECT_CALL(*state, TransferIsInProgress())
        .WillOnce(Return(false))
        .RetiresOnSaturation();
    EXPECT_CALL(*state, BindTransfer(_))
        .WillOnce(SetArgPointee<0>(teximage_params))
        .RetiresOnSaturation();
    TextureManager* manager = decoder_->GetContextGroup()->texture_manager();
    bool texture_dirty, framebuffer_dirty;
    manager->BindFinishedAsyncPixelTransfers(&texture_dirty,
                                             &framebuffer_dirty);
    EXPECT_TRUE(texture_dirty);
    EXPECT_FALSE(framebuffer_dirty);
    // Texture is safe, and has the right size etc.
    EXPECT_TRUE(info->SafeToRenderFrom());
    GLsizei width, height;
    EXPECT_TRUE(info->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height));
    EXPECT_EQ(width, 8);
    EXPECT_EQ(height, 8);
  }

  // AsyncTexSubImage2D
  info->SetAsyncTransferState(scoped_ptr<gfx::AsyncPixelTransferState>());
  info->SetImmutable(false);
  {
    // Create transfer state since it doesn't exist.
    EXPECT_CALL(*delegate, CreateRawPixelTransferState(kServiceTextureId))
        .WillOnce(Return(
            state = new StrictMock<gfx::MockAsyncPixelTransferState>))
        .RetiresOnSaturation();
    EXPECT_CALL(*delegate, AsyncTexSubImage2D(state, _, _))
        .RetiresOnSaturation();
    // Command succeeds.
    EXPECT_EQ(error::kNoError, ExecuteCmd(texsubimage_cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
    EXPECT_TRUE(info->GetAsyncTransferState());
    EXPECT_TRUE(info->IsImmutable());
    EXPECT_TRUE(info->SafeToRenderFrom());
  }
  {
    // No transfer is in progress.
    EXPECT_CALL(*state, TransferIsInProgress())
        .WillOnce(Return(false))  // texSubImage validation
        .WillOnce(Return(false))  // async validation
        .RetiresOnSaturation();
    EXPECT_CALL(*delegate, AsyncTexSubImage2D(state, _, _))
        .RetiresOnSaturation();
    // Command succeeds.
    EXPECT_EQ(error::kNoError, ExecuteCmd(texsubimage_cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
    EXPECT_TRUE(info->GetAsyncTransferState());
    EXPECT_TRUE(info->IsImmutable());
    EXPECT_TRUE(info->SafeToRenderFrom());
  }
  {
    // A transfer is still in progress!
    EXPECT_CALL(*state, TransferIsInProgress())
        .WillOnce(Return(true))
        .RetiresOnSaturation();
    // No async call, command fails.
    EXPECT_EQ(error::kNoError, ExecuteCmd(texsubimage_cmd));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
    EXPECT_TRUE(info->GetAsyncTransferState());
    EXPECT_TRUE(info->IsImmutable());
    EXPECT_TRUE(info->SafeToRenderFrom());
  }

  decoder_->SetAsyncPixelTransferDelegate(NULL);
  info->SetAsyncTransferState(scoped_ptr<gfx::AsyncPixelTransferState>());
}

namespace {

class SizeOnlyMemoryTracker : public MemoryTracker {
 public:
  SizeOnlyMemoryTracker() {
    // These are the default textures. 1 for TEXTURE_2D and 6 faces for
    // TEXTURE_CUBE_MAP.
    const size_t kInitialUnmanagedPoolSize = 7 * 4;
    const size_t kInitialManagedPoolSize = 0;
    pool_infos_[MemoryTracker::kUnmanaged].initial_size =
        kInitialUnmanagedPoolSize;
    pool_infos_[MemoryTracker::kManaged].initial_size =
        kInitialManagedPoolSize;
  }

  // Ensure a certain amount of GPU memory is free. Returns true on success.
  MOCK_METHOD1(EnsureGPUMemoryAvailable, bool(size_t size_needed));

  virtual void TrackMemoryAllocatedChange(
      size_t old_size, size_t new_size, Pool pool) {
    PoolInfo& info = pool_infos_[pool];
    info.size += new_size - old_size;
  }

  size_t GetPoolSize(Pool pool) {
    const PoolInfo& info = pool_infos_[pool];
    return info.size - info.initial_size;
  }

 private:
  virtual ~SizeOnlyMemoryTracker() {
  }
  struct PoolInfo {
    PoolInfo()
        : initial_size(0),
          size(0) {
    }
    size_t initial_size;
    size_t size;
  };
  std::map<Pool, PoolInfo> pool_infos_;
};

}  // anonymous namespace.

TEST_F(GLES2DecoderManualInitTest, MemoryTrackerInitialSize) {
  scoped_refptr<SizeOnlyMemoryTracker> memory_tracker =
      new SizeOnlyMemoryTracker();
  set_memory_tracker(memory_tracker.get());
  InitDecoder(
      "",      // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource
  // Expect that initial size - size is 0.
  EXPECT_EQ(0u, memory_tracker->GetPoolSize(MemoryTracker::kUnmanaged));
  EXPECT_EQ(0u, memory_tracker->GetPoolSize(MemoryTracker::kManaged));
}

TEST_F(GLES2DecoderManualInitTest, MemoryTrackerTexImage2D) {
  scoped_refptr<SizeOnlyMemoryTracker> memory_tracker =
      new SizeOnlyMemoryTracker();
  set_memory_tracker(memory_tracker.get());
  InitDecoder(
      "",      // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  EXPECT_CALL(*memory_tracker, EnsureGPUMemoryAvailable(128))
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(128u, memory_tracker->GetPoolSize(MemoryTracker::kUnmanaged));
  EXPECT_CALL(*memory_tracker, EnsureGPUMemoryAvailable(64))
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(64u, memory_tracker->GetPoolSize(MemoryTracker::kUnmanaged));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  // Check we get out of memory and no call to glTexImage2D if Ensure fails.
  EXPECT_CALL(*memory_tracker, EnsureGPUMemoryAvailable(64))
      .WillOnce(Return(false))
      .RetiresOnSaturation();
  TexImage2D cmd;
  cmd.Init(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
  EXPECT_EQ(64u, memory_tracker->GetPoolSize(MemoryTracker::kUnmanaged));
}

TEST_F(GLES2DecoderManualInitTest, MemoryTrackerTexStorage2DEXT) {
  scoped_refptr<SizeOnlyMemoryTracker> memory_tracker =
      new SizeOnlyMemoryTracker();
  set_memory_tracker(memory_tracker.get());
  InitDecoder(
      "",      // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  // Check we get out of memory and no call to glTexStorage2DEXT
  // if Ensure fails.
  EXPECT_CALL(*memory_tracker, EnsureGPUMemoryAvailable(128))
      .WillOnce(Return(false))
      .RetiresOnSaturation();
  TexStorage2DEXT cmd;
  cmd.Init(GL_TEXTURE_2D, 1, GL_RGBA8, 8, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0u, memory_tracker->GetPoolSize(MemoryTracker::kUnmanaged));
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
}

TEST_F(GLES2DecoderManualInitTest, MemoryTrackerCopyTexImage2D) {
  GLenum target = GL_TEXTURE_2D;
  GLint level = 0;
  GLenum internal_format = GL_RGBA;
  GLsizei width = 4;
  GLsizei height = 8;
  GLint border = 0;
  scoped_refptr<SizeOnlyMemoryTracker> memory_tracker =
      new SizeOnlyMemoryTracker();
  set_memory_tracker(memory_tracker.get());
  InitDecoder(
      "",      // extensions
      true,    // has alpha
      false,   // has depth
      false,   // has stencil
      true,    // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  EXPECT_CALL(*memory_tracker, EnsureGPUMemoryAvailable(128))
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, CopyTexImage2D(
      target, level, internal_format, 0, 0, width, height, border))
      .Times(1)
      .RetiresOnSaturation();
  CopyTexImage2D cmd;
  cmd.Init(target, level, internal_format, 0, 0, width, height, border);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(128u, memory_tracker->GetPoolSize(MemoryTracker::kUnmanaged));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  // Check we get out of memory and no call to glCopyTexImage2D if Ensure fails.
  EXPECT_CALL(*memory_tracker, EnsureGPUMemoryAvailable(128))
      .WillOnce(Return(false))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
  EXPECT_EQ(128u, memory_tracker->GetPoolSize(MemoryTracker::kUnmanaged));
}

TEST_F(GLES2DecoderManualInitTest, MemoryTrackerRenderbufferStorage) {
  scoped_refptr<SizeOnlyMemoryTracker> memory_tracker =
      new SizeOnlyMemoryTracker();
  set_memory_tracker(memory_tracker.get());
  InitDecoder(
      "",      // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource
  DoBindRenderbuffer(GL_RENDERBUFFER, client_renderbuffer_id_,
                    kServiceRenderbufferId);
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*memory_tracker, EnsureGPUMemoryAvailable(128))
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, RenderbufferStorageEXT(
      GL_RENDERBUFFER, GL_RGBA, 8, 4))
      .Times(1)
      .RetiresOnSaturation();
  RenderbufferStorage cmd;
  cmd.Init(GL_RENDERBUFFER, GL_RGBA4, 8, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(128u, memory_tracker->GetPoolSize(MemoryTracker::kUnmanaged));
  // Check we get out of memory and no call to glRenderbufferStorage if Ensure
  // fails.
  EXPECT_CALL(*memory_tracker, EnsureGPUMemoryAvailable(128))
      .WillOnce(Return(false))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
  EXPECT_EQ(128u, memory_tracker->GetPoolSize(MemoryTracker::kUnmanaged));
}

TEST_F(GLES2DecoderManualInitTest, MemoryTrackerBufferData) {
  scoped_refptr<SizeOnlyMemoryTracker> memory_tracker =
      new SizeOnlyMemoryTracker();
  set_memory_tracker(memory_tracker.get());
  InitDecoder(
      "",      // extensions
      false,   // has alpha
      false,   // has depth
      false,   // has stencil
      false,   // request alpha
      false,   // request depth
      false,   // request stencil
      true);   // bind generates resource
  DoBindBuffer(GL_ARRAY_BUFFER, client_buffer_id_,
               kServiceBufferId);
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*memory_tracker, EnsureGPUMemoryAvailable(128))
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BufferData(GL_ARRAY_BUFFER, 128, _, GL_STREAM_DRAW))
      .Times(1)
      .RetiresOnSaturation();
  BufferData cmd;
  cmd.Init(GL_ARRAY_BUFFER, 128, 0, 0, GL_STREAM_DRAW);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(128u, memory_tracker->GetPoolSize(MemoryTracker::kManaged));
  // Check we get out of memory and no call to glBufferData if Ensure
  // fails.
  EXPECT_CALL(*memory_tracker, EnsureGPUMemoryAvailable(128))
      .WillOnce(Return(false))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
  EXPECT_EQ(128u, memory_tracker->GetPoolSize(MemoryTracker::kManaged));
}

// TODO(gman): Complete this test.
// TEST_F(GLES2DecoderTest, CompressedTexImage2DGLError) {
// }

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
