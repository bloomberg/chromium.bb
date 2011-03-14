// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest_base.h"

#include <algorithm>
#include <string>

#include "base/string_number_conversions.h"
#include "gpu/command_buffer/common/gl_mock.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/test_helper.h"
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
using ::testing::StrictMock;

namespace gpu {
namespace gles2 {

GLES2DecoderTestBase::GLES2DecoderTestBase()
    : client_buffer_id_(100),
      client_framebuffer_id_(101),
      client_program_id_(102),
      client_renderbuffer_id_(103),
      client_shader_id_(104),
      client_texture_id_(106),
      client_element_buffer_id_(107) {
  memset(immediate_buffer_, 0xEE, sizeof(immediate_buffer_));
}

GLES2DecoderTestBase::~GLES2DecoderTestBase() {}

void GLES2DecoderTestBase::SetUp() {
  InitDecoder("", true);
}

void GLES2DecoderTestBase::InitDecoder(
    const char* extensions, bool has_alpha_backbuffer) {
  gl_.reset(new StrictMock<MockGLInterface>());
  ::gfx::GLInterface::SetGLInterface(gl_.get());
  group_ = ContextGroup::Ref(new ContextGroup());

  InSequence sequence;

  TestHelper::SetupContextGroupInitExpectations(gl_.get(),
      DisallowedExtensions(), extensions);

  EXPECT_TRUE(group_->Initialize(DisallowedExtensions(), extensions));

  EXPECT_CALL(*gl_, GetIntegerv(GL_ALPHA_BITS, _))
      .WillOnce(SetArgumentPointee<1>(has_alpha_backbuffer ? 8 : 0))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, EnableVertexAttribArray(0))
      .Times(1)
      .RetiresOnSaturation();
  static GLuint attrib_0_id[] = {
    kServiceAttrib0BufferId,
  };
  static GLuint fixed_attrib_buffer_id[] = {
    kServiceFixedAttribBufferId,
  };
  EXPECT_CALL(*gl_, GenBuffersARB(arraysize(attrib_0_id), _))
      .WillOnce(SetArrayArgument<1>(attrib_0_id,
                                    attrib_0_id + arraysize(attrib_0_id)))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BindBuffer(GL_ARRAY_BUFFER, kServiceAttrib0BufferId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, VertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 0, NULL))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BindBuffer(GL_ARRAY_BUFFER, 0))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GenBuffersARB(arraysize(fixed_attrib_buffer_id), _))
      .WillOnce(SetArrayArgument<1>(
          fixed_attrib_buffer_id,
          fixed_attrib_buffer_id + arraysize(fixed_attrib_buffer_id)))
      .RetiresOnSaturation();

  for (GLint tt = 0; tt < TestHelper::kNumTextureUnits; ++tt) {
    EXPECT_CALL(*gl_, ActiveTexture(GL_TEXTURE0 + tt))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, BindTexture(
        GL_TEXTURE_CUBE_MAP, TestHelper::kServiceDefaultTextureCubemapId))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, BindTexture(
        GL_TEXTURE_2D, TestHelper::kServiceDefaultTexture2dId))
        .Times(1)
        .RetiresOnSaturation();
  }
  EXPECT_CALL(*gl_, ActiveTexture(GL_TEXTURE0))
      .Times(1)
      .RetiresOnSaturation();

  EXPECT_CALL(*gl_, Enable(GL_VERTEX_PROGRAM_POINT_SIZE))
      .Times(1)
      .RetiresOnSaturation();

  EXPECT_CALL(*gl_, Enable(GL_POINT_SPRITE))
      .Times(1)
      .RetiresOnSaturation();

  engine_.reset(new StrictMock<MockCommandBufferEngine>());
  Buffer buffer = engine_->GetSharedMemoryBuffer(kSharedMemoryId);
  shared_memory_offset_ = kSharedMemoryOffset;
  shared_memory_address_ = reinterpret_cast<int8*>(buffer.ptr) +
      shared_memory_offset_;
  shared_memory_id_ = kSharedMemoryId;

  context_ = new gfx::StubGLContext;
  context_->SetSize(gfx::Size(kBackBufferWidth, kBackBufferHeight));

  decoder_.reset(GLES2Decoder::Create(group_.get()));
  decoder_->Initialize(
      context_, context_->GetSize(), DisallowedExtensions(),
      NULL, std::vector<int32>(), NULL, 0);
  decoder_->set_engine(engine_.get());

  EXPECT_CALL(*gl_, GenBuffersARB(_, _))
      .WillOnce(SetArgumentPointee<1>(kServiceBufferId))
      .RetiresOnSaturation();
  GenHelper<GenBuffersImmediate>(client_buffer_id_);
  EXPECT_CALL(*gl_, GenFramebuffersEXT(_, _))
      .WillOnce(SetArgumentPointee<1>(kServiceFramebufferId))
      .RetiresOnSaturation();
  GenHelper<GenFramebuffersImmediate>(client_framebuffer_id_);
  EXPECT_CALL(*gl_, GenRenderbuffersEXT(_, _))
      .WillOnce(SetArgumentPointee<1>(kServiceRenderbufferId))
      .RetiresOnSaturation();
  GenHelper<GenRenderbuffersImmediate>(client_renderbuffer_id_);
  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgumentPointee<1>(kServiceTextureId))
      .RetiresOnSaturation();
  GenHelper<GenTexturesImmediate>(client_texture_id_);
  EXPECT_CALL(*gl_, GenBuffersARB(_, _))
      .WillOnce(SetArgumentPointee<1>(kServiceElementBufferId))
      .RetiresOnSaturation();
  GenHelper<GenBuffersImmediate>(client_element_buffer_id_);

  DoCreateProgram(client_program_id_, kServiceProgramId);
  DoCreateShader(GL_VERTEX_SHADER, client_shader_id_, kServiceShaderId);

  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

