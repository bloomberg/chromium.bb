// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest_base.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/string_number_conversions.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_mock.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/vertex_attrib_manager.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_mock.h"

using ::gfx::MockGLInterface;
using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::MatcherCast;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArrayArgument;
using ::testing::SetArgPointee;
using ::testing::SetArgumentPointee;
using ::testing::StrEq;
using ::testing::StrictMock;

namespace gpu {
namespace gles2 {

GLES2DecoderTestBase::GLES2DecoderTestBase()
    : surface_(NULL),
      context_(NULL),
      memory_tracker_(NULL),
      client_buffer_id_(100),
      client_framebuffer_id_(101),
      client_program_id_(102),
      client_renderbuffer_id_(103),
      client_shader_id_(104),
      client_texture_id_(106),
      client_element_buffer_id_(107),
      client_vertex_shader_id_(121),
      client_fragment_shader_id_(122),
      client_query_id_(123),
      client_vertexarray_id_(124) {
  memset(immediate_buffer_, 0xEE, sizeof(immediate_buffer_));
}

GLES2DecoderTestBase::~GLES2DecoderTestBase() {}

void GLES2DecoderTestBase::SetUp() {
  InitDecoder(
      "",      // extensions
      true,    // has alpha
      true,    // has depth
      false,   // has stencil
      true,    // request alpha
      true,    // request depth
      false,   // request stencil
      true);   // bind generates resource
}

void GLES2DecoderTestBase::AddExpectationsForVertexAttribManager() {
  for (GLint ii = 0; ii < kNumVertexAttribs; ++ii) {
    EXPECT_CALL(*gl_, VertexAttrib4f(ii, 0.0f, 0.0f, 0.0f, 1.0f))
        .Times(1)
        .RetiresOnSaturation();
  }
}

void GLES2DecoderTestBase::InitDecoder(
    const char* extensions,
    bool has_alpha,
    bool has_depth,
    bool has_stencil,
    bool request_alpha,
    bool request_depth,
    bool request_stencil,
    bool bind_generates_resource) {
  FramebufferManager::FramebufferInfo::ClearFramebufferCompleteComboMap();
  gl_.reset(new StrictMock<MockGLInterface>());
  ::gfx::GLInterface::SetGLInterface(gl_.get());
  group_ = ContextGroup::Ref(new ContextGroup(NULL,
                                              NULL,
                                              memory_tracker_,
                                              bind_generates_resource));

  InSequence sequence;

  TestHelper::SetupContextGroupInitExpectations(gl_.get(),
      DisallowedFeatures(), extensions);

  // We initialize the ContextGroup with a MockGLES2Decoder so that
  // we can use the ContextGroup to figure out how the real GLES2Decoder
  // will initialize itself.
  mock_decoder_.reset(new MockGLES2Decoder());
  EXPECT_TRUE(
      group_->Initialize(mock_decoder_.get(), DisallowedFeatures(), NULL));

  AddExpectationsForVertexAttribManager();

  AddExpectationsForBindVertexArrayOES();

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
    if (group_->feature_info()->feature_flags().oes_egl_image_external) {
      EXPECT_CALL(*gl_, BindTexture(
              GL_TEXTURE_EXTERNAL_OES,
              TestHelper::kServiceDefaultExternalTextureId))
          .Times(1)
          .RetiresOnSaturation();
    }
    if (group_->feature_info()->feature_flags().arb_texture_rectangle) {
      EXPECT_CALL(*gl_, BindTexture(
              GL_TEXTURE_RECTANGLE_ARB,
              TestHelper::kServiceDefaultRectangleTextureId))
          .Times(1)
          .RetiresOnSaturation();
    }
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

  EXPECT_CALL(*gl_, GetIntegerv(GL_ALPHA_BITS, _))
       .WillOnce(SetArgumentPointee<1>(has_alpha ? 8 : 0))
       .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetIntegerv(GL_DEPTH_BITS, _))
       .WillOnce(SetArgumentPointee<1>(has_depth ? 24 : 0))
       .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetIntegerv(GL_STENCIL_BITS, _))
       .WillOnce(SetArgumentPointee<1>(has_stencil ? 8 : 0))
       .RetiresOnSaturation();

  EXPECT_CALL(*gl_, Clear(
      GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT))
      .Times(1)
      .RetiresOnSaturation();

