// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated. DO NOT EDIT!

// It is included by gles2_cmd_decoder_unittest_1.cc
#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_1_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_1_AUTOGEN_H_

// TODO(gman): ActiveTexture

TEST_F(GLES2DecoderTest1, AttachShaderValidArgs) {
  EXPECT_CALL(*gl_, AttachShader(kServiceProgramId, kServiceShaderId));
  SpecializedSetup<AttachShader, 0>(true);
  AttachShader cmd;
  cmd.Init(client_program_id_, client_shader_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
// TODO(gman): BindAttribLocation

// TODO(gman): BindAttribLocationImmediate

// TODO(gman): BindAttribLocationBucket

TEST_F(GLES2DecoderTest1, BindBufferValidArgs) {
  EXPECT_CALL(*gl_, BindBuffer(GL_ARRAY_BUFFER, kServiceBufferId));
  SpecializedSetup<BindBuffer, 0>(true);
  BindBuffer cmd;
  cmd.Init(GL_ARRAY_BUFFER, client_buffer_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, BindBufferValidArgsNewId) {
  EXPECT_CALL(*gl_, BindBuffer(GL_ARRAY_BUFFER, kNewServiceId));
  EXPECT_CALL(*gl_, GenBuffersARB(1, _))
     .WillOnce(SetArgumentPointee<1>(kNewServiceId));
  SpecializedSetup<BindBuffer, 0>(true);
  BindBuffer cmd;
  cmd.Init(GL_ARRAY_BUFFER, kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(GetBufferInfo(kNewClientId) != NULL);
}

TEST_F(GLES2DecoderTest1, BindBufferInvalidArgs0_0) {
  EXPECT_CALL(*gl_, BindBuffer(_, _)).Times(0);
  SpecializedSetup<BindBuffer, 0>(false);
  BindBuffer cmd;
  cmd.Init(GL_RENDERBUFFER, client_buffer_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, BindFramebufferValidArgs) {
  EXPECT_CALL(*gl_, BindFramebufferEXT(GL_FRAMEBUFFER, kServiceFramebufferId));
  SpecializedSetup<BindFramebuffer, 0>(true);
  BindFramebuffer cmd;
  cmd.Init(GL_FRAMEBUFFER, client_framebuffer_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, BindFramebufferValidArgsNewId) {
  EXPECT_CALL(*gl_, BindFramebufferEXT(GL_FRAMEBUFFER, kNewServiceId));
  EXPECT_CALL(*gl_, GenFramebuffersEXT(1, _))
     .WillOnce(SetArgumentPointee<1>(kNewServiceId));
  SpecializedSetup<BindFramebuffer, 0>(true);
  BindFramebuffer cmd;
  cmd.Init(GL_FRAMEBUFFER, kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(GetFramebufferInfo(kNewClientId) != NULL);
}

TEST_F(GLES2DecoderTest1, BindFramebufferInvalidArgs0_0) {
  EXPECT_CALL(*gl_, BindFramebufferEXT(_, _)).Times(0);
  SpecializedSetup<BindFramebuffer, 0>(false);
  BindFramebuffer cmd;
  cmd.Init(GL_READ_FRAMEBUFFER, client_framebuffer_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, BindRenderbufferValidArgs) {
  EXPECT_CALL(
      *gl_, BindRenderbufferEXT(GL_RENDERBUFFER, kServiceRenderbufferId));
  SpecializedSetup<BindRenderbuffer, 0>(true);
  BindRenderbuffer cmd;
  cmd.Init(GL_RENDERBUFFER, client_renderbuffer_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, BindRenderbufferValidArgsNewId) {
  EXPECT_CALL(*gl_, BindRenderbufferEXT(GL_RENDERBUFFER, kNewServiceId));
  EXPECT_CALL(*gl_, GenRenderbuffersEXT(1, _))
     .WillOnce(SetArgumentPointee<1>(kNewServiceId));
  SpecializedSetup<BindRenderbuffer, 0>(true);
  BindRenderbuffer cmd;
  cmd.Init(GL_RENDERBUFFER, kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(GetRenderbufferInfo(kNewClientId) != NULL);
}

TEST_F(GLES2DecoderTest1, BindRenderbufferInvalidArgs0_0) {
  EXPECT_CALL(*gl_, BindRenderbufferEXT(_, _)).Times(0);
  SpecializedSetup<BindRenderbuffer, 0>(false);
  BindRenderbuffer cmd;
  cmd.Init(GL_FRAMEBUFFER, client_renderbuffer_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, BindTextureValidArgs) {
  EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_2D, kServiceTextureId));
  SpecializedSetup<BindTexture, 0>(true);
  BindTexture cmd;
  cmd.Init(GL_TEXTURE_2D, client_texture_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, BindTextureValidArgsNewId) {
  EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_2D, kNewServiceId));
  EXPECT_CALL(*gl_, GenTextures(1, _))
     .WillOnce(SetArgumentPointee<1>(kNewServiceId));
  SpecializedSetup<BindTexture, 0>(true);
  BindTexture cmd;
  cmd.Init(GL_TEXTURE_2D, kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(GetTextureInfo(kNewClientId) != NULL);
}

TEST_F(GLES2DecoderTest1, BindTextureInvalidArgs0_0) {
  EXPECT_CALL(*gl_, BindTexture(_, _)).Times(0);
  SpecializedSetup<BindTexture, 0>(false);
  BindTexture cmd;
  cmd.Init(GL_TEXTURE_1D, client_texture_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, BindTextureInvalidArgs0_1) {
  EXPECT_CALL(*gl_, BindTexture(_, _)).Times(0);
  SpecializedSetup<BindTexture, 0>(false);
  BindTexture cmd;
  cmd.Init(GL_TEXTURE_3D, client_texture_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, BlendColorValidArgs) {
  EXPECT_CALL(*gl_, BlendColor(1, 2, 3, 4));
  SpecializedSetup<BlendColor, 0>(true);
  BlendColor cmd;
  cmd.Init(1, 2, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, BlendEquationValidArgs) {
  EXPECT_CALL(*gl_, BlendEquation(GL_FUNC_ADD));
  SpecializedSetup<BlendEquation, 0>(true);
  BlendEquation cmd;
  cmd.Init(GL_FUNC_ADD);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, BlendEquationInvalidArgs0_0) {
  EXPECT_CALL(*gl_, BlendEquation(_)).Times(0);
  SpecializedSetup<BlendEquation, 0>(false);
  BlendEquation cmd;
  cmd.Init(GL_MIN);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, BlendEquationInvalidArgs0_1) {
  EXPECT_CALL(*gl_, BlendEquation(_)).Times(0);
  SpecializedSetup<BlendEquation, 0>(false);
  BlendEquation cmd;
  cmd.Init(GL_MAX);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, BlendEquationSeparateValidArgs) {
  EXPECT_CALL(*gl_, BlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD));
  SpecializedSetup<BlendEquationSeparate, 0>(true);
  BlendEquationSeparate cmd;
  cmd.Init(GL_FUNC_ADD, GL_FUNC_ADD);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, BlendEquationSeparateInvalidArgs0_0) {
  EXPECT_CALL(*gl_, BlendEquationSeparate(_, _)).Times(0);
  SpecializedSetup<BlendEquationSeparate, 0>(false);
  BlendEquationSeparate cmd;
  cmd.Init(GL_MIN, GL_FUNC_ADD);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, BlendEquationSeparateInvalidArgs0_1) {
  EXPECT_CALL(*gl_, BlendEquationSeparate(_, _)).Times(0);
  SpecializedSetup<BlendEquationSeparate, 0>(false);
  BlendEquationSeparate cmd;
  cmd.Init(GL_MAX, GL_FUNC_ADD);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, BlendEquationSeparateInvalidArgs1_0) {
  EXPECT_CALL(*gl_, BlendEquationSeparate(_, _)).Times(0);
  SpecializedSetup<BlendEquationSeparate, 0>(false);
  BlendEquationSeparate cmd;
  cmd.Init(GL_FUNC_ADD, GL_MIN);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, BlendEquationSeparateInvalidArgs1_1) {
  EXPECT_CALL(*gl_, BlendEquationSeparate(_, _)).Times(0);
  SpecializedSetup<BlendEquationSeparate, 0>(false);
  BlendEquationSeparate cmd;
  cmd.Init(GL_FUNC_ADD, GL_MAX);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, BlendFuncValidArgs) {
  EXPECT_CALL(*gl_, BlendFunc(GL_ZERO, GL_ZERO));
  SpecializedSetup<BlendFunc, 0>(true);
  BlendFunc cmd;
  cmd.Init(GL_ZERO, GL_ZERO);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, BlendFuncSeparateValidArgs) {
  EXPECT_CALL(*gl_, BlendFuncSeparate(GL_ZERO, GL_ZERO, GL_ZERO, GL_ZERO));
  SpecializedSetup<BlendFuncSeparate, 0>(true);
  BlendFuncSeparate cmd;
  cmd.Init(GL_ZERO, GL_ZERO, GL_ZERO, GL_ZERO);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
// TODO(gman): BufferData

// TODO(gman): BufferDataImmediate

// TODO(gman): BufferSubData

// TODO(gman): BufferSubDataImmediate


TEST_F(GLES2DecoderTest1, CheckFramebufferStatusValidArgs) {
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_FRAMEBUFFER));
  SpecializedSetup<CheckFramebufferStatus, 0>(true);
  CheckFramebufferStatus cmd;
  cmd.Init(GL_FRAMEBUFFER, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, CheckFramebufferStatusInvalidArgs0_0) {
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(_)).Times(0);
  SpecializedSetup<CheckFramebufferStatus, 0>(false);
  CheckFramebufferStatus cmd;
  cmd.Init(GL_READ_FRAMEBUFFER, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, CheckFramebufferStatusInvalidArgsBadSharedMemoryId) {
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_FRAMEBUFFER)).Times(0);
  SpecializedSetup<CheckFramebufferStatus, 0>(false);
  CheckFramebufferStatus cmd;
  cmd.Init(GL_FRAMEBUFFER, kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(GL_FRAMEBUFFER, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest1, ClearValidArgs) {
  EXPECT_CALL(*gl_, Clear(1));
  SpecializedSetup<Clear, 0>(true);
  Clear cmd;
  cmd.Init(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, ClearColorValidArgs) {
  EXPECT_CALL(*gl_, ClearColor(1, 2, 3, 4));
  SpecializedSetup<ClearColor, 0>(true);
  ClearColor cmd;
  cmd.Init(1, 2, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, ClearDepthfValidArgs) {
  EXPECT_CALL(*gl_, ClearDepth(1));
  SpecializedSetup<ClearDepthf, 0>(true);
  ClearDepthf cmd;
  cmd.Init(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, ClearStencilValidArgs) {
  EXPECT_CALL(*gl_, ClearStencil(1));
  SpecializedSetup<ClearStencil, 0>(true);
  ClearStencil cmd;
  cmd.Init(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, ColorMaskValidArgs) {
  EXPECT_CALL(*gl_, ColorMask(1, 2, 3, 4));
  SpecializedSetup<ColorMask, 0>(true);
  ColorMask cmd;
  cmd.Init(1, 2, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
// TODO(gman): CompileShader
// TODO(gman): CompressedTexImage2D

// TODO(gman): CompressedTexImage2DImmediate

// TODO(gman): CompressedTexImage2DBucket
// TODO(gman): CompressedTexSubImage2D

// TODO(gman): CompressedTexSubImage2DImmediate

// TODO(gman): CompressedTexSubImage2DBucket
// TODO(gman): CopyTexImage2D

TEST_F(GLES2DecoderTest1, CopyTexSubImage2DValidArgs) {
  EXPECT_CALL(*gl_, CopyTexSubImage2D(GL_TEXTURE_2D, 2, 3, 4, 5, 6, 7, 8));
  SpecializedSetup<CopyTexSubImage2D, 0>(true);
  CopyTexSubImage2D cmd;
  cmd.Init(GL_TEXTURE_2D, 2, 3, 4, 5, 6, 7, 8);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, CopyTexSubImage2DInvalidArgs0_0) {
  EXPECT_CALL(*gl_, CopyTexSubImage2D(_, _, _, _, _, _, _, _)).Times(0);
  SpecializedSetup<CopyTexSubImage2D, 0>(false);
  CopyTexSubImage2D cmd;
  cmd.Init(GL_PROXY_TEXTURE_CUBE_MAP, 2, 3, 4, 5, 6, 7, 8);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, CopyTexSubImage2DInvalidArgs6_0) {
  EXPECT_CALL(*gl_, CopyTexSubImage2D(_, _, _, _, _, _, _, _)).Times(0);
  SpecializedSetup<CopyTexSubImage2D, 0>(false);
  CopyTexSubImage2D cmd;
  cmd.Init(GL_TEXTURE_2D, 2, 3, 4, 5, 6, -1, 8);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderTest1, CopyTexSubImage2DInvalidArgs7_0) {
  EXPECT_CALL(*gl_, CopyTexSubImage2D(_, _, _, _, _, _, _, _)).Times(0);
  SpecializedSetup<CopyTexSubImage2D, 0>(false);
  CopyTexSubImage2D cmd;
  cmd.Init(GL_TEXTURE_2D, 2, 3, 4, 5, 6, 7, -1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderTest1, CreateProgramValidArgs) {
  EXPECT_CALL(*gl_, CreateProgram())
      .WillOnce(Return(kNewServiceId));
  SpecializedSetup<CreateProgram, 0>(true);
  CreateProgram cmd;
  cmd.Init(kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(GetProgramInfo(kNewClientId) != NULL);
}

TEST_F(GLES2DecoderTest1, CreateShaderValidArgs) {
  EXPECT_CALL(*gl_, CreateShader(GL_VERTEX_SHADER))
      .WillOnce(Return(kNewServiceId));
  SpecializedSetup<CreateShader, 0>(true);
  CreateShader cmd;
  cmd.Init(GL_VERTEX_SHADER, kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(GetShaderInfo(kNewClientId) != NULL);
}

TEST_F(GLES2DecoderTest1, CreateShaderInvalidArgs0_0) {
  EXPECT_CALL(*gl_, CreateShader(_)).Times(0);
  SpecializedSetup<CreateShader, 0>(false);
  CreateShader cmd;
  cmd.Init(GL_GEOMETRY_SHADER, kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, CullFaceValidArgs) {
  EXPECT_CALL(*gl_, CullFace(GL_FRONT));
  SpecializedSetup<CullFace, 0>(true);
  CullFace cmd;
  cmd.Init(GL_FRONT);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, DeleteBuffersValidArgs) {
  EXPECT_CALL(
      *gl_,
      DeleteBuffersARB(1, Pointee(kServiceBufferId)))
      .Times(1);
  GetSharedMemoryAs<GLuint*>()[0] = client_buffer_id_;
  SpecializedSetup<DeleteBuffers, 0>(true);
  DeleteBuffers cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(
      GetBufferInfo(client_buffer_id_) == NULL);
}

TEST_F(GLES2DecoderTest1, DeleteBuffersInvalidArgs) {
  GetSharedMemoryAs<GLuint*>()[0] = kInvalidClientId;
  SpecializedSetup<DeleteBuffers, 0>(false);
  DeleteBuffers cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest1, DeleteBuffersImmediateValidArgs) {
  EXPECT_CALL(
      *gl_,
      DeleteBuffersARB(1, Pointee(kServiceBufferId)))
      .Times(1);
  DeleteBuffersImmediate& cmd = *GetImmediateAs<DeleteBuffersImmediate>();
  SpecializedSetup<DeleteBuffersImmediate, 0>(true);
  cmd.Init(1, &client_buffer_id_);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(client_buffer_id_)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(
      GetBufferInfo(client_buffer_id_) == NULL);
}

TEST_F(GLES2DecoderTest1, DeleteBuffersImmediateInvalidArgs) {
  DeleteBuffersImmediate& cmd = *GetImmediateAs<DeleteBuffersImmediate>();
  SpecializedSetup<DeleteBuffersImmediate, 0>(false);
  GLuint temp = kInvalidClientId;
  cmd.Init(1, &temp);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderTest1, DeleteFramebuffersValidArgs) {
  EXPECT_CALL(
      *gl_,
      DeleteFramebuffersEXT(1, Pointee(kServiceFramebufferId)))
      .Times(1);
  GetSharedMemoryAs<GLuint*>()[0] = client_framebuffer_id_;
  SpecializedSetup<DeleteFramebuffers, 0>(true);
  DeleteFramebuffers cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(
      GetFramebufferInfo(client_framebuffer_id_) == NULL);
}

TEST_F(GLES2DecoderTest1, DeleteFramebuffersInvalidArgs) {
  GetSharedMemoryAs<GLuint*>()[0] = kInvalidClientId;
  SpecializedSetup<DeleteFramebuffers, 0>(false);
  DeleteFramebuffers cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest1, DeleteFramebuffersImmediateValidArgs) {
  EXPECT_CALL(
      *gl_,
      DeleteFramebuffersEXT(1, Pointee(kServiceFramebufferId)))
      .Times(1);
  DeleteFramebuffersImmediate& cmd =
      *GetImmediateAs<DeleteFramebuffersImmediate>();
  SpecializedSetup<DeleteFramebuffersImmediate, 0>(true);
  cmd.Init(1, &client_framebuffer_id_);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(client_framebuffer_id_)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(
      GetFramebufferInfo(client_framebuffer_id_) == NULL);
}

TEST_F(GLES2DecoderTest1, DeleteFramebuffersImmediateInvalidArgs) {
  DeleteFramebuffersImmediate& cmd =
      *GetImmediateAs<DeleteFramebuffersImmediate>();
  SpecializedSetup<DeleteFramebuffersImmediate, 0>(false);
  GLuint temp = kInvalidClientId;
  cmd.Init(1, &temp);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderTest1, DeleteProgramValidArgs) {
  EXPECT_CALL(*gl_, DeleteProgram(kServiceProgramId));
  SpecializedSetup<DeleteProgram, 0>(true);
  DeleteProgram cmd;
  cmd.Init(client_program_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, DeleteRenderbuffersValidArgs) {
  EXPECT_CALL(
      *gl_,
      DeleteRenderbuffersEXT(1, Pointee(kServiceRenderbufferId)))
      .Times(1);
  GetSharedMemoryAs<GLuint*>()[0] = client_renderbuffer_id_;
  SpecializedSetup<DeleteRenderbuffers, 0>(true);
  DeleteRenderbuffers cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(
      GetRenderbufferInfo(client_renderbuffer_id_) == NULL);
}

TEST_F(GLES2DecoderTest1, DeleteRenderbuffersInvalidArgs) {
  GetSharedMemoryAs<GLuint*>()[0] = kInvalidClientId;
  SpecializedSetup<DeleteRenderbuffers, 0>(false);
  DeleteRenderbuffers cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest1, DeleteRenderbuffersImmediateValidArgs) {
  EXPECT_CALL(
      *gl_,
      DeleteRenderbuffersEXT(1, Pointee(kServiceRenderbufferId)))
      .Times(1);
  DeleteRenderbuffersImmediate& cmd =
      *GetImmediateAs<DeleteRenderbuffersImmediate>();
  SpecializedSetup<DeleteRenderbuffersImmediate, 0>(true);
  cmd.Init(1, &client_renderbuffer_id_);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(client_renderbuffer_id_)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(
      GetRenderbufferInfo(client_renderbuffer_id_) == NULL);
}

TEST_F(GLES2DecoderTest1, DeleteRenderbuffersImmediateInvalidArgs) {
  DeleteRenderbuffersImmediate& cmd =
      *GetImmediateAs<DeleteRenderbuffersImmediate>();
  SpecializedSetup<DeleteRenderbuffersImmediate, 0>(false);
  GLuint temp = kInvalidClientId;
  cmd.Init(1, &temp);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderTest1, DeleteShaderValidArgs) {
  EXPECT_CALL(*gl_, DeleteShader(kServiceShaderId));
  SpecializedSetup<DeleteShader, 0>(true);
  DeleteShader cmd;
  cmd.Init(client_shader_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, DeleteTexturesValidArgs) {
  EXPECT_CALL(
      *gl_,
      DeleteTextures(1, Pointee(kServiceTextureId)))
      .Times(1);
  GetSharedMemoryAs<GLuint*>()[0] = client_texture_id_;
  SpecializedSetup<DeleteTextures, 0>(true);
  DeleteTextures cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(
      GetTextureInfo(client_texture_id_) == NULL);
}

TEST_F(GLES2DecoderTest1, DeleteTexturesInvalidArgs) {
  GetSharedMemoryAs<GLuint*>()[0] = kInvalidClientId;
  SpecializedSetup<DeleteTextures, 0>(false);
  DeleteTextures cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest1, DeleteTexturesImmediateValidArgs) {
  EXPECT_CALL(
      *gl_,
      DeleteTextures(1, Pointee(kServiceTextureId)))
      .Times(1);
  DeleteTexturesImmediate& cmd = *GetImmediateAs<DeleteTexturesImmediate>();
  SpecializedSetup<DeleteTexturesImmediate, 0>(true);
  cmd.Init(1, &client_texture_id_);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(client_texture_id_)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(
      GetTextureInfo(client_texture_id_) == NULL);
}

TEST_F(GLES2DecoderTest1, DeleteTexturesImmediateInvalidArgs) {
  DeleteTexturesImmediate& cmd = *GetImmediateAs<DeleteTexturesImmediate>();
  SpecializedSetup<DeleteTexturesImmediate, 0>(false);
  GLuint temp = kInvalidClientId;
  cmd.Init(1, &temp);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderTest1, DepthFuncValidArgs) {
  EXPECT_CALL(*gl_, DepthFunc(GL_NEVER));
  SpecializedSetup<DepthFunc, 0>(true);
  DepthFunc cmd;
  cmd.Init(GL_NEVER);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, DepthMaskValidArgs) {
  EXPECT_CALL(*gl_, DepthMask(1));
  SpecializedSetup<DepthMask, 0>(true);
  DepthMask cmd;
  cmd.Init(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, DepthRangefValidArgs) {
  EXPECT_CALL(*gl_, DepthRange(1, 2));
  SpecializedSetup<DepthRangef, 0>(true);
  DepthRangef cmd;
  cmd.Init(1, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, DetachShaderValidArgs) {
  EXPECT_CALL(*gl_, DetachShader(kServiceProgramId, kServiceShaderId));
  SpecializedSetup<DetachShader, 0>(true);
  DetachShader cmd;
  cmd.Init(client_program_id_, client_shader_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, DisableValidArgs) {
  EXPECT_CALL(*gl_, Disable(GL_BLEND));
  SpecializedSetup<Disable, 0>(true);
  Disable cmd;
  cmd.Init(GL_BLEND);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, DisableInvalidArgs0_0) {
  EXPECT_CALL(*gl_, Disable(_)).Times(0);
  SpecializedSetup<Disable, 0>(false);
  Disable cmd;
  cmd.Init(GL_CLIP_PLANE0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, DisableInvalidArgs0_1) {
  EXPECT_CALL(*gl_, Disable(_)).Times(0);
  SpecializedSetup<Disable, 0>(false);
  Disable cmd;
  cmd.Init(GL_POINT_SPRITE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, DisableVertexAttribArrayValidArgs) {
  EXPECT_CALL(*gl_, DisableVertexAttribArray(1));
  SpecializedSetup<DisableVertexAttribArray, 0>(true);
  DisableVertexAttribArray cmd;
  cmd.Init(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
// TODO(gman): DrawArrays
// TODO(gman): DrawElements


TEST_F(GLES2DecoderTest1, EnableValidArgs) {
  EXPECT_CALL(*gl_, Enable(GL_BLEND));
  SpecializedSetup<Enable, 0>(true);
  Enable cmd;
  cmd.Init(GL_BLEND);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, EnableInvalidArgs0_0) {
  EXPECT_CALL(*gl_, Enable(_)).Times(0);
  SpecializedSetup<Enable, 0>(false);
  Enable cmd;
  cmd.Init(GL_CLIP_PLANE0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, EnableInvalidArgs0_1) {
  EXPECT_CALL(*gl_, Enable(_)).Times(0);
  SpecializedSetup<Enable, 0>(false);
  Enable cmd;
  cmd.Init(GL_POINT_SPRITE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, EnableVertexAttribArrayValidArgs) {
  EXPECT_CALL(*gl_, EnableVertexAttribArray(1));
  SpecializedSetup<EnableVertexAttribArray, 0>(true);
  EnableVertexAttribArray cmd;
  cmd.Init(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, FinishValidArgs) {
  EXPECT_CALL(*gl_, Finish());
  SpecializedSetup<Finish, 0>(true);
  Finish cmd;
  cmd.Init();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, FlushValidArgs) {
  EXPECT_CALL(*gl_, Flush());
  SpecializedSetup<Flush, 0>(true);
  Flush cmd;
  cmd.Init();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, FramebufferRenderbufferValidArgs) {
  EXPECT_CALL(
      *gl_, FramebufferRenderbufferEXT(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
          kServiceRenderbufferId));
  SpecializedSetup<FramebufferRenderbuffer, 0>(true);
  FramebufferRenderbuffer cmd;
  cmd.Init(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
      client_renderbuffer_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, FramebufferRenderbufferInvalidArgs0_0) {
  EXPECT_CALL(*gl_, FramebufferRenderbufferEXT(_, _, _, _)).Times(0);
  SpecializedSetup<FramebufferRenderbuffer, 0>(false);
  FramebufferRenderbuffer cmd;
  cmd.Init(
      GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
      client_renderbuffer_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, FramebufferRenderbufferInvalidArgs2_0) {
  EXPECT_CALL(*gl_, FramebufferRenderbufferEXT(_, _, _, _)).Times(0);
  SpecializedSetup<FramebufferRenderbuffer, 0>(false);
  FramebufferRenderbuffer cmd;
  cmd.Init(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER,
      client_renderbuffer_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, FramebufferTexture2DValidArgs) {
  EXPECT_CALL(
      *gl_, FramebufferTexture2DEXT(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
          kServiceTextureId, 0));
  SpecializedSetup<FramebufferTexture2D, 0>(true);
  FramebufferTexture2D cmd;
  cmd.Init(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, client_texture_id_,
      0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, FramebufferTexture2DInvalidArgs0_0) {
  EXPECT_CALL(*gl_, FramebufferTexture2DEXT(_, _, _, _, _)).Times(0);
  SpecializedSetup<FramebufferTexture2D, 0>(false);
  FramebufferTexture2D cmd;
  cmd.Init(
      GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
      client_texture_id_, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, FramebufferTexture2DInvalidArgs2_0) {
  EXPECT_CALL(*gl_, FramebufferTexture2DEXT(_, _, _, _, _)).Times(0);
  SpecializedSetup<FramebufferTexture2D, 0>(false);
  FramebufferTexture2D cmd;
  cmd.Init(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_PROXY_TEXTURE_CUBE_MAP,
      client_texture_id_, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, FramebufferTexture2DInvalidArgs4_0) {
  EXPECT_CALL(*gl_, FramebufferTexture2DEXT(_, _, _, _, _)).Times(0);
  SpecializedSetup<FramebufferTexture2D, 0>(false);
  FramebufferTexture2D cmd;
  cmd.Init(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, client_texture_id_,
      1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderTest1, FrontFaceValidArgs) {
  EXPECT_CALL(*gl_, FrontFace(GL_CW));
  SpecializedSetup<FrontFace, 0>(true);
  FrontFace cmd;
  cmd.Init(GL_CW);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, GenBuffersValidArgs) {
  EXPECT_CALL(*gl_, GenBuffersARB(1, _))
      .WillOnce(SetArgumentPointee<1>(kNewServiceId));
  GetSharedMemoryAs<GLuint*>()[0] = kNewClientId;
  SpecializedSetup<GenBuffers, 0>(true);
  GenBuffers cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(GetBufferInfo(kNewClientId) != NULL);
}

TEST_F(GLES2DecoderTest1, GenBuffersInvalidArgs) {
  EXPECT_CALL(*gl_, GenBuffersARB(_, _)).Times(0);
  GetSharedMemoryAs<GLuint*>()[0] = client_buffer_id_;
  SpecializedSetup<GenBuffers, 0>(false);
  GenBuffers cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest1, GenBuffersImmediateValidArgs) {
  EXPECT_CALL(*gl_, GenBuffersARB(1, _))
      .WillOnce(SetArgumentPointee<1>(kNewServiceId));
  GenBuffersImmediate& cmd = *GetImmediateAs<GenBuffersImmediate>();
  GLuint temp = kNewClientId;
  SpecializedSetup<GenBuffersImmediate, 0>(true);
  cmd.Init(1, &temp);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(GetBufferInfo(kNewClientId) != NULL);
}

TEST_F(GLES2DecoderTest1, GenBuffersImmediateInvalidArgs) {
  EXPECT_CALL(*gl_, GenBuffersARB(_, _)).Times(0);
  GenBuffersImmediate& cmd = *GetImmediateAs<GenBuffersImmediate>();
  SpecializedSetup<GenBuffersImmediate, 0>(false);
  cmd.Init(1, &client_buffer_id_);
  EXPECT_EQ(error::kInvalidArguments,
            ExecuteImmediateCmd(cmd, sizeof(&client_buffer_id_)));
}

TEST_F(GLES2DecoderTest1, GenerateMipmapValidArgs) {
  EXPECT_CALL(*gl_, GenerateMipmapEXT(GL_TEXTURE_2D));
  SpecializedSetup<GenerateMipmap, 0>(true);
  GenerateMipmap cmd;
  cmd.Init(GL_TEXTURE_2D);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, GenerateMipmapInvalidArgs0_0) {
  EXPECT_CALL(*gl_, GenerateMipmapEXT(_)).Times(0);
  SpecializedSetup<GenerateMipmap, 0>(false);
  GenerateMipmap cmd;
  cmd.Init(GL_TEXTURE_1D);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, GenerateMipmapInvalidArgs0_1) {
  EXPECT_CALL(*gl_, GenerateMipmapEXT(_)).Times(0);
  SpecializedSetup<GenerateMipmap, 0>(false);
  GenerateMipmap cmd;
  cmd.Init(GL_TEXTURE_3D);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, GenFramebuffersValidArgs) {
  EXPECT_CALL(*gl_, GenFramebuffersEXT(1, _))
      .WillOnce(SetArgumentPointee<1>(kNewServiceId));
  GetSharedMemoryAs<GLuint*>()[0] = kNewClientId;
  SpecializedSetup<GenFramebuffers, 0>(true);
  GenFramebuffers cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(GetFramebufferInfo(kNewClientId) != NULL);
}

TEST_F(GLES2DecoderTest1, GenFramebuffersInvalidArgs) {
  EXPECT_CALL(*gl_, GenFramebuffersEXT(_, _)).Times(0);
  GetSharedMemoryAs<GLuint*>()[0] = client_framebuffer_id_;
  SpecializedSetup<GenFramebuffers, 0>(false);
  GenFramebuffers cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest1, GenFramebuffersImmediateValidArgs) {
  EXPECT_CALL(*gl_, GenFramebuffersEXT(1, _))
      .WillOnce(SetArgumentPointee<1>(kNewServiceId));
  GenFramebuffersImmediate& cmd = *GetImmediateAs<GenFramebuffersImmediate>();
  GLuint temp = kNewClientId;
  SpecializedSetup<GenFramebuffersImmediate, 0>(true);
  cmd.Init(1, &temp);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(GetFramebufferInfo(kNewClientId) != NULL);
}

TEST_F(GLES2DecoderTest1, GenFramebuffersImmediateInvalidArgs) {
  EXPECT_CALL(*gl_, GenFramebuffersEXT(_, _)).Times(0);
  GenFramebuffersImmediate& cmd = *GetImmediateAs<GenFramebuffersImmediate>();
  SpecializedSetup<GenFramebuffersImmediate, 0>(false);
  cmd.Init(1, &client_framebuffer_id_);
  EXPECT_EQ(error::kInvalidArguments,
            ExecuteImmediateCmd(cmd, sizeof(&client_framebuffer_id_)));
}

TEST_F(GLES2DecoderTest1, GenRenderbuffersValidArgs) {
  EXPECT_CALL(*gl_, GenRenderbuffersEXT(1, _))
      .WillOnce(SetArgumentPointee<1>(kNewServiceId));
  GetSharedMemoryAs<GLuint*>()[0] = kNewClientId;
  SpecializedSetup<GenRenderbuffers, 0>(true);
  GenRenderbuffers cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(GetRenderbufferInfo(kNewClientId) != NULL);
}

TEST_F(GLES2DecoderTest1, GenRenderbuffersInvalidArgs) {
  EXPECT_CALL(*gl_, GenRenderbuffersEXT(_, _)).Times(0);
  GetSharedMemoryAs<GLuint*>()[0] = client_renderbuffer_id_;
  SpecializedSetup<GenRenderbuffers, 0>(false);
  GenRenderbuffers cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest1, GenRenderbuffersImmediateValidArgs) {
  EXPECT_CALL(*gl_, GenRenderbuffersEXT(1, _))
      .WillOnce(SetArgumentPointee<1>(kNewServiceId));
  GenRenderbuffersImmediate& cmd =
      *GetImmediateAs<GenRenderbuffersImmediate>();
  GLuint temp = kNewClientId;
  SpecializedSetup<GenRenderbuffersImmediate, 0>(true);
  cmd.Init(1, &temp);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(GetRenderbufferInfo(kNewClientId) != NULL);
}

TEST_F(GLES2DecoderTest1, GenRenderbuffersImmediateInvalidArgs) {
  EXPECT_CALL(*gl_, GenRenderbuffersEXT(_, _)).Times(0);
  GenRenderbuffersImmediate& cmd =
      *GetImmediateAs<GenRenderbuffersImmediate>();
  SpecializedSetup<GenRenderbuffersImmediate, 0>(false);
  cmd.Init(1, &client_renderbuffer_id_);
  EXPECT_EQ(error::kInvalidArguments,
            ExecuteImmediateCmd(cmd, sizeof(&client_renderbuffer_id_)));
}

TEST_F(GLES2DecoderTest1, GenTexturesValidArgs) {
  EXPECT_CALL(*gl_, GenTextures(1, _))
      .WillOnce(SetArgumentPointee<1>(kNewServiceId));
  GetSharedMemoryAs<GLuint*>()[0] = kNewClientId;
  SpecializedSetup<GenTextures, 0>(true);
  GenTextures cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(GetTextureInfo(kNewClientId) != NULL);
}

TEST_F(GLES2DecoderTest1, GenTexturesInvalidArgs) {
  EXPECT_CALL(*gl_, GenTextures(_, _)).Times(0);
  GetSharedMemoryAs<GLuint*>()[0] = client_texture_id_;
  SpecializedSetup<GenTextures, 0>(false);
  GenTextures cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest1, GenTexturesImmediateValidArgs) {
  EXPECT_CALL(*gl_, GenTextures(1, _))
      .WillOnce(SetArgumentPointee<1>(kNewServiceId));
  GenTexturesImmediate& cmd = *GetImmediateAs<GenTexturesImmediate>();
  GLuint temp = kNewClientId;
  SpecializedSetup<GenTexturesImmediate, 0>(true);
  cmd.Init(1, &temp);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(GetTextureInfo(kNewClientId) != NULL);
}

TEST_F(GLES2DecoderTest1, GenTexturesImmediateInvalidArgs) {
  EXPECT_CALL(*gl_, GenTextures(_, _)).Times(0);
  GenTexturesImmediate& cmd = *GetImmediateAs<GenTexturesImmediate>();
  SpecializedSetup<GenTexturesImmediate, 0>(false);
  cmd.Init(1, &client_texture_id_);
  EXPECT_EQ(error::kInvalidArguments,
            ExecuteImmediateCmd(cmd, sizeof(&client_texture_id_)));
}
// TODO(gman): GetActiveAttrib

// TODO(gman): GetActiveUniform

// TODO(gman): GetAttachedShaders

// TODO(gman): GetAttribLocation

// TODO(gman): GetAttribLocationImmediate

// TODO(gman): GetAttribLocationBucket


TEST_F(GLES2DecoderTest1, GetBooleanvValidArgs) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  SpecializedSetup<GetBooleanv, 0>(true);
  typedef GetBooleanv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_, GetBooleanv(GL_ACTIVE_TEXTURE, result->GetData()));
  result->size = 0;
  GetBooleanv cmd;
  cmd.Init(GL_ACTIVE_TEXTURE, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(
                GL_ACTIVE_TEXTURE),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, GetBooleanvInvalidArgs0_0) {
  EXPECT_CALL(*gl_, GetBooleanv(_, _)).Times(0);
  SpecializedSetup<GetBooleanv, 0>(false);
  GetBooleanv::Result* result =
      static_cast<GetBooleanv::Result*>(shared_memory_address_);
  result->size = 0;
  GetBooleanv cmd;
  cmd.Init(GL_FOG_HINT, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, GetBooleanvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, GetBooleanv(_, _)).Times(0);
  SpecializedSetup<GetBooleanv, 0>(false);
  GetBooleanv::Result* result =
      static_cast<GetBooleanv::Result*>(shared_memory_address_);
  result->size = 0;
  GetBooleanv cmd;
  cmd.Init(GL_ACTIVE_TEXTURE, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_F(GLES2DecoderTest1, GetBooleanvInvalidArgs1_1) {
  EXPECT_CALL(*gl_, GetBooleanv(_, _)).Times(0);
  SpecializedSetup<GetBooleanv, 0>(false);
  GetBooleanv::Result* result =
      static_cast<GetBooleanv::Result*>(shared_memory_address_);
  result->size = 0;
  GetBooleanv cmd;
  cmd.Init(GL_ACTIVE_TEXTURE, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_F(GLES2DecoderTest1, GetBufferParameterivValidArgs) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  SpecializedSetup<GetBufferParameteriv, 0>(true);
  typedef GetBufferParameteriv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(
      *gl_, GetBufferParameteriv(
          GL_ARRAY_BUFFER, GL_BUFFER_SIZE, result->GetData()));
  result->size = 0;
  GetBufferParameteriv cmd;
  cmd.Init(
      GL_ARRAY_BUFFER, GL_BUFFER_SIZE, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(
                GL_BUFFER_SIZE),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, GetBufferParameterivInvalidArgs0_0) {
  EXPECT_CALL(*gl_, GetBufferParameteriv(_, _, _)).Times(0);
  SpecializedSetup<GetBufferParameteriv, 0>(false);
  GetBufferParameteriv::Result* result =
      static_cast<GetBufferParameteriv::Result*>(shared_memory_address_);
  result->size = 0;
  GetBufferParameteriv cmd;
  cmd.Init(
      GL_RENDERBUFFER, GL_BUFFER_SIZE, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, GetBufferParameterivInvalidArgs1_0) {
  EXPECT_CALL(*gl_, GetBufferParameteriv(_, _, _)).Times(0);
  SpecializedSetup<GetBufferParameteriv, 0>(false);
  GetBufferParameteriv::Result* result =
      static_cast<GetBufferParameteriv::Result*>(shared_memory_address_);
  result->size = 0;
  GetBufferParameteriv cmd;
  cmd.Init(
      GL_ARRAY_BUFFER, GL_PIXEL_PACK_BUFFER, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, GetBufferParameterivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, GetBufferParameteriv(_, _, _)).Times(0);
  SpecializedSetup<GetBufferParameteriv, 0>(false);
  GetBufferParameteriv::Result* result =
      static_cast<GetBufferParameteriv::Result*>(shared_memory_address_);
  result->size = 0;
  GetBufferParameteriv cmd;
  cmd.Init(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_F(GLES2DecoderTest1, GetBufferParameterivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, GetBufferParameteriv(_, _, _)).Times(0);
  SpecializedSetup<GetBufferParameteriv, 0>(false);
  GetBufferParameteriv::Result* result =
      static_cast<GetBufferParameteriv::Result*>(shared_memory_address_);
  result->size = 0;
  GetBufferParameteriv cmd;
  cmd.Init(
      GL_ARRAY_BUFFER, GL_BUFFER_SIZE, shared_memory_id_,
      kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_F(GLES2DecoderTest1, GetErrorValidArgs) {
  EXPECT_CALL(*gl_, GetError());
  SpecializedSetup<GetError, 0>(true);
  GetError cmd;
  cmd.Init(shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, GetErrorInvalidArgsBadSharedMemoryId) {
  EXPECT_CALL(*gl_, GetError()).Times(0);
  SpecializedSetup<GetError, 0>(false);
  GetError cmd;
  cmd.Init(kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest1, GetFloatvValidArgs) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  SpecializedSetup<GetFloatv, 0>(true);
  typedef GetFloatv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_, GetFloatv(GL_ACTIVE_TEXTURE, result->GetData()));
  result->size = 0;
  GetFloatv cmd;
  cmd.Init(GL_ACTIVE_TEXTURE, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(
                GL_ACTIVE_TEXTURE),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, GetFloatvInvalidArgs0_0) {
  EXPECT_CALL(*gl_, GetFloatv(_, _)).Times(0);
  SpecializedSetup<GetFloatv, 0>(false);
  GetFloatv::Result* result =
      static_cast<GetFloatv::Result*>(shared_memory_address_);
  result->size = 0;
  GetFloatv cmd;
  cmd.Init(GL_FOG_HINT, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, GetFloatvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, GetFloatv(_, _)).Times(0);
  SpecializedSetup<GetFloatv, 0>(false);
  GetFloatv::Result* result =
      static_cast<GetFloatv::Result*>(shared_memory_address_);
  result->size = 0;
  GetFloatv cmd;
  cmd.Init(GL_ACTIVE_TEXTURE, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_F(GLES2DecoderTest1, GetFloatvInvalidArgs1_1) {
  EXPECT_CALL(*gl_, GetFloatv(_, _)).Times(0);
  SpecializedSetup<GetFloatv, 0>(false);
  GetFloatv::Result* result =
      static_cast<GetFloatv::Result*>(shared_memory_address_);
  result->size = 0;
  GetFloatv cmd;
  cmd.Init(GL_ACTIVE_TEXTURE, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_F(GLES2DecoderTest1, GetFramebufferAttachmentParameterivValidArgs) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  SpecializedSetup<GetFramebufferAttachmentParameteriv, 0>(true);
  typedef GetFramebufferAttachmentParameteriv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(
      *gl_, GetFramebufferAttachmentParameterivEXT(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, result->GetData()));
  result->size = 0;
  GetFramebufferAttachmentParameteriv cmd;
  cmd.Init(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
      GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(
                GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, GetFramebufferAttachmentParameterivInvalidArgs0_0) {
  EXPECT_CALL(
      *gl_, GetFramebufferAttachmentParameterivEXT(_, _, _, _)).Times(0);
  SpecializedSetup<GetFramebufferAttachmentParameteriv, 0>(false);
  GetFramebufferAttachmentParameteriv::Result* result =
      static_cast<GetFramebufferAttachmentParameteriv::Result*>(
          shared_memory_address_);
  result->size = 0;
  GetFramebufferAttachmentParameteriv cmd;
  cmd.Init(
      GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
      GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, GetFramebufferAttachmentParameterivInvalidArgs3_0) {
  EXPECT_CALL(
      *gl_, GetFramebufferAttachmentParameterivEXT(_, _, _, _)).Times(0);
  SpecializedSetup<GetFramebufferAttachmentParameteriv, 0>(false);
  GetFramebufferAttachmentParameteriv::Result* result =
      static_cast<GetFramebufferAttachmentParameteriv::Result*>(
          shared_memory_address_);
  result->size = 0;
  GetFramebufferAttachmentParameteriv cmd;
  cmd.Init(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
      GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_F(GLES2DecoderTest1, GetFramebufferAttachmentParameterivInvalidArgs3_1) {
  EXPECT_CALL(
      *gl_, GetFramebufferAttachmentParameterivEXT(_, _, _, _)).Times(0);
  SpecializedSetup<GetFramebufferAttachmentParameteriv, 0>(false);
  GetFramebufferAttachmentParameteriv::Result* result =
      static_cast<GetFramebufferAttachmentParameteriv::Result*>(
          shared_memory_address_);
  result->size = 0;
  GetFramebufferAttachmentParameteriv cmd;
  cmd.Init(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
      GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, shared_memory_id_,
      kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_F(GLES2DecoderTest1, GetIntegervValidArgs) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  SpecializedSetup<GetIntegerv, 0>(true);
  typedef GetIntegerv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_ACTIVE_TEXTURE, result->GetData()));
  result->size = 0;
  GetIntegerv cmd;
  cmd.Init(GL_ACTIVE_TEXTURE, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(
                GL_ACTIVE_TEXTURE),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, GetIntegervInvalidArgs0_0) {
  EXPECT_CALL(*gl_, GetIntegerv(_, _)).Times(0);
  SpecializedSetup<GetIntegerv, 0>(false);
  GetIntegerv::Result* result =
      static_cast<GetIntegerv::Result*>(shared_memory_address_);
  result->size = 0;
  GetIntegerv cmd;
  cmd.Init(GL_FOG_HINT, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, GetIntegervInvalidArgs1_0) {
  EXPECT_CALL(*gl_, GetIntegerv(_, _)).Times(0);
  SpecializedSetup<GetIntegerv, 0>(false);
  GetIntegerv::Result* result =
      static_cast<GetIntegerv::Result*>(shared_memory_address_);
  result->size = 0;
  GetIntegerv cmd;
  cmd.Init(GL_ACTIVE_TEXTURE, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_F(GLES2DecoderTest1, GetIntegervInvalidArgs1_1) {
  EXPECT_CALL(*gl_, GetIntegerv(_, _)).Times(0);
  SpecializedSetup<GetIntegerv, 0>(false);
  GetIntegerv::Result* result =
      static_cast<GetIntegerv::Result*>(shared_memory_address_);
  result->size = 0;
  GetIntegerv cmd;
  cmd.Init(GL_ACTIVE_TEXTURE, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_F(GLES2DecoderTest1, GetProgramivValidArgs) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  SpecializedSetup<GetProgramiv, 0>(true);
  typedef GetProgramiv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(
      *gl_, GetProgramiv(
          kServiceProgramId, GL_DELETE_STATUS, result->GetData()));
  result->size = 0;
  GetProgramiv cmd;
  cmd.Init(
      client_program_id_, GL_DELETE_STATUS, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(
                GL_DELETE_STATUS),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, GetProgramivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, GetProgramiv(_, _, _)).Times(0);
  SpecializedSetup<GetProgramiv, 0>(false);
  GetProgramiv::Result* result =
      static_cast<GetProgramiv::Result*>(shared_memory_address_);
  result->size = 0;
  GetProgramiv cmd;
  cmd.Init(client_program_id_, GL_DELETE_STATUS, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_F(GLES2DecoderTest1, GetProgramivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, GetProgramiv(_, _, _)).Times(0);
  SpecializedSetup<GetProgramiv, 0>(false);
  GetProgramiv::Result* result =
      static_cast<GetProgramiv::Result*>(shared_memory_address_);
  result->size = 0;
  GetProgramiv cmd;
  cmd.Init(
      client_program_id_, GL_DELETE_STATUS, shared_memory_id_,
      kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_F(GLES2DecoderTest1, GetProgramInfoLogValidArgs) {
  const char* kInfo = "hello";
  const uint32 kBucketId = 123;
  SpecializedSetup<GetProgramInfoLog, 0>(true);

  GetProgramInfoLog cmd;
  cmd.Init(client_program_id_, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  ASSERT_TRUE(bucket != NULL);
  EXPECT_EQ(strlen(kInfo) + 1, bucket->size());
  EXPECT_EQ(0, memcmp(bucket->GetData(0, bucket->size()), kInfo,
                      bucket->size()));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, GetProgramInfoLogInvalidArgs) {
  const uint32 kBucketId = 123;
  GetProgramInfoLog cmd;
  cmd.Init(kInvalidClientId, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderTest1, GetRenderbufferParameterivValidArgs) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  SpecializedSetup<GetRenderbufferParameteriv, 0>(true);
  typedef GetRenderbufferParameteriv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(
      *gl_, GetRenderbufferParameterivEXT(
          GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, result->GetData()));
  result->size = 0;
  GetRenderbufferParameteriv cmd;
  cmd.Init(
      GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(
                GL_RENDERBUFFER_WIDTH),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, GetRenderbufferParameterivInvalidArgs0_0) {
  EXPECT_CALL(*gl_, GetRenderbufferParameterivEXT(_, _, _)).Times(0);
  SpecializedSetup<GetRenderbufferParameteriv, 0>(false);
  GetRenderbufferParameteriv::Result* result =
      static_cast<GetRenderbufferParameteriv::Result*>(shared_memory_address_);
  result->size = 0;
  GetRenderbufferParameteriv cmd;
  cmd.Init(
      GL_FRAMEBUFFER, GL_RENDERBUFFER_WIDTH, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, GetRenderbufferParameterivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, GetRenderbufferParameterivEXT(_, _, _)).Times(0);
  SpecializedSetup<GetRenderbufferParameteriv, 0>(false);
  GetRenderbufferParameteriv::Result* result =
      static_cast<GetRenderbufferParameteriv::Result*>(shared_memory_address_);
  result->size = 0;
  GetRenderbufferParameteriv cmd;
  cmd.Init(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_F(GLES2DecoderTest1, GetRenderbufferParameterivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, GetRenderbufferParameterivEXT(_, _, _)).Times(0);
  SpecializedSetup<GetRenderbufferParameteriv, 0>(false);
  GetRenderbufferParameteriv::Result* result =
      static_cast<GetRenderbufferParameteriv::Result*>(shared_memory_address_);
  result->size = 0;
  GetRenderbufferParameteriv cmd;
  cmd.Init(
      GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, shared_memory_id_,
      kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_F(GLES2DecoderTest1, GetShaderivValidArgs) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  SpecializedSetup<GetShaderiv, 0>(true);
  typedef GetShaderiv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(
      *gl_, GetShaderiv(kServiceShaderId, GL_SHADER_TYPE, result->GetData()));
  result->size = 0;
  GetShaderiv cmd;
  cmd.Init(
      client_shader_id_, GL_SHADER_TYPE, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(
                GL_SHADER_TYPE),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, GetShaderivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, GetShaderiv(_, _, _)).Times(0);
  SpecializedSetup<GetShaderiv, 0>(false);
  GetShaderiv::Result* result =
      static_cast<GetShaderiv::Result*>(shared_memory_address_);
  result->size = 0;
  GetShaderiv cmd;
  cmd.Init(client_shader_id_, GL_SHADER_TYPE, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_F(GLES2DecoderTest1, GetShaderivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, GetShaderiv(_, _, _)).Times(0);
  SpecializedSetup<GetShaderiv, 0>(false);
  GetShaderiv::Result* result =
      static_cast<GetShaderiv::Result*>(shared_memory_address_);
  result->size = 0;
  GetShaderiv cmd;
  cmd.Init(
      client_shader_id_, GL_SHADER_TYPE, shared_memory_id_,
      kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}
// TODO(gman): GetShaderInfoLog
// TODO(gman): GetShaderPrecisionFormat

// TODO(gman): GetShaderSource
// TODO(gman): GetString


TEST_F(GLES2DecoderTest1, GetTexParameterfvValidArgs) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  SpecializedSetup<GetTexParameterfv, 0>(true);
  typedef GetTexParameterfv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(
      *gl_, GetTexParameterfv(
          GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, result->GetData()));
  result->size = 0;
  GetTexParameterfv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(
                GL_TEXTURE_MAG_FILTER),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, GetTexParameterfvInvalidArgs0_0) {
  EXPECT_CALL(*gl_, GetTexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<GetTexParameterfv, 0>(false);
  GetTexParameterfv::Result* result =
      static_cast<GetTexParameterfv::Result*>(shared_memory_address_);
  result->size = 0;
  GetTexParameterfv cmd;
  cmd.Init(
      GL_PROXY_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, GetTexParameterfvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, GetTexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<GetTexParameterfv, 0>(false);
  GetTexParameterfv::Result* result =
      static_cast<GetTexParameterfv::Result*>(shared_memory_address_);
  result->size = 0;
  GetTexParameterfv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_GENERATE_MIPMAP, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, GetTexParameterfvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, GetTexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<GetTexParameterfv, 0>(false);
  GetTexParameterfv::Result* result =
      static_cast<GetTexParameterfv::Result*>(shared_memory_address_);
  result->size = 0;
  GetTexParameterfv cmd;
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_F(GLES2DecoderTest1, GetTexParameterfvInvalidArgs2_1) {
  EXPECT_CALL(*gl_, GetTexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<GetTexParameterfv, 0>(false);
  GetTexParameterfv::Result* result =
      static_cast<GetTexParameterfv::Result*>(shared_memory_address_);
  result->size = 0;
  GetTexParameterfv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_F(GLES2DecoderTest1, GetTexParameterivValidArgs) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  SpecializedSetup<GetTexParameteriv, 0>(true);
  typedef GetTexParameteriv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(
      *gl_, GetTexParameteriv(
          GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, result->GetData()));
  result->size = 0;
  GetTexParameteriv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(
                GL_TEXTURE_MAG_FILTER),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, GetTexParameterivInvalidArgs0_0) {
  EXPECT_CALL(*gl_, GetTexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<GetTexParameteriv, 0>(false);
  GetTexParameteriv::Result* result =
      static_cast<GetTexParameteriv::Result*>(shared_memory_address_);
  result->size = 0;
  GetTexParameteriv cmd;
  cmd.Init(
      GL_PROXY_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, GetTexParameterivInvalidArgs1_0) {
  EXPECT_CALL(*gl_, GetTexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<GetTexParameteriv, 0>(false);
  GetTexParameteriv::Result* result =
      static_cast<GetTexParameteriv::Result*>(shared_memory_address_);
  result->size = 0;
  GetTexParameteriv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_GENERATE_MIPMAP, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest1, GetTexParameterivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, GetTexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<GetTexParameteriv, 0>(false);
  GetTexParameteriv::Result* result =
      static_cast<GetTexParameteriv::Result*>(shared_memory_address_);
  result->size = 0;
  GetTexParameteriv cmd;
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_F(GLES2DecoderTest1, GetTexParameterivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, GetTexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<GetTexParameteriv, 0>(false);
  GetTexParameteriv::Result* result =
      static_cast<GetTexParameteriv::Result*>(shared_memory_address_);
  result->size = 0;
  GetTexParameteriv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}
// TODO(gman): GetUniformfv

// TODO(gman): GetUniformiv

// TODO(gman): GetUniformLocation

// TODO(gman): GetUniformLocationImmediate

// TODO(gman): GetUniformLocationBucket


TEST_F(GLES2DecoderTest1, GetVertexAttribfvValidArgs) {
  SpecializedSetup<GetVertexAttribfv, 0>(true);
  typedef GetVertexAttribfv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->size = 0;
  GetVertexAttribfv cmd;
  cmd.Init(
      1, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(
                GL_VERTEX_ATTRIB_ARRAY_NORMALIZED),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest1, GetVertexAttribfvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, GetVertexAttribfv(_, _, _)).Times(0);
  SpecializedSetup<GetVertexAttribfv, 0>(false);
  GetVertexAttribfv::Result* result =
      static_cast<GetVertexAttribfv::Result*>(shared_memory_address_);
  result->size = 0;
  GetVertexAttribfv cmd;
  cmd.Init(1, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_F(GLES2DecoderTest1, GetVertexAttribfvInvalidArgs2_1) {
  EXPECT_CALL(*gl_, GetVertexAttribfv(_, _, _)).Times(0);
  SpecializedSetup<GetVertexAttribfv, 0>(false);
  GetVertexAttribfv::Result* result =
      static_cast<GetVertexAttribfv::Result*>(shared_memory_address_);
  result->size = 0;
  GetVertexAttribfv cmd;
  cmd.Init(
      1, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, shared_memory_id_,
      kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}
#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_1_AUTOGEN_H_