void GLES2DecoderTestBase::TearDown() {
  // All Tests should have read all their GLErrors before getting here.
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_CALL(*gl_, DeleteBuffersARB(1, _))
      .Times(2)
      .RetiresOnSaturation();
  decoder_->Destroy();
  decoder_.reset();
  group_->set_have_context(false);
  group_ = NULL;
  engine_.reset();
  ::gfx::GLInterface::SetGLInterface(NULL);
  gl_.reset();
}

GLint GLES2DecoderTestBase::GetGLError() {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  GetError cmd;
  cmd.Init(shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  return static_cast<GLint>(*GetSharedMemoryAs<GLenum*>());
}

void GLES2DecoderTestBase::DoCreateShader(
    GLenum shader_type, GLuint client_id, GLuint service_id) {
  EXPECT_CALL(*gl_, CreateShader(shader_type))
      .Times(1)
      .WillOnce(Return(service_id))
      .RetiresOnSaturation();
  CreateShader cmd;
  cmd.Init(shader_type, client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

bool GLES2DecoderTestBase::DoIsShader(GLuint client_id) {
  return IsObjectHelper<IsShader, IsShader::Result>(client_id);
}

void GLES2DecoderTestBase::DoDeleteShader(
    GLuint client_id, GLuint service_id) {
  EXPECT_CALL(*gl_, DeleteShader(service_id))
      .Times(1)
      .RetiresOnSaturation();
  DeleteShader cmd;
  cmd.Init(client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoCreateProgram(
    GLuint client_id, GLuint service_id) {
  EXPECT_CALL(*gl_, CreateProgram())
      .Times(1)
      .WillOnce(Return(service_id))
      .RetiresOnSaturation();
  CreateProgram cmd;
  cmd.Init(client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

bool GLES2DecoderTestBase::DoIsProgram(GLuint client_id) {
  return IsObjectHelper<IsProgram, IsProgram::Result>(client_id);
}

void GLES2DecoderTestBase::DoDeleteProgram(
    GLuint client_id, GLuint service_id) {
  EXPECT_CALL(*gl_, DeleteProgram(service_id))
      .Times(1)
      .RetiresOnSaturation();
  DeleteProgram cmd;
  cmd.Init(client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::SetBucketAsCString(
    uint32 bucket_id, const char* str) {
  uint32 size = str ? (strlen(str) + 1) : 0;
  cmd::SetBucketSize cmd1;
  cmd1.Init(bucket_id, size);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd1));
  if (str) {
    memcpy(shared_memory_address_, str, size);
    cmd::SetBucketData cmd2;
    cmd2.Init(bucket_id, 0, size, kSharedMemoryId, kSharedMemoryOffset);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
    ClearSharedMemory();
  }
}

void GLES2DecoderTestBase::SetupExpectationsForFramebufferAttachment(
    GLuint clear_bits,
    GLclampf restore_red,
    GLclampf restore_green,
    GLclampf restore_blue,
    GLclampf restore_alpha,
    GLuint restore_color_mask,
    GLuint restore_stencil,
    GLuint restore_stencil_front_mask,
    GLuint restore_stencil_back_mask,
    GLclampf restore_depth,
    GLboolean restore_depth_mask,
    bool restore_scissor_test) {
  InSequence sequence;
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_FRAMEBUFFER))
      .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
      .RetiresOnSaturation();
  if ((clear_bits & GL_COLOR_BUFFER_BIT) != 0) {
    EXPECT_CALL(*gl_, ClearColor(0, 0, 0, 0))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, ColorMask(1, 1, 1, 1))
        .Times(1)
        .RetiresOnSaturation();
  }
  if ((clear_bits & GL_STENCIL_BUFFER_BIT) != 0) {
    EXPECT_CALL(*gl_, ClearStencil(0))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, StencilMask(static_cast<GLuint>(-1)))
        .Times(1)
        .RetiresOnSaturation();
  }
  if ((clear_bits & GL_DEPTH_BUFFER_BIT) != 0) {
    EXPECT_CALL(*gl_, ClearDepth(1.0f))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, DepthMask(1))
        .Times(1)
        .RetiresOnSaturation();
  }
  EXPECT_CALL(*gl_, Disable(GL_SCISSOR_TEST))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, Clear(clear_bits))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, ClearColor(
      restore_red, restore_green, restore_blue, restore_alpha))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, ColorMask(
      ((restore_color_mask & 0x1000) != 0) ? 1 : 0,
      ((restore_color_mask & 0x0100) != 0) ? 1 : 0,
      ((restore_color_mask & 0x0010) != 0) ? 1 : 0,
      ((restore_color_mask & 0x0001) != 0) ? 1 : 0))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, ClearStencil(restore_stencil))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, StencilMaskSeparate(GL_FRONT, restore_stencil_front_mask))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, StencilMaskSeparate(GL_BACK, restore_stencil_back_mask))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, ClearDepth(restore_depth))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, DepthMask(restore_depth_mask))
      .Times(1)
      .RetiresOnSaturation();
  if (restore_scissor_test) {
    EXPECT_CALL(*gl_, Enable(GL_SCISSOR_TEST))
        .Times(1)
        .RetiresOnSaturation();
  }
}