  EXPECT_CALL(*gl_, Enable(GL_VERTEX_PROGRAM_POINT_SIZE))
      .Times(1)
      .RetiresOnSaturation();

  EXPECT_CALL(*gl_, Enable(GL_POINT_SPRITE))
      .Times(1)
      .RetiresOnSaturation();

  static GLint max_viewport_dims[] = {
    kMaxViewportWidth,
    kMaxViewportHeight
  };
  EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_VIEWPORT_DIMS, _))
      .WillOnce(SetArrayArgument<1>(
          max_viewport_dims, max_viewport_dims + arraysize(max_viewport_dims)))
      .RetiresOnSaturation();

  SetupInitCapabilitiesExpectations();
  SetupInitStateExpectations();

  EXPECT_CALL(*gl_, ActiveTexture(GL_TEXTURE0))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, Hint(GL_GENERATE_MIPMAP_HINT, GL_DONT_CARE))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, PixelStorei(GL_PACK_ALIGNMENT, 4))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, PixelStorei(GL_UNPACK_ALIGNMENT, 4))
      .Times(1)
      .RetiresOnSaturation();

  EXPECT_CALL(*gl_, BindBuffer(GL_ARRAY_BUFFER, 0))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BindFramebufferEXT(GL_FRAMEBUFFER, 0))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BindRenderbufferEXT(GL_RENDERBUFFER, 0))
      .Times(1)
      .RetiresOnSaturation();

  engine_.reset(new StrictMock<MockCommandBufferEngine>());
  Buffer buffer = engine_->GetSharedMemoryBuffer(kSharedMemoryId);
  shared_memory_offset_ = kSharedMemoryOffset;
  shared_memory_address_ = reinterpret_cast<int8*>(buffer.ptr) +
      shared_memory_offset_;
  shared_memory_id_ = kSharedMemoryId;
  shared_memory_base_ = buffer.ptr;

  surface_ = new gfx::GLSurfaceStub;
  surface_->SetSize(gfx::Size(kBackBufferWidth, kBackBufferHeight));

  context_ = new gfx::GLContextStub;

  context_->MakeCurrent(surface_);

  int32 attributes[] = {
    EGL_ALPHA_SIZE, request_alpha ? 8 : 0,
    EGL_DEPTH_SIZE, request_depth ? 24 : 0,
    EGL_STENCIL_SIZE, request_stencil ? 8 : 0,
  };
  std::vector<int32> attribs(attributes, attributes + arraysize(attributes));

  decoder_.reset(GLES2Decoder::Create(group_.get()));
  decoder_->set_log_synthesized_gl_errors(false);
  decoder_->Initialize(
      surface_, context_, false, surface_->GetSize(), DisallowedFeatures(),
      NULL, attribs);
  decoder_->MakeCurrent();
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

  decoder_->Destroy(true);
  decoder_.reset();
  group_->Destroy(mock_decoder_.get(), false);
  engine_.reset();
  ::gfx::GLInterface::SetGLInterface(NULL);
  gl_.reset();
}

void GLES2DecoderTestBase::ExpectEnableDisable(GLenum cap, bool enable) {
  if (enable) {
    EXPECT_CALL(*gl_, Enable(cap))
        .Times(1)
        .RetiresOnSaturation();
  } else {
    EXPECT_CALL(*gl_, Disable(cap))
        .Times(1)
        .RetiresOnSaturation();
  }
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
    GLuint client_id, GLuint /* service_id */) {
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

void GLES2DecoderTestBase::SetupClearTextureExpections(
      GLuint service_id,
      GLuint old_service_id,
      GLenum bind_target,
      GLenum target,
      GLint level,
      GLenum format,
      GLenum type,
      GLsizei width,
      GLsizei height) {
  EXPECT_CALL(*gl_, BindTexture(bind_target, service_id))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, TexImage2D(
      target, level, format, width, height, 0, format, type, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BindTexture(bind_target, old_service_id))
      .Times(1)
      .RetiresOnSaturation();
}

void GLES2DecoderTestBase::SetupExpectationsForFramebufferClearing(
    GLenum target,
    GLuint clear_bits,
    GLclampf restore_red,
    GLclampf restore_green,
    GLclampf restore_blue,
    GLclampf restore_alpha,
    GLuint restore_stencil,
    GLclampf restore_depth,
    bool restore_scissor_test) {
  SetupExpectationsForFramebufferClearingMulti(
      0,
      0,
      target,
      clear_bits,
      restore_red,
      restore_green,
      restore_blue,
      restore_alpha,
      restore_stencil,
      restore_depth,
      restore_scissor_test);
}

void GLES2DecoderTestBase::SetupExpectationsForRestoreClearState(
    GLclampf restore_red,
    GLclampf restore_green,
    GLclampf restore_blue,
    GLclampf restore_alpha,
    GLuint restore_stencil,
    GLclampf restore_depth,
    bool restore_scissor_test) {
  EXPECT_CALL(*gl_, ClearColor(
      restore_red, restore_green, restore_blue, restore_alpha))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, ClearStencil(restore_stencil))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, ClearDepth(restore_depth))
      .Times(1)
      .RetiresOnSaturation();
  if (restore_scissor_test) {
    EXPECT_CALL(*gl_, Enable(GL_SCISSOR_TEST))
        .Times(1)
        .RetiresOnSaturation();
  }
}

void GLES2DecoderTestBase::SetupExpectationsForFramebufferClearingMulti(
    GLuint read_framebuffer_service_id,
    GLuint draw_framebuffer_service_id,
    GLenum target,
    GLuint clear_bits,
    GLclampf restore_red,
    GLclampf restore_green,
    GLclampf restore_blue,
    GLclampf restore_alpha,
    GLuint restore_stencil,
    GLclampf restore_depth,
    bool restore_scissor_test) {
  // TODO(gman): Figure out why InSequence stopped working.
  // InSequence sequence;
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(target))
      .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
      .RetiresOnSaturation();
  if (target == GL_READ_FRAMEBUFFER_EXT) {
    EXPECT_CALL(*gl_, BindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, 0))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, BindFramebufferEXT(
        GL_DRAW_FRAMEBUFFER_EXT, read_framebuffer_service_id))
        .Times(1)
        .RetiresOnSaturation();
  }
  if ((clear_bits & GL_COLOR_BUFFER_BIT) != 0) {
    EXPECT_CALL(*gl_, ClearColor(0.0f, 0.0f, 0.0f, 0.0f))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, ColorMask(true, true, true, true))
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
  SetupExpectationsForRestoreClearState(
      restore_red, restore_green, restore_blue, restore_alpha,
      restore_stencil, restore_depth, restore_scissor_test);
  if (target == GL_READ_FRAMEBUFFER_EXT) {
    EXPECT_CALL(*gl_, BindFramebufferEXT(
        GL_READ_FRAMEBUFFER_EXT, read_framebuffer_service_id))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, BindFramebufferEXT(
        GL_DRAW_FRAMEBUFFER_EXT, draw_framebuffer_service_id))
        .Times(1)
        .RetiresOnSaturation();
  }
}