void GLES2DecoderTestBase::SetupShaderForUniform() {
  static AttribInfo attribs[] = {
    { "foo", 1, GL_FLOAT, 1, },
  };
  static UniformInfo uniforms[] = {
    { "bar", 1, GL_INT, 1, },
  };
  const GLuint kClientVertexShaderId = 5001;
  const GLuint kServiceVertexShaderId = 6001;
  const GLuint kClientFragmentShaderId = 5002;
  const GLuint kServiceFragmentShaderId = 6002;
  SetupShader(attribs, arraysize(attribs), uniforms, arraysize(uniforms),
              client_program_id_, kServiceProgramId,
              kClientVertexShaderId, kServiceVertexShaderId,
              kClientFragmentShaderId, kServiceFragmentShaderId);

  EXPECT_CALL(*gl_, UseProgram(kServiceProgramId))
      .Times(1)
      .RetiresOnSaturation();
  UseProgram cmd;
  cmd.Init(client_program_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoBindBuffer(
    GLenum target, GLuint client_id, GLuint service_id) {
  EXPECT_CALL(*gl_, BindBuffer(target, service_id))
      .Times(1)
      .RetiresOnSaturation();
  BindBuffer cmd;
  cmd.Init(target, client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

bool GLES2DecoderTestBase::DoIsBuffer(GLuint client_id) {
  return IsObjectHelper<IsBuffer, IsBuffer::Result>(client_id);
}

void GLES2DecoderTestBase::DoDeleteBuffer(
    GLuint client_id, GLuint service_id) {
  EXPECT_CALL(*gl_, DeleteBuffersARB(1, Pointee(service_id)))
      .Times(1)
      .RetiresOnSaturation();
  DeleteBuffers cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  memcpy(shared_memory_address_, &client_id, sizeof(client_id));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoBindFramebuffer(
    GLenum target, GLuint client_id, GLuint service_id) {
  EXPECT_CALL(*gl_, BindFramebufferEXT(target, service_id))
      .Times(1)
      .RetiresOnSaturation();
  BindFramebuffer cmd;
  cmd.Init(target, client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

bool GLES2DecoderTestBase::DoIsFramebuffer(GLuint client_id) {
  return IsObjectHelper<IsFramebuffer, IsFramebuffer::Result>(client_id);
}

void GLES2DecoderTestBase::DoDeleteFramebuffer(
    GLuint client_id, GLuint service_id) {
  EXPECT_CALL(*gl_, DeleteFramebuffersEXT(1, Pointee(service_id)))
      .Times(1)
      .RetiresOnSaturation();
  DeleteFramebuffers cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  memcpy(shared_memory_address_, &client_id, sizeof(client_id));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoBindRenderbuffer(
    GLenum target, GLuint client_id, GLuint service_id) {
  EXPECT_CALL(*gl_, BindRenderbufferEXT(target, service_id))
      .Times(1)
      .RetiresOnSaturation();
  BindRenderbuffer cmd;
  cmd.Init(target, client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

bool GLES2DecoderTestBase::DoIsRenderbuffer(GLuint client_id) {
  return IsObjectHelper<IsRenderbuffer, IsRenderbuffer::Result>(client_id);
}

void GLES2DecoderTestBase::DoDeleteRenderbuffer(
    GLuint client_id, GLuint service_id) {
  EXPECT_CALL(*gl_, DeleteRenderbuffersEXT(1, Pointee(service_id)))
      .Times(1)
      .RetiresOnSaturation();
  DeleteRenderbuffers cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  memcpy(shared_memory_address_, &client_id, sizeof(client_id));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoBindTexture(
    GLenum target, GLuint client_id, GLuint service_id) {
  EXPECT_CALL(*gl_, BindTexture(target, service_id))
      .Times(1)
      .RetiresOnSaturation();
  BindTexture cmd;
  cmd.Init(target, client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

bool GLES2DecoderTestBase::DoIsTexture(GLuint client_id) {
  return IsObjectHelper<IsTexture, IsTexture::Result>(client_id);
}

void GLES2DecoderTestBase::DoDeleteTexture(
    GLuint client_id, GLuint service_id) {
  EXPECT_CALL(*gl_, DeleteTextures(1, Pointee(service_id)))
      .Times(1)
      .RetiresOnSaturation();
  DeleteTextures cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  memcpy(shared_memory_address_, &client_id, sizeof(client_id));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoTexImage2D(
    GLenum target, GLint level, GLenum internal_format,
    GLsizei width, GLsizei height, GLint border,
    GLenum format, GLenum type,
    uint32 shared_memory_id, uint32 shared_memory_offset) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, TexImage2D(target, level, internal_format,
                               width, height, border, format, type, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  TexImage2D cmd;
  cmd.Init(target, level, internal_format, width, height, border, format,
           type, shared_memory_id, shared_memory_offset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoVertexAttribPointer(
    GLuint index, GLint size, GLenum type, GLsizei stride, GLuint offset) {
  EXPECT_CALL(*gl_,
              VertexAttribPointer(index, size, type, GL_FALSE, stride,
                                  BufferOffset(offset)))
      .Times(1)
      .RetiresOnSaturation();
  VertexAttribPointer cmd;
  cmd.Init(index, size, GL_FLOAT, GL_FALSE, stride, offset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const GLint GLES2DecoderTestBase::kMaxTextureSize;
const GLint GLES2DecoderTestBase::kMaxCubeMapTextureSize;
const GLint GLES2DecoderTestBase::kNumVertexAttribs;
const GLint GLES2DecoderTestBase::kNumTextureUnits;
const GLint GLES2DecoderTestBase::kMaxTextureImageUnits;
const GLint GLES2DecoderTestBase::kMaxVertexTextureImageUnits;
const GLint GLES2DecoderTestBase::kMaxFragmentUniformVectors;
const GLint GLES2DecoderTestBase::kMaxVaryingVectors;
const GLint GLES2DecoderTestBase::kMaxVertexUniformVectors;

const GLuint GLES2DecoderTestBase::kServiceAttrib0BufferId;
const GLuint GLES2DecoderTestBase::kServiceFixedAttribBufferId;

const GLuint GLES2DecoderTestBase::kServiceBufferId;
const GLuint GLES2DecoderTestBase::kServiceFramebufferId;
const GLuint GLES2DecoderTestBase::kServiceRenderbufferId;
const GLuint GLES2DecoderTestBase::kServiceTextureId;
const GLuint GLES2DecoderTestBase::kServiceProgramId;
const GLuint GLES2DecoderTestBase::kServiceShaderId;
const GLuint GLES2DecoderTestBase::kServiceElementBufferId;

const int32 GLES2DecoderTestBase::kSharedMemoryId;
const size_t GLES2DecoderTestBase::kSharedBufferSize;
const uint32 GLES2DecoderTestBase::kSharedMemoryOffset;
const int32 GLES2DecoderTestBase::kInvalidSharedMemoryId;
const uint32 GLES2DecoderTestBase::kInvalidSharedMemoryOffset;
const uint32 GLES2DecoderTestBase::kInitialResult;
const uint8 GLES2DecoderTestBase::kInitialMemoryValue;

const uint32 GLES2DecoderTestBase::kNewClientId;
const uint32 GLES2DecoderTestBase::kNewServiceId;
const uint32 GLES2DecoderTestBase::kInvalidClientId;

const int GLES2DecoderTestBase::kBackBufferWidth;
const int GLES2DecoderTestBase::kBackBufferHeight;
#endif

void GLES2DecoderWithShaderTestBase::SetUp() {
  GLES2DecoderTestBase::SetUp();

  {
    static AttribInfo attribs[] = {
      { kAttrib1Name, kAttrib1Size, kAttrib1Type, kAttrib1Location, },
      { kAttrib2Name, kAttrib2Size, kAttrib2Type, kAttrib2Location, },
      { kAttrib3Name, kAttrib3Size, kAttrib3Type, kAttrib3Location, },
    };
    static UniformInfo uniforms[] = {
      { kUniform1Name, kUniform1Size, kUniform1Type, kUniform1Location, },
      { kUniform2Name, kUniform2Size, kUniform2Type, kUniform2Location, },
      { kUniform3Name, kUniform3Size, kUniform3Type, kUniform3Location, },
    };
    SetupShader(attribs, arraysize(attribs), uniforms, arraysize(uniforms),
                client_program_id_, kServiceProgramId,
                client_vertex_shader_id_, kServiceVertexShaderId,
                client_fragment_shader_id_, kServiceFragmentShaderId);
  }

  {
    EXPECT_CALL(*gl_, UseProgram(kServiceProgramId))
        .Times(1)
        .RetiresOnSaturation();
    UseProgram cmd;
    cmd.Init(client_program_id_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }
}

void GLES2DecoderWithShaderTestBase::TearDown() {
  GLES2DecoderTestBase::TearDown();
}

void GLES2DecoderTestBase::SetupShader(
    GLES2DecoderTestBase::AttribInfo* attribs, size_t num_attribs,
    GLES2DecoderTestBase::UniformInfo* uniforms, size_t num_uniforms,
    GLuint program_client_id, GLuint program_service_id,
    GLuint vertex_shader_client_id, GLuint vertex_shader_service_id,
    GLuint fragment_shader_client_id, GLuint fragment_shader_service_id) {
  {
    InSequence s;

    EXPECT_CALL(*gl_,
                AttachShader(program_service_id, vertex_shader_service_id))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_,
                AttachShader(program_service_id, fragment_shader_service_id))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, LinkProgram(program_service_id))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, GetProgramiv(program_service_id, GL_LINK_STATUS, _))
        .WillOnce(SetArgumentPointee<2>(GL_TRUE))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_,
        GetProgramiv(program_service_id, GL_INFO_LOG_LENGTH, _))
        .WillOnce(SetArgumentPointee<2>(0))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_,
        GetProgramiv(program_service_id, GL_ACTIVE_ATTRIBUTES, _))
        .WillOnce(SetArgumentPointee<2>(num_attribs))
        .RetiresOnSaturation();
    size_t max_attrib_len = 0;
    for (size_t ii = 0; ii < num_attribs; ++ii) {
      size_t len = strlen(attribs[ii].name) + 1;
      max_attrib_len = std::max(max_attrib_len, len);
    }
    EXPECT_CALL(*gl_,
        GetProgramiv(program_service_id, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, _))
        .WillOnce(SetArgumentPointee<2>(max_attrib_len))
        .RetiresOnSaturation();
    for (size_t ii = 0; ii < num_attribs; ++ii) {
      const AttribInfo& info = attribs[ii];
      EXPECT_CALL(*gl_,
          GetActiveAttrib(program_service_id, ii, max_attrib_len, _, _, _, _))
          .WillOnce(DoAll(
              SetArgumentPointee<3>(strlen(info.name)),
              SetArgumentPointee<4>(info.size),
              SetArgumentPointee<5>(info.type),
              SetArrayArgument<6>(info.name,
                                  info.name + strlen(info.name) + 1)))
          .RetiresOnSaturation();
      if (!ProgramManager::IsInvalidPrefix(info.name, strlen(info.name))) {
        EXPECT_CALL(*gl_, GetAttribLocation(program_service_id,
                                            StrEq(info.name)))
            .WillOnce(Return(info.location))
            .RetiresOnSaturation();
      }
    }
    EXPECT_CALL(*gl_,
        GetProgramiv(program_service_id, GL_ACTIVE_UNIFORMS, _))
        .WillOnce(SetArgumentPointee<2>(num_uniforms))
        .RetiresOnSaturation();
    size_t max_uniform_len = 0;
    for (size_t ii = 0; ii < num_uniforms; ++ii) {
      size_t len = strlen(uniforms[ii].name) + 1;
      max_uniform_len = std::max(max_uniform_len, len);
    }
    EXPECT_CALL(*gl_,
        GetProgramiv(program_service_id, GL_ACTIVE_UNIFORM_MAX_LENGTH, _))
        .WillOnce(SetArgumentPointee<2>(max_uniform_len))
        .RetiresOnSaturation();
    for (size_t ii = 0; ii < num_uniforms; ++ii) {
      const UniformInfo& info = uniforms[ii];
      EXPECT_CALL(*gl_,
          GetActiveUniform(program_service_id, ii, max_uniform_len, _, _, _, _))
          .WillOnce(DoAll(
              SetArgumentPointee<3>(strlen(info.name)),
              SetArgumentPointee<4>(info.size),
              SetArgumentPointee<5>(info.type),
              SetArrayArgument<6>(info.name,
                                  info.name + strlen(info.name) + 1)))
          .RetiresOnSaturation();
      if (!ProgramManager::IsInvalidPrefix(info.name, strlen(info.name))) {
        EXPECT_CALL(*gl_, GetUniformLocation(program_service_id,
                                             StrEq(info.name)))
            .WillOnce(Return(info.location))
            .RetiresOnSaturation();
        if (info.size > 1) {
          std::string base_name = info.name;
          size_t array_pos = base_name.rfind("[0]");
          if (base_name.size() > 3 && array_pos == base_name.size() - 3) {
            base_name = base_name.substr(0, base_name.size() - 3);
          }
          for (GLsizei jj = 1; jj < info.size; ++jj) {
            std::string element_name(
                std::string(base_name) + "[" + base::IntToString(jj) + "]");
            EXPECT_CALL(*gl_, GetUniformLocation(program_service_id,
                                                 StrEq(element_name)))
                .WillOnce(Return(info.location + jj * 2))
                .RetiresOnSaturation();
          }
        }
      }
    }
  }

  DoCreateShader(
      GL_VERTEX_SHADER, vertex_shader_client_id, vertex_shader_service_id);
  DoCreateShader(
      GL_FRAGMENT_SHADER, fragment_shader_client_id,
      fragment_shader_service_id);

  GetShaderInfo(vertex_shader_client_id)->SetStatus(true, "", NULL);
  GetShaderInfo(fragment_shader_client_id)->SetStatus(true, "", NULL);

  AttachShader attach_cmd;
  attach_cmd.Init(program_client_id, vertex_shader_client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(attach_cmd));

  attach_cmd.Init(program_client_id, fragment_shader_client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(attach_cmd));

  LinkProgram link_cmd;
  link_cmd.Init(program_client_id);

  EXPECT_EQ(error::kNoError, ExecuteCmd(link_cmd));
}

void GLES2DecoderWithShaderTestBase::DoEnableVertexAttribArray(GLint index) {
  EXPECT_CALL(*gl_, EnableVertexAttribArray(index))
      .Times(1)
      .RetiresOnSaturation();
  EnableVertexAttribArray cmd;
  cmd.Init(index);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderWithShaderTestBase::DoBufferData(GLenum target, GLsizei size) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BufferData(target, size, _, GL_STREAM_DRAW))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  BufferData cmd;
  cmd.Init(target, size, 0, 0, GL_STREAM_DRAW);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderWithShaderTestBase::DoBufferSubData(
    GLenum target, GLint offset, GLsizei size, const void* data) {
  EXPECT_CALL(*gl_, BufferSubData(target, offset, size,
                                  shared_memory_address_))
      .Times(1)
      .RetiresOnSaturation();
  memcpy(shared_memory_address_, data, size);
  BufferSubData cmd;
  cmd.Init(target, offset, size, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderWithShaderTestBase::SetupVertexBuffer() {
  DoEnableVertexAttribArray(1);
  DoBindBuffer(GL_ARRAY_BUFFER, client_buffer_id_, kServiceBufferId);
  GLfloat f = 0;
  DoBufferData(GL_ARRAY_BUFFER, kNumVertices * 2 * sizeof(f));
}

void GLES2DecoderWithShaderTestBase::SetupIndexBuffer() {
  DoBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
               client_element_buffer_id_,
               kServiceElementBufferId);
  static const GLshort indices[] = {100, 1, 2, 3, 4, 5, 6, 7, 100, 9};
  COMPILE_ASSERT(arraysize(indices) == kNumIndices, Indices_is_not_10);
  DoBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices));
  DoBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(indices), indices);
}

void GLES2DecoderWithShaderTestBase::SetupTexture() {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               0, 0);
};

void GLES2DecoderWithShaderTestBase::DeleteVertexBuffer() {
  DoDeleteBuffer(client_buffer_id_, kServiceBufferId);
}

void GLES2DecoderWithShaderTestBase::DeleteIndexBuffer() {
  DoDeleteBuffer(client_element_buffer_id_, kServiceElementBufferId);
}

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const GLuint GLES2DecoderWithShaderTestBase::kServiceVertexShaderId;
const GLuint GLES2DecoderWithShaderTestBase::kServiceFragmentShaderId;

const GLsizei GLES2DecoderWithShaderTestBase::kNumVertices;
const GLsizei GLES2DecoderWithShaderTestBase::kNumIndices;
const int GLES2DecoderWithShaderTestBase::kValidIndexRangeStart;
const int GLES2DecoderWithShaderTestBase::kValidIndexRangeCount;
const int GLES2DecoderWithShaderTestBase::kInvalidIndexRangeStart;
const int GLES2DecoderWithShaderTestBase::kInvalidIndexRangeCount;
const int GLES2DecoderWithShaderTestBase::kOutOfRangeIndexRangeEnd;
const GLuint GLES2DecoderWithShaderTestBase::kMaxValidIndex;

const GLint GLES2DecoderWithShaderTestBase::kMaxAttribLength;
const GLint GLES2DecoderWithShaderTestBase::kAttrib1Size;
const GLint GLES2DecoderWithShaderTestBase::kAttrib2Size;
const GLint GLES2DecoderWithShaderTestBase::kAttrib3Size;
const GLint GLES2DecoderWithShaderTestBase::kAttrib1Location;
const GLint GLES2DecoderWithShaderTestBase::kAttrib2Location;
const GLint GLES2DecoderWithShaderTestBase::kAttrib3Location;
const GLenum GLES2DecoderWithShaderTestBase::kAttrib1Type;
const GLenum GLES2DecoderWithShaderTestBase::kAttrib2Type;
const GLenum GLES2DecoderWithShaderTestBase::kAttrib3Type;
const GLint GLES2DecoderWithShaderTestBase::kInvalidAttribLocation;
const GLint GLES2DecoderWithShaderTestBase::kBadAttribIndex;

const GLint GLES2DecoderWithShaderTestBase::kMaxUniformLength;
const GLint GLES2DecoderWithShaderTestBase::kUniform1Size;
const GLint GLES2DecoderWithShaderTestBase::kUniform2Size;
const GLint GLES2DecoderWithShaderTestBase::kUniform3Size;
const GLint GLES2DecoderWithShaderTestBase::kUniform1Location;
const GLint GLES2DecoderWithShaderTestBase::kUniform2Location;
const GLint GLES2DecoderWithShaderTestBase::kUniform2ElementLocation;
const GLint GLES2DecoderWithShaderTestBase::kUniform3Location;
const GLenum GLES2DecoderWithShaderTestBase::kUniform1Type;
const GLenum GLES2DecoderWithShaderTestBase::kUniform2Type;
const GLenum GLES2DecoderWithShaderTestBase::kUniform3Type;
const GLint GLES2DecoderWithShaderTestBase::kInvalidUniformLocation;
const GLint GLES2DecoderWithShaderTestBase::kBadUniformIndex;
#endif

const char* GLES2DecoderWithShaderTestBase::kAttrib1Name = "attrib1";
const char* GLES2DecoderWithShaderTestBase::kAttrib2Name = "attrib2";
const char* GLES2DecoderWithShaderTestBase::kAttrib3Name = "attrib3";
const char* GLES2DecoderWithShaderTestBase::kUniform1Name = "uniform1";
const char* GLES2DecoderWithShaderTestBase::kUniform2Name = "uniform2[0]";
const char* GLES2DecoderWithShaderTestBase::kUniform3Name = "uniform3[0]";

}  // namespace gles2
}  // namespace gpu