void GLES2DecoderTestBase::SetupShaderForUniform(GLenum uniform_type) {
  static AttribInfo attribs[] = {
    { "foo", 1, GL_FLOAT, 1, },
    { "goo", 1, GL_FLOAT, 2, },
  };
  UniformInfo uniforms[] = {
    { "bar", 1, uniform_type, 0, 2, -1, },
    { "car", 4, uniform_type, 1, 1, -1, },
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

void GLES2DecoderTestBase::SetupExpectationsForApplyingDirtyState(
    bool framebuffer_is_rgb,
    bool framebuffer_has_depth,
    bool framebuffer_has_stencil,
    GLuint color_bits,
    bool depth_mask,
    bool depth_enabled,
    GLuint front_stencil_mask,
    GLuint back_stencil_mask,
    bool stencil_enabled,
    bool cull_face_enabled,
    bool scissor_test_enabled,
    bool blend_enabled) {
  EXPECT_CALL(*gl_, ColorMask(
      (color_bits & 0x1000) != 0,
      (color_bits & 0x0100) != 0,
      (color_bits & 0x0010) != 0,
      (color_bits & 0x0001) && !framebuffer_is_rgb))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, DepthMask(depth_mask))
      .Times(1)
      .RetiresOnSaturation();
  if (framebuffer_has_depth && depth_enabled) {
    EXPECT_CALL(*gl_, Enable(GL_DEPTH_TEST))
        .Times(1)
        .RetiresOnSaturation();
  } else {
    EXPECT_CALL(*gl_, Disable(GL_DEPTH_TEST))
        .Times(1)
        .RetiresOnSaturation();
  }
  EXPECT_CALL(*gl_, StencilMaskSeparate(GL_FRONT, front_stencil_mask))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, StencilMaskSeparate(GL_BACK, back_stencil_mask))
      .Times(1)
      .RetiresOnSaturation();
  if (framebuffer_has_stencil && stencil_enabled) {
    EXPECT_CALL(*gl_, Enable(GL_STENCIL_TEST))
        .Times(1)
        .RetiresOnSaturation();
  } else {
    EXPECT_CALL(*gl_, Disable(GL_STENCIL_TEST))
        .Times(1)
        .RetiresOnSaturation();
  }
  if (cull_face_enabled) {
    EXPECT_CALL(*gl_, Enable(GL_CULL_FACE))
        .Times(1)
        .RetiresOnSaturation();
  } else {
    EXPECT_CALL(*gl_, Disable(GL_CULL_FACE))
        .Times(1)
        .RetiresOnSaturation();
  }
  if (scissor_test_enabled) {
    EXPECT_CALL(*gl_, Enable(GL_SCISSOR_TEST))
        .Times(1)
        .RetiresOnSaturation();
  } else {
    EXPECT_CALL(*gl_, Disable(GL_SCISSOR_TEST))
        .Times(1)
        .RetiresOnSaturation();
  }
  if (blend_enabled) {
    EXPECT_CALL(*gl_, Enable(GL_BLEND))
        .Times(1)
        .RetiresOnSaturation();
  } else {
    EXPECT_CALL(*gl_, Disable(GL_BLEND))
        .Times(1)
        .RetiresOnSaturation();
  }
}

void GLES2DecoderTestBase::SetupExpectationsForApplyingDefaultDirtyState() {
  SetupExpectationsForApplyingDirtyState(
      false,   // Framebuffer is RGB
      false,   // Framebuffer has depth
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
    GLuint client_id, GLuint service_id,
    bool reset_draw, GLenum draw_target, GLuint draw_id,
    bool reset_read, GLenum read_target, GLuint read_id) {
  if (reset_draw) {
    EXPECT_CALL(*gl_, BindFramebufferEXT(draw_target, draw_id))
        .Times(1)
        .RetiresOnSaturation();
  }
  if (reset_read) {
    EXPECT_CALL(*gl_, BindFramebufferEXT(read_target, read_id))
        .Times(1)
        .RetiresOnSaturation();
  }
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

void GLES2DecoderTestBase::DoCompressedTexImage2D(
    GLenum target, GLint level, GLenum format,
    GLsizei width, GLsizei height, GLint border,
    GLsizei size, uint32 bucket_id) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, CompressedTexImage2D(
      target, level, format, width, height, border, size, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  CommonDecoder::Bucket* bucket = decoder_->CreateBucket(bucket_id);
  bucket->SetSize(size);
  CompressedTexImage2DBucket cmd;
  cmd.Init(
      target, level, format, width, height, border,
      bucket_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoTexImage2DSameSize(
    GLenum target, GLint level, GLenum internal_format,
    GLsizei width, GLsizei height, GLint border,
    GLenum format, GLenum type,
    uint32 shared_memory_id, uint32 shared_memory_offset) {
  if (GLES2Decoder::IsAngle()) {
    EXPECT_CALL(*gl_, TexSubImage2D(
        target, level, 0, 0, width, height, format, type, _))
        .Times(1)
        .RetiresOnSaturation();
  } else {
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
  }
  TexImage2D cmd;
  cmd.Init(target, level, internal_format, width, height, border, format,
           type, shared_memory_id, shared_memory_offset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoRenderbufferStorage(
    GLenum target, GLenum internal_format, GLenum actual_format,
    GLsizei width, GLsizei height,  GLenum error) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, RenderbufferStorageEXT(
      target, actual_format, width, height))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(error))
      .RetiresOnSaturation();
  RenderbufferStorage cmd;
  cmd.Init(target, internal_format, width, height);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoFramebufferTexture2D(
    GLenum target, GLenum attachment, GLenum textarget,
    GLuint texture_client_id, GLuint texture_service_id, GLint level,
    GLenum error) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, FramebufferTexture2DEXT(
      target, attachment, textarget, texture_service_id, level))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(error))
      .RetiresOnSaturation();
  FramebufferTexture2D cmd;
  cmd.Init(target, attachment, textarget, texture_client_id, level);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoFramebufferRenderbuffer(
    GLenum target,
    GLenum attachment,
    GLenum renderbuffer_target,
    GLuint renderbuffer_client_id,
    GLuint renderbuffer_service_id,
    GLenum error) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, FramebufferRenderbufferEXT(
      target, attachment, renderbuffer_target, renderbuffer_service_id))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(error))
      .RetiresOnSaturation();
  FramebufferRenderbuffer cmd;
  cmd.Init(target, attachment, renderbuffer_target, renderbuffer_client_id);
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

void GLES2DecoderTestBase::DoVertexAttribDivisorANGLE(
    GLuint index, GLuint divisor) {
  EXPECT_CALL(*gl_,
              VertexAttribDivisorANGLE(index, divisor))
      .Times(1)
      .RetiresOnSaturation();
  VertexAttribDivisorANGLE cmd;
  cmd.Init(index, divisor);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::AddExpectationsForGenVertexArraysOES(){
  if (group_->feature_info()->feature_flags().native_vertex_array_object) {
      EXPECT_CALL(*gl_, GenVertexArraysOES(1, _))
          .WillOnce(SetArgumentPointee<1>(kServiceVertexArrayId))
          .RetiresOnSaturation();
  }
}

void GLES2DecoderTestBase::AddExpectationsForDeleteVertexArraysOES(){
  if (group_->feature_info()->feature_flags().native_vertex_array_object) {
      EXPECT_CALL(*gl_, DeleteVertexArraysOES(1, _))
          .Times(1)
          .RetiresOnSaturation();
  }
}

void GLES2DecoderTestBase::AddExpectationsForBindVertexArrayOES() {
  if (group_->feature_info()->feature_flags().native_vertex_array_object) {
    EXPECT_CALL(*gl_, BindVertexArrayOES(_))
      .Times(1)
      .RetiresOnSaturation();
  } else {
    for (uint32 vv = 0; vv < group_->max_vertex_attribs(); ++vv) {
      AddExpectationsForRestoreAttribState(vv);
    }

    EXPECT_CALL(*gl_, BindBuffer(GL_ELEMENT_ARRAY_BUFFER, _))
      .Times(1)
      .RetiresOnSaturation();
  }
}

void GLES2DecoderTestBase::AddExpectationsForRestoreAttribState(GLuint attrib) {
  EXPECT_CALL(*gl_, BindBuffer(GL_ARRAY_BUFFER, _))
      .Times(1)
      .RetiresOnSaturation();

  EXPECT_CALL(*gl_, VertexAttribPointer(attrib, _, _, _, _, _))
      .Times(1)
      .RetiresOnSaturation();

  EXPECT_CALL(*gl_, VertexAttribDivisorANGLE(attrib, _))
        .Times(testing::AtMost(1))
        .RetiresOnSaturation();

  EXPECT_CALL(*gl_, BindBuffer(GL_ARRAY_BUFFER, _))
      .Times(1)
      .RetiresOnSaturation();

  if (attrib != 0 ||
      gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2) {

      // TODO(bajones): Not sure if I can tell which of these will be called
      EXPECT_CALL(*gl_, EnableVertexAttribArray(attrib))
          .Times(testing::AtMost(1))
          .RetiresOnSaturation();

      EXPECT_CALL(*gl_, DisableVertexAttribArray(attrib))
          .Times(testing::AtMost(1))
          .RetiresOnSaturation();
  }
}

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const int GLES2DecoderTestBase::kBackBufferWidth;
const int GLES2DecoderTestBase::kBackBufferHeight;

const GLint GLES2DecoderTestBase::kMaxTextureSize;
const GLint GLES2DecoderTestBase::kMaxCubeMapTextureSize;
const GLint GLES2DecoderTestBase::kNumVertexAttribs;
const GLint GLES2DecoderTestBase::kNumTextureUnits;
const GLint GLES2DecoderTestBase::kMaxTextureImageUnits;
const GLint GLES2DecoderTestBase::kMaxVertexTextureImageUnits;
const GLint GLES2DecoderTestBase::kMaxFragmentUniformVectors;
const GLint GLES2DecoderTestBase::kMaxVaryingVectors;
const GLint GLES2DecoderTestBase::kMaxVertexUniformVectors;
const GLint GLES2DecoderTestBase::kMaxViewportWidth;
const GLint GLES2DecoderTestBase::kMaxViewportHeight;

const GLint GLES2DecoderTestBase::kViewportX;
const GLint GLES2DecoderTestBase::kViewportY;
const GLint GLES2DecoderTestBase::kViewportWidth;
const GLint GLES2DecoderTestBase::kViewportHeight;

const GLuint GLES2DecoderTestBase::kServiceAttrib0BufferId;
const GLuint GLES2DecoderTestBase::kServiceFixedAttribBufferId;

const GLuint GLES2DecoderTestBase::kServiceBufferId;
const GLuint GLES2DecoderTestBase::kServiceFramebufferId;
const GLuint GLES2DecoderTestBase::kServiceRenderbufferId;
const GLuint GLES2DecoderTestBase::kServiceTextureId;
const GLuint GLES2DecoderTestBase::kServiceProgramId;
const GLuint GLES2DecoderTestBase::kServiceShaderId;
const GLuint GLES2DecoderTestBase::kServiceElementBufferId;
const GLuint GLES2DecoderTestBase::kServiceQueryId;
const GLuint GLES2DecoderTestBase::kServiceVertexArrayId;

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

const GLuint GLES2DecoderTestBase::kServiceVertexShaderId;
const GLuint GLES2DecoderTestBase::kServiceFragmentShaderId;

const GLuint GLES2DecoderTestBase::kServiceCopyTextureChromiumShaderId;
const GLuint GLES2DecoderTestBase::kServiceCopyTextureChromiumProgramId;

const GLuint GLES2DecoderTestBase::kServiceCopyTextureChromiumTextureBufferId;
const GLuint GLES2DecoderTestBase::kServiceCopyTextureChromiumVertexBufferId;
const GLuint GLES2DecoderTestBase::kServiceCopyTextureChromiumFBOId;
const GLuint GLES2DecoderTestBase::kServiceCopyTextureChromiumPositionAttrib;
const GLuint GLES2DecoderTestBase::kServiceCopyTextureChromiumTexAttrib;
const GLuint GLES2DecoderTestBase::kServiceCopyTextureChromiumSamplerLocation;

const GLsizei GLES2DecoderTestBase::kNumVertices;
const GLsizei GLES2DecoderTestBase::kNumIndices;
const int GLES2DecoderTestBase::kValidIndexRangeStart;
const int GLES2DecoderTestBase::kValidIndexRangeCount;
const int GLES2DecoderTestBase::kInvalidIndexRangeStart;
const int GLES2DecoderTestBase::kInvalidIndexRangeCount;
const int GLES2DecoderTestBase::kOutOfRangeIndexRangeEnd;
const GLuint GLES2DecoderTestBase::kMaxValidIndex;

const GLint GLES2DecoderTestBase::kMaxAttribLength;
const GLint GLES2DecoderTestBase::kAttrib1Size;
const GLint GLES2DecoderTestBase::kAttrib2Size;
const GLint GLES2DecoderTestBase::kAttrib3Size;
const GLint GLES2DecoderTestBase::kAttrib1Location;
const GLint GLES2DecoderTestBase::kAttrib2Location;
const GLint GLES2DecoderTestBase::kAttrib3Location;
const GLenum GLES2DecoderTestBase::kAttrib1Type;
const GLenum GLES2DecoderTestBase::kAttrib2Type;
const GLenum GLES2DecoderTestBase::kAttrib3Type;
const GLint GLES2DecoderTestBase::kInvalidAttribLocation;
const GLint GLES2DecoderTestBase::kBadAttribIndex;

const GLint GLES2DecoderTestBase::kMaxUniformLength;
const GLint GLES2DecoderTestBase::kUniform1Size;
const GLint GLES2DecoderTestBase::kUniform2Size;
const GLint GLES2DecoderTestBase::kUniform3Size;
const GLint GLES2DecoderTestBase::kUniform1RealLocation;
const GLint GLES2DecoderTestBase::kUniform2RealLocation;
const GLint GLES2DecoderTestBase::kUniform2ElementRealLocation;
const GLint GLES2DecoderTestBase::kUniform3RealLocation;
const GLint GLES2DecoderTestBase::kUniform1FakeLocation;
const GLint GLES2DecoderTestBase::kUniform2FakeLocation;
const GLint GLES2DecoderTestBase::kUniform2ElementFakeLocation;
const GLint GLES2DecoderTestBase::kUniform3FakeLocation;
const GLint GLES2DecoderTestBase::kUniform1DesiredLocation;
const GLint GLES2DecoderTestBase::kUniform2DesiredLocation;
const GLint GLES2DecoderTestBase::kUniform3DesiredLocation;
const GLenum GLES2DecoderTestBase::kUniform1Type;
const GLenum GLES2DecoderTestBase::kUniform2Type;
const GLenum GLES2DecoderTestBase::kUniform3Type;
const GLenum GLES2DecoderTestBase::kUniformCubemapType;
const GLint GLES2DecoderTestBase::kInvalidUniformLocation;
const GLint GLES2DecoderTestBase::kBadUniformIndex;

#endif

const char* GLES2DecoderTestBase::kAttrib1Name = "attrib1";
const char* GLES2DecoderTestBase::kAttrib2Name = "attrib2";
const char* GLES2DecoderTestBase::kAttrib3Name = "attrib3";
const char* GLES2DecoderTestBase::kUniform1Name = "uniform1";
const char* GLES2DecoderTestBase::kUniform2Name = "uniform2[0]";
const char* GLES2DecoderTestBase::kUniform3Name = "uniform3[0]";

void GLES2DecoderTestBase::SetupDefaultProgram() {
  {
    static AttribInfo attribs[] = {
      { kAttrib1Name, kAttrib1Size, kAttrib1Type, kAttrib1Location, },
      { kAttrib2Name, kAttrib2Size, kAttrib2Type, kAttrib2Location, },
      { kAttrib3Name, kAttrib3Size, kAttrib3Type, kAttrib3Location, },
    };
    static UniformInfo uniforms[] = {
      { kUniform1Name, kUniform1Size, kUniform1Type,
        kUniform1FakeLocation, kUniform1RealLocation,
        kUniform1DesiredLocation },
      { kUniform2Name, kUniform2Size, kUniform2Type,
        kUniform2FakeLocation, kUniform2RealLocation,
        kUniform2DesiredLocation },
      { kUniform3Name, kUniform3Size, kUniform3Type,
        kUniform3FakeLocation, kUniform3RealLocation,
        kUniform3DesiredLocation },
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

void GLES2DecoderTestBase::SetupCubemapProgram() {
  {
    static AttribInfo attribs[] = {
      { kAttrib1Name, kAttrib1Size, kAttrib1Type, kAttrib1Location, },
      { kAttrib2Name, kAttrib2Size, kAttrib2Type, kAttrib2Location, },
      { kAttrib3Name, kAttrib3Size, kAttrib3Type, kAttrib3Location, },
    };
    static UniformInfo uniforms[] = {
      { kUniform1Name, kUniform1Size, kUniformCubemapType,
        kUniform1FakeLocation, kUniform1RealLocation,
        kUniform1DesiredLocation, },
      { kUniform2Name, kUniform2Size, kUniform2Type,
        kUniform2FakeLocation, kUniform2RealLocation,
        kUniform2DesiredLocation, },
      { kUniform3Name, kUniform3Size, kUniform3Type,
        kUniform3FakeLocation, kUniform3RealLocation,
        kUniform3DesiredLocation, },
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
    TestHelper::SetupShader(
        gl_.get(), attribs, num_attribs, uniforms, num_uniforms,
        program_service_id);
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

  // Assume the next command will be UseProgram.
  SetupExpectationsForClearingUniforms(uniforms, num_uniforms);
}

void GLES2DecoderTestBase::DoEnableVertexAttribArray(GLint index) {
  EXPECT_CALL(*gl_, EnableVertexAttribArray(index))
      .Times(1)
      .RetiresOnSaturation();
  EnableVertexAttribArray cmd;
  cmd.Init(index);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void GLES2DecoderTestBase::DoBufferData(GLenum target, GLsizei size) {
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

void GLES2DecoderTestBase::DoBufferSubData(
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

void GLES2DecoderTestBase::SetupVertexBuffer() {
  DoEnableVertexAttribArray(1);
  DoBindBuffer(GL_ARRAY_BUFFER, client_buffer_id_, kServiceBufferId);
  GLfloat f = 0;
  DoBufferData(GL_ARRAY_BUFFER, kNumVertices * 2 * sizeof(f));
}

void GLES2DecoderTestBase::SetupAllNeededVertexBuffers() {
  DoBindBuffer(GL_ARRAY_BUFFER, client_buffer_id_, kServiceBufferId);
  DoBufferData(GL_ARRAY_BUFFER, kNumVertices * 16 * sizeof(float));
  DoEnableVertexAttribArray(0);
  DoEnableVertexAttribArray(1);
  DoEnableVertexAttribArray(2);
  DoVertexAttribPointer(0, 2, GL_FLOAT, 0, 0);
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);
  DoVertexAttribPointer(2, 2, GL_FLOAT, 0, 0);
}

void GLES2DecoderTestBase::SetupIndexBuffer() {
  DoBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
               client_element_buffer_id_,
               kServiceElementBufferId);
  static const GLshort indices[] = {100, 1, 2, 3, 4, 5, 6, 7, 100, 9};
  COMPILE_ASSERT(arraysize(indices) == kNumIndices, Indices_is_not_10);
  DoBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices));
  DoBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, 2, indices);
  DoBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 2, sizeof(indices) - 2, &indices[1]);
}

void GLES2DecoderTestBase::SetupTexture() {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               kSharedMemoryId, kSharedMemoryOffset);
};

void GLES2DecoderTestBase::DeleteVertexBuffer() {
  DoDeleteBuffer(client_buffer_id_, kServiceBufferId);
}

void GLES2DecoderTestBase::DeleteIndexBuffer() {
  DoDeleteBuffer(client_element_buffer_id_, kServiceElementBufferId);
}

void GLES2DecoderTestBase::AddExpectationsForSimulatedAttrib0WithError(
    GLsizei num_vertices, GLuint buffer_id, GLenum error) {
  if (gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2) {
    return;
  }

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(error))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BindBuffer(GL_ARRAY_BUFFER, kServiceAttrib0BufferId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BufferData(GL_ARRAY_BUFFER,
                               num_vertices * sizeof(GLfloat) * 4,
                               _, GL_DYNAMIC_DRAW))
      .Times(1)
      .RetiresOnSaturation();
  if (error == GL_NO_ERROR) {
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
}

void GLES2DecoderTestBase::AddExpectationsForSimulatedAttrib0(
    GLsizei num_vertices, GLuint buffer_id) {
  AddExpectationsForSimulatedAttrib0WithError(
      num_vertices, buffer_id, GL_NO_ERROR);
}

GLES2DecoderWithShaderTestBase::MockCommandBufferEngine::
MockCommandBufferEngine() {
  data_.reset(new int8[kSharedBufferSize]);
  ClearSharedMemory();
  valid_buffer_.ptr = data_.get();
  valid_buffer_.size = kSharedBufferSize;
}

GLES2DecoderWithShaderTestBase::MockCommandBufferEngine::
~MockCommandBufferEngine() {}

Buffer
GLES2DecoderWithShaderTestBase::MockCommandBufferEngine::GetSharedMemoryBuffer(
    int32 shm_id) {
  return shm_id == kSharedMemoryId ? valid_buffer_ : invalid_buffer_;
}

void GLES2DecoderWithShaderTestBase::MockCommandBufferEngine::set_token(
    int32 token) {
  DCHECK(false);
}

bool GLES2DecoderWithShaderTestBase::MockCommandBufferEngine::SetGetBuffer(
    int32 /* transfer_buffer_id */) {
  DCHECK(false);
  return false;
}

bool GLES2DecoderWithShaderTestBase::MockCommandBufferEngine::SetGetOffset(
   int32 offset) {
  DCHECK(false);
  return false;
}

int32 GLES2DecoderWithShaderTestBase::MockCommandBufferEngine::GetGetOffset() {
  DCHECK(false);
  return 0;
}

void GLES2DecoderWithShaderTestBase::SetUp() {
  GLES2DecoderTestBase::SetUp();
  SetupDefaultProgram();
}

// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest_0_autogen.h"

}  // namespace gles2
}  // namespace gpu
