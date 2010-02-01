// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated. DO NOT EDIT!

// It is included by gles2_cmd_decoder_unittest.cc
#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_AUTOGEN_H_


TEST_F(GLES2DecoderTest, ActiveTextureValidArgs) {
  EXPECT_CALL(*gl_, ActiveTexture(1));
  SpecializedSetup<ActiveTexture, 0>();
  ActiveTexture cmd;
  cmd.Init(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, AttachShaderValidArgs) {
  EXPECT_CALL(*gl_, AttachShader(kServiceProgramId, kServiceShaderId));
  SpecializedSetup<AttachShader, 0>();
  AttachShader cmd;
  cmd.Init(client_program_id_, client_shader_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}
// TODO(gman): BindAttribLocation

// TODO(gman): BindAttribLocationImmediate


TEST_F(GLES2DecoderTest, BindBufferValidArgs) {
  EXPECT_CALL(*gl_, BindBuffer(GL_ARRAY_BUFFER, kServiceBufferId));
  SpecializedSetup<BindBuffer, 0>();
  BindBuffer cmd;
  cmd.Init(GL_ARRAY_BUFFER, client_buffer_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, BindBufferInvalidArgs0_0) {
  EXPECT_CALL(*gl_, BindBuffer(_, _)).Times(0);
  SpecializedSetup<BindBuffer, 0>();
  BindBuffer cmd;
  cmd.Init(GL_RENDERBUFFER, client_buffer_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, BindFramebufferValidArgs) {
  EXPECT_CALL(*gl_, BindFramebufferEXT(GL_FRAMEBUFFER, kServiceFramebufferId));
  SpecializedSetup<BindFramebuffer, 0>();
  BindFramebuffer cmd;
  cmd.Init(GL_FRAMEBUFFER, client_framebuffer_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, BindRenderbufferValidArgs) {
  EXPECT_CALL(
      *gl_, BindRenderbufferEXT(GL_RENDERBUFFER, kServiceRenderbufferId));
  SpecializedSetup<BindRenderbuffer, 0>();
  BindRenderbuffer cmd;
  cmd.Init(GL_RENDERBUFFER, client_renderbuffer_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, BindTextureValidArgs) {
  EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_2D, kServiceTextureId));
  SpecializedSetup<BindTexture, 0>();
  BindTexture cmd;
  cmd.Init(GL_TEXTURE_2D, client_texture_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, BindTextureInvalidArgs0_0) {
  EXPECT_CALL(*gl_, BindTexture(_, _)).Times(0);
  SpecializedSetup<BindTexture, 0>();
  BindTexture cmd;
  cmd.Init(GL_TEXTURE_1D, client_texture_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, BindTextureInvalidArgs0_1) {
  EXPECT_CALL(*gl_, BindTexture(_, _)).Times(0);
  SpecializedSetup<BindTexture, 0>();
  BindTexture cmd;
  cmd.Init(GL_TEXTURE_3D, client_texture_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, BlendColorValidArgs) {
  EXPECT_CALL(*gl_, BlendColor(1, 2, 3, 4));
  SpecializedSetup<BlendColor, 0>();
  BlendColor cmd;
  cmd.Init(1, 2, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, BlendEquationValidArgs) {
  EXPECT_CALL(*gl_, BlendEquation(GL_FUNC_ADD));
  SpecializedSetup<BlendEquation, 0>();
  BlendEquation cmd;
  cmd.Init(GL_FUNC_ADD);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, BlendEquationInvalidArgs0_0) {
  EXPECT_CALL(*gl_, BlendEquation(_)).Times(0);
  SpecializedSetup<BlendEquation, 0>();
  BlendEquation cmd;
  cmd.Init(GL_MIN);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, BlendEquationInvalidArgs0_1) {
  EXPECT_CALL(*gl_, BlendEquation(_)).Times(0);
  SpecializedSetup<BlendEquation, 0>();
  BlendEquation cmd;
  cmd.Init(GL_MAX);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, BlendEquationSeparateValidArgs) {
  EXPECT_CALL(*gl_, BlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD));
  SpecializedSetup<BlendEquationSeparate, 0>();
  BlendEquationSeparate cmd;
  cmd.Init(GL_FUNC_ADD, GL_FUNC_ADD);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, BlendEquationSeparateInvalidArgs0_0) {
  EXPECT_CALL(*gl_, BlendEquationSeparate(_, _)).Times(0);
  SpecializedSetup<BlendEquationSeparate, 0>();
  BlendEquationSeparate cmd;
  cmd.Init(GL_MIN, GL_FUNC_ADD);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, BlendEquationSeparateInvalidArgs0_1) {
  EXPECT_CALL(*gl_, BlendEquationSeparate(_, _)).Times(0);
  SpecializedSetup<BlendEquationSeparate, 0>();
  BlendEquationSeparate cmd;
  cmd.Init(GL_MAX, GL_FUNC_ADD);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, BlendEquationSeparateInvalidArgs1_0) {
  EXPECT_CALL(*gl_, BlendEquationSeparate(_, _)).Times(0);
  SpecializedSetup<BlendEquationSeparate, 0>();
  BlendEquationSeparate cmd;
  cmd.Init(GL_FUNC_ADD, GL_MIN);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, BlendEquationSeparateInvalidArgs1_1) {
  EXPECT_CALL(*gl_, BlendEquationSeparate(_, _)).Times(0);
  SpecializedSetup<BlendEquationSeparate, 0>();
  BlendEquationSeparate cmd;
  cmd.Init(GL_FUNC_ADD, GL_MAX);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, BlendFuncValidArgs) {
  EXPECT_CALL(*gl_, BlendFunc(GL_ZERO, GL_ZERO));
  SpecializedSetup<BlendFunc, 0>();
  BlendFunc cmd;
  cmd.Init(GL_ZERO, GL_ZERO);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, BlendFuncSeparateValidArgs) {
  EXPECT_CALL(*gl_, BlendFuncSeparate(GL_ZERO, GL_ZERO, GL_ZERO, GL_ZERO));
  SpecializedSetup<BlendFuncSeparate, 0>();
  BlendFuncSeparate cmd;
  cmd.Init(GL_ZERO, GL_ZERO, GL_ZERO, GL_ZERO);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}
// TODO(gman): BufferData

// TODO(gman): BufferDataImmediate

// TODO(gman): BufferSubData

// TODO(gman): BufferSubDataImmediate


TEST_F(GLES2DecoderTest, CheckFramebufferStatusValidArgs) {
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_FRAMEBUFFER));
  SpecializedSetup<CheckFramebufferStatus, 0>();
  CheckFramebufferStatus cmd;
  cmd.Init(GL_FRAMEBUFFER);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, ClearValidArgs) {
  EXPECT_CALL(*gl_, Clear(1));
  SpecializedSetup<Clear, 0>();
  Clear cmd;
  cmd.Init(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, ClearColorValidArgs) {
  EXPECT_CALL(*gl_, ClearColor(1, 2, 3, 4));
  SpecializedSetup<ClearColor, 0>();
  ClearColor cmd;
  cmd.Init(1, 2, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, ClearDepthfValidArgs) {
  EXPECT_CALL(*gl_, ClearDepth(1));
  SpecializedSetup<ClearDepthf, 0>();
  ClearDepthf cmd;
  cmd.Init(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, ClearStencilValidArgs) {
  EXPECT_CALL(*gl_, ClearStencil(1));
  SpecializedSetup<ClearStencil, 0>();
  ClearStencil cmd;
  cmd.Init(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, ColorMaskValidArgs) {
  EXPECT_CALL(*gl_, ColorMask(1, 2, 3, 4));
  SpecializedSetup<ColorMask, 0>();
  ColorMask cmd;
  cmd.Init(1, 2, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, CompileShaderValidArgs) {
  EXPECT_CALL(*gl_, CompileShader(kServiceShaderId));
  SpecializedSetup<CompileShader, 0>();
  CompileShader cmd;
  cmd.Init(client_shader_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}
// TODO(gman): CompressedTexImage2D

// TODO(gman): CompressedTexImage2DImmediate

// TODO(gman): CompressedTexSubImage2D

// TODO(gman): CompressedTexSubImage2DImmediate


TEST_F(GLES2DecoderTest, CopyTexImage2DValidArgs) {
  EXPECT_CALL(*gl_, CopyTexImage2D(GL_TEXTURE_2D, 2, 3, 4, 5, 6, 7, 8));
  SpecializedSetup<CopyTexImage2D, 0>();
  CopyTexImage2D cmd;
  cmd.Init(GL_TEXTURE_2D, 2, 3, 4, 5, 6, 7, 8);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, CopyTexImage2DInvalidArgs0_0) {
  EXPECT_CALL(*gl_, CopyTexImage2D(_, _, _, _, _, _, _, _)).Times(0);
  SpecializedSetup<CopyTexImage2D, 0>();
  CopyTexImage2D cmd;
  cmd.Init(GL_PROXY_TEXTURE_CUBE_MAP, 2, 3, 4, 5, 6, 7, 8);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, CopyTexSubImage2DValidArgs) {
  EXPECT_CALL(*gl_, CopyTexSubImage2D(GL_TEXTURE_2D, 2, 3, 4, 5, 6, 7, 8));
  SpecializedSetup<CopyTexSubImage2D, 0>();
  CopyTexSubImage2D cmd;
  cmd.Init(GL_TEXTURE_2D, 2, 3, 4, 5, 6, 7, 8);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, CopyTexSubImage2DInvalidArgs0_0) {
  EXPECT_CALL(*gl_, CopyTexSubImage2D(_, _, _, _, _, _, _, _)).Times(0);
  SpecializedSetup<CopyTexSubImage2D, 0>();
  CopyTexSubImage2D cmd;
  cmd.Init(GL_PROXY_TEXTURE_CUBE_MAP, 2, 3, 4, 5, 6, 7, 8);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, CreateProgramValidArgs) {
  EXPECT_CALL(*gl_, CreateProgram())
      .WillOnce(Return(kNewServiceId));
  SpecializedSetup<CreateProgram, 0>();
  CreateProgram cmd;
  cmd.Init(kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GetServiceId(kNewClientId), kNewServiceId);
}

TEST_F(GLES2DecoderTest, CreateShaderValidArgs) {
  EXPECT_CALL(*gl_, CreateShader(GL_VERTEX_SHADER))
      .WillOnce(Return(kNewServiceId));
  SpecializedSetup<CreateShader, 0>();
  CreateShader cmd;
  cmd.Init(GL_VERTEX_SHADER, kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GetServiceId(kNewClientId), kNewServiceId);
}

TEST_F(GLES2DecoderTest, CullFaceValidArgs) {
  EXPECT_CALL(*gl_, CullFace(GL_FRONT));
  SpecializedSetup<CullFace, 0>();
  CullFace cmd;
  cmd.Init(GL_FRONT);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, DeleteBuffersValidArgs) {
  EXPECT_CALL(
      *gl_,
      DeleteBuffersARB(1, Pointee(kServiceBufferId)))
      .Times(1);
  GetSharedMemoryAs<GLuint*>()[0] = client_buffer_id_;
  SpecializedSetup<DeleteBuffers, 0>();
  DeleteBuffers cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GetServiceId(kNewClientId), 0u);
}

TEST_F(GLES2DecoderTest, DeleteBuffersInvalidArgs) {
  EXPECT_CALL(*gl_, DeleteBuffersARB(1, Pointee(0)))
      .Times(1);
  GetSharedMemoryAs<GLuint*>()[0] = kInvalidClientId;
  SpecializedSetup<DeleteBuffers, 0>();
  DeleteBuffers cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, DeleteBuffersImmediateValidArgs) {
  EXPECT_CALL(
      *gl_,
      DeleteBuffersARB(1, Pointee(kServiceBufferId)))
      .Times(1);
  DeleteBuffersImmediate& cmd = *GetImmediateAs<DeleteBuffersImmediate>();
  SpecializedSetup<DeleteBuffersImmediate, 0>();
  cmd.Init(1, &client_buffer_id_);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(client_buffer_id_)));
  EXPECT_EQ(GetServiceId(kNewClientId), 0u);
}

TEST_F(GLES2DecoderTest, DeleteBuffersImmediateInvalidArgs) {
  EXPECT_CALL(*gl_, DeleteBuffersARB(1, Pointee(0)))
      .Times(1);
  DeleteBuffersImmediate& cmd = *GetImmediateAs<DeleteBuffersImmediate>();
  SpecializedSetup<DeleteBuffersImmediate, 0>();
  GLuint temp = kInvalidClientId;
  cmd.Init(1, &temp);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderTest, DeleteFramebuffersValidArgs) {
  EXPECT_CALL(
      *gl_,
      DeleteFramebuffersEXT(1, Pointee(kServiceFramebufferId)))
      .Times(1);
  GetSharedMemoryAs<GLuint*>()[0] = client_framebuffer_id_;
  SpecializedSetup<DeleteFramebuffers, 0>();
  DeleteFramebuffers cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GetServiceId(kNewClientId), 0u);
}

TEST_F(GLES2DecoderTest, DeleteFramebuffersInvalidArgs) {
  EXPECT_CALL(*gl_, DeleteFramebuffersEXT(1, Pointee(0)))
      .Times(1);
  GetSharedMemoryAs<GLuint*>()[0] = kInvalidClientId;
  SpecializedSetup<DeleteFramebuffers, 0>();
  DeleteFramebuffers cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, DeleteFramebuffersImmediateValidArgs) {
  EXPECT_CALL(
      *gl_,
      DeleteFramebuffersEXT(1, Pointee(kServiceFramebufferId)))
      .Times(1);
  DeleteFramebuffersImmediate& cmd =
      *GetImmediateAs<DeleteFramebuffersImmediate>();
  SpecializedSetup<DeleteFramebuffersImmediate, 0>();
  cmd.Init(1, &client_framebuffer_id_);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(client_framebuffer_id_)));
  EXPECT_EQ(GetServiceId(kNewClientId), 0u);
}

TEST_F(GLES2DecoderTest, DeleteFramebuffersImmediateInvalidArgs) {
  EXPECT_CALL(*gl_, DeleteFramebuffersEXT(1, Pointee(0)))
      .Times(1);
  DeleteFramebuffersImmediate& cmd =
      *GetImmediateAs<DeleteFramebuffersImmediate>();
  SpecializedSetup<DeleteFramebuffersImmediate, 0>();
  GLuint temp = kInvalidClientId;
  cmd.Init(1, &temp);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}
// TODO(gman): DeleteProgram


TEST_F(GLES2DecoderTest, DeleteRenderbuffersValidArgs) {
  EXPECT_CALL(
      *gl_,
      DeleteRenderbuffersEXT(1, Pointee(kServiceRenderbufferId)))
      .Times(1);
  GetSharedMemoryAs<GLuint*>()[0] = client_renderbuffer_id_;
  SpecializedSetup<DeleteRenderbuffers, 0>();
  DeleteRenderbuffers cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GetServiceId(kNewClientId), 0u);
}

TEST_F(GLES2DecoderTest, DeleteRenderbuffersInvalidArgs) {
  EXPECT_CALL(*gl_, DeleteRenderbuffersEXT(1, Pointee(0)))
      .Times(1);
  GetSharedMemoryAs<GLuint*>()[0] = kInvalidClientId;
  SpecializedSetup<DeleteRenderbuffers, 0>();
  DeleteRenderbuffers cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, DeleteRenderbuffersImmediateValidArgs) {
  EXPECT_CALL(
      *gl_,
      DeleteRenderbuffersEXT(1, Pointee(kServiceRenderbufferId)))
      .Times(1);
  DeleteRenderbuffersImmediate& cmd =
      *GetImmediateAs<DeleteRenderbuffersImmediate>();
  SpecializedSetup<DeleteRenderbuffersImmediate, 0>();
  cmd.Init(1, &client_renderbuffer_id_);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(client_renderbuffer_id_)));
  EXPECT_EQ(GetServiceId(kNewClientId), 0u);
}

TEST_F(GLES2DecoderTest, DeleteRenderbuffersImmediateInvalidArgs) {
  EXPECT_CALL(*gl_, DeleteRenderbuffersEXT(1, Pointee(0)))
      .Times(1);
  DeleteRenderbuffersImmediate& cmd =
      *GetImmediateAs<DeleteRenderbuffersImmediate>();
  SpecializedSetup<DeleteRenderbuffersImmediate, 0>();
  GLuint temp = kInvalidClientId;
  cmd.Init(1, &temp);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}
// TODO(gman): DeleteShader


TEST_F(GLES2DecoderTest, DeleteTexturesValidArgs) {
  EXPECT_CALL(
      *gl_,
      DeleteTextures(1, Pointee(kServiceTextureId)))
      .Times(1);
  GetSharedMemoryAs<GLuint*>()[0] = client_texture_id_;
  SpecializedSetup<DeleteTextures, 0>();
  DeleteTextures cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GetServiceId(kNewClientId), 0u);
}

TEST_F(GLES2DecoderTest, DeleteTexturesInvalidArgs) {
  EXPECT_CALL(*gl_, DeleteTextures(1, Pointee(0)))
      .Times(1);
  GetSharedMemoryAs<GLuint*>()[0] = kInvalidClientId;
  SpecializedSetup<DeleteTextures, 0>();
  DeleteTextures cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, DeleteTexturesImmediateValidArgs) {
  EXPECT_CALL(
      *gl_,
      DeleteTextures(1, Pointee(kServiceTextureId)))
      .Times(1);
  DeleteTexturesImmediate& cmd = *GetImmediateAs<DeleteTexturesImmediate>();
  SpecializedSetup<DeleteTexturesImmediate, 0>();
  cmd.Init(1, &client_texture_id_);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(client_texture_id_)));
  EXPECT_EQ(GetServiceId(kNewClientId), 0u);
}

TEST_F(GLES2DecoderTest, DeleteTexturesImmediateInvalidArgs) {
  EXPECT_CALL(*gl_, DeleteTextures(1, Pointee(0)))
      .Times(1);
  DeleteTexturesImmediate& cmd = *GetImmediateAs<DeleteTexturesImmediate>();
  SpecializedSetup<DeleteTexturesImmediate, 0>();
  GLuint temp = kInvalidClientId;
  cmd.Init(1, &temp);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderTest, DepthFuncValidArgs) {
  EXPECT_CALL(*gl_, DepthFunc(GL_NEVER));
  SpecializedSetup<DepthFunc, 0>();
  DepthFunc cmd;
  cmd.Init(GL_NEVER);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, DepthMaskValidArgs) {
  EXPECT_CALL(*gl_, DepthMask(1));
  SpecializedSetup<DepthMask, 0>();
  DepthMask cmd;
  cmd.Init(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, DepthRangefValidArgs) {
  EXPECT_CALL(*gl_, DepthRange(1, 2));
  SpecializedSetup<DepthRangef, 0>();
  DepthRangef cmd;
  cmd.Init(1, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, DetachShaderValidArgs) {
  EXPECT_CALL(*gl_, DetachShader(kServiceProgramId, kServiceShaderId));
  SpecializedSetup<DetachShader, 0>();
  DetachShader cmd;
  cmd.Init(client_program_id_, client_shader_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, DisableValidArgs) {
  EXPECT_CALL(*gl_, Disable(GL_BLEND));
  SpecializedSetup<Disable, 0>();
  Disable cmd;
  cmd.Init(GL_BLEND);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, DisableInvalidArgs0_0) {
  EXPECT_CALL(*gl_, Disable(_)).Times(0);
  SpecializedSetup<Disable, 0>();
  Disable cmd;
  cmd.Init(GL_CLIP_PLANE0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, DisableInvalidArgs0_1) {
  EXPECT_CALL(*gl_, Disable(_)).Times(0);
  SpecializedSetup<Disable, 0>();
  Disable cmd;
  cmd.Init(GL_POINT_SPRITE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, DisableVertexAttribArrayValidArgs) {
  EXPECT_CALL(*gl_, DisableVertexAttribArray(1));
  SpecializedSetup<DisableVertexAttribArray, 0>();
  DisableVertexAttribArray cmd;
  cmd.Init(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}
// TODO(gman): DrawArrays
// TODO(gman): DrawElements


TEST_F(GLES2DecoderTest, EnableValidArgs) {
  EXPECT_CALL(*gl_, Enable(GL_BLEND));
  SpecializedSetup<Enable, 0>();
  Enable cmd;
  cmd.Init(GL_BLEND);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, EnableInvalidArgs0_0) {
  EXPECT_CALL(*gl_, Enable(_)).Times(0);
  SpecializedSetup<Enable, 0>();
  Enable cmd;
  cmd.Init(GL_CLIP_PLANE0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, EnableInvalidArgs0_1) {
  EXPECT_CALL(*gl_, Enable(_)).Times(0);
  SpecializedSetup<Enable, 0>();
  Enable cmd;
  cmd.Init(GL_POINT_SPRITE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, EnableVertexAttribArrayValidArgs) {
  EXPECT_CALL(*gl_, EnableVertexAttribArray(1));
  SpecializedSetup<EnableVertexAttribArray, 0>();
  EnableVertexAttribArray cmd;
  cmd.Init(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, FinishValidArgs) {
  EXPECT_CALL(*gl_, Finish());
  SpecializedSetup<Finish, 0>();
  Finish cmd;
  cmd.Init();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, FlushValidArgs) {
  EXPECT_CALL(*gl_, Flush());
  SpecializedSetup<Flush, 0>();
  Flush cmd;
  cmd.Init();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, FramebufferRenderbufferValidArgs) {
  EXPECT_CALL(
      *gl_, FramebufferRenderbufferEXT(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
          kServiceRenderbufferId));
  SpecializedSetup<FramebufferRenderbuffer, 0>();
  FramebufferRenderbuffer cmd;
  cmd.Init(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
      client_renderbuffer_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, FramebufferTexture2DValidArgs) {
  EXPECT_CALL(
      *gl_, FramebufferTexture2DEXT(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
          kServiceTextureId, 5));
  SpecializedSetup<FramebufferTexture2D, 0>();
  FramebufferTexture2D cmd;
  cmd.Init(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, client_texture_id_,
      5);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, FramebufferTexture2DInvalidArgs2_0) {
  EXPECT_CALL(*gl_, FramebufferTexture2DEXT(_, _, _, _, _)).Times(0);
  SpecializedSetup<FramebufferTexture2D, 0>();
  FramebufferTexture2D cmd;
  cmd.Init(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_PROXY_TEXTURE_CUBE_MAP,
      client_texture_id_, 5);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, FrontFaceValidArgs) {
  EXPECT_CALL(*gl_, FrontFace(GL_CW));
  SpecializedSetup<FrontFace, 0>();
  FrontFace cmd;
  cmd.Init(GL_CW);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GenBuffersValidArgs) {
  EXPECT_CALL(*gl_, GenBuffersARB(1, _))
      .WillOnce(SetArgumentPointee<1>(kNewServiceId));
  GetSharedMemoryAs<GLuint*>()[0] = kNewClientId;
  SpecializedSetup<GenBuffers, 0>();
  GenBuffers cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GetServiceId(kNewClientId), kNewServiceId);
}

TEST_F(GLES2DecoderTest, GenBuffersInvalidArgs) {
  EXPECT_CALL(*gl_, GenBuffersARB(_, _)).Times(0);
  GetSharedMemoryAs<GLuint*>()[0] = client_buffer_id_;
  SpecializedSetup<GenBuffers, 0>();
  GenBuffers cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GenBuffersImmediateValidArgs) {
  EXPECT_CALL(*gl_, GenBuffersARB(1, _))
      .WillOnce(SetArgumentPointee<1>(kNewServiceId));
  GenBuffersImmediate& cmd = *GetImmediateAs<GenBuffersImmediate>();
  GLuint temp = kNewClientId;
  SpecializedSetup<GenBuffersImmediate, 0>();
  cmd.Init(1, &temp);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GetServiceId(kNewClientId), kNewServiceId);
}

TEST_F(GLES2DecoderTest, GenBuffersImmediateInvalidArgs) {
  EXPECT_CALL(*gl_, GenBuffersARB(_, _)).Times(0);
  GenBuffersImmediate& cmd = *GetImmediateAs<GenBuffersImmediate>();
  SpecializedSetup<GenBuffersImmediate, 0>();
  cmd.Init(1, &client_buffer_id_);
  EXPECT_EQ(error::kInvalidArguments,
            ExecuteImmediateCmd(cmd, sizeof(&client_buffer_id_)));
}

TEST_F(GLES2DecoderTest, GenerateMipmapValidArgs) {
  EXPECT_CALL(*gl_, GenerateMipmapEXT(GL_TEXTURE_2D));
  SpecializedSetup<GenerateMipmap, 0>();
  GenerateMipmap cmd;
  cmd.Init(GL_TEXTURE_2D);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GenerateMipmapInvalidArgs0_0) {
  EXPECT_CALL(*gl_, GenerateMipmapEXT(_)).Times(0);
  SpecializedSetup<GenerateMipmap, 0>();
  GenerateMipmap cmd;
  cmd.Init(GL_TEXTURE_1D);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GenerateMipmapInvalidArgs0_1) {
  EXPECT_CALL(*gl_, GenerateMipmapEXT(_)).Times(0);
  SpecializedSetup<GenerateMipmap, 0>();
  GenerateMipmap cmd;
  cmd.Init(GL_TEXTURE_3D);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GenFramebuffersValidArgs) {
  EXPECT_CALL(*gl_, GenFramebuffersEXT(1, _))
      .WillOnce(SetArgumentPointee<1>(kNewServiceId));
  GetSharedMemoryAs<GLuint*>()[0] = kNewClientId;
  SpecializedSetup<GenFramebuffers, 0>();
  GenFramebuffers cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GetServiceId(kNewClientId), kNewServiceId);
}

TEST_F(GLES2DecoderTest, GenFramebuffersInvalidArgs) {
  EXPECT_CALL(*gl_, GenFramebuffersEXT(_, _)).Times(0);
  GetSharedMemoryAs<GLuint*>()[0] = client_framebuffer_id_;
  SpecializedSetup<GenFramebuffers, 0>();
  GenFramebuffers cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GenFramebuffersImmediateValidArgs) {
  EXPECT_CALL(*gl_, GenFramebuffersEXT(1, _))
      .WillOnce(SetArgumentPointee<1>(kNewServiceId));
  GenFramebuffersImmediate& cmd = *GetImmediateAs<GenFramebuffersImmediate>();
  GLuint temp = kNewClientId;
  SpecializedSetup<GenFramebuffersImmediate, 0>();
  cmd.Init(1, &temp);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GetServiceId(kNewClientId), kNewServiceId);
}

TEST_F(GLES2DecoderTest, GenFramebuffersImmediateInvalidArgs) {
  EXPECT_CALL(*gl_, GenFramebuffersEXT(_, _)).Times(0);
  GenFramebuffersImmediate& cmd = *GetImmediateAs<GenFramebuffersImmediate>();
  SpecializedSetup<GenFramebuffersImmediate, 0>();
  cmd.Init(1, &client_framebuffer_id_);
  EXPECT_EQ(error::kInvalidArguments,
            ExecuteImmediateCmd(cmd, sizeof(&client_framebuffer_id_)));
}

TEST_F(GLES2DecoderTest, GenRenderbuffersValidArgs) {
  EXPECT_CALL(*gl_, GenRenderbuffersEXT(1, _))
      .WillOnce(SetArgumentPointee<1>(kNewServiceId));
  GetSharedMemoryAs<GLuint*>()[0] = kNewClientId;
  SpecializedSetup<GenRenderbuffers, 0>();
  GenRenderbuffers cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GetServiceId(kNewClientId), kNewServiceId);
}

TEST_F(GLES2DecoderTest, GenRenderbuffersInvalidArgs) {
  EXPECT_CALL(*gl_, GenRenderbuffersEXT(_, _)).Times(0);
  GetSharedMemoryAs<GLuint*>()[0] = client_renderbuffer_id_;
  SpecializedSetup<GenRenderbuffers, 0>();
  GenRenderbuffers cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GenRenderbuffersImmediateValidArgs) {
  EXPECT_CALL(*gl_, GenRenderbuffersEXT(1, _))
      .WillOnce(SetArgumentPointee<1>(kNewServiceId));
  GenRenderbuffersImmediate& cmd =
      *GetImmediateAs<GenRenderbuffersImmediate>();
  GLuint temp = kNewClientId;
  SpecializedSetup<GenRenderbuffersImmediate, 0>();
  cmd.Init(1, &temp);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GetServiceId(kNewClientId), kNewServiceId);
}

TEST_F(GLES2DecoderTest, GenRenderbuffersImmediateInvalidArgs) {
  EXPECT_CALL(*gl_, GenRenderbuffersEXT(_, _)).Times(0);
  GenRenderbuffersImmediate& cmd =
      *GetImmediateAs<GenRenderbuffersImmediate>();
  SpecializedSetup<GenRenderbuffersImmediate, 0>();
  cmd.Init(1, &client_renderbuffer_id_);
  EXPECT_EQ(error::kInvalidArguments,
            ExecuteImmediateCmd(cmd, sizeof(&client_renderbuffer_id_)));
}

TEST_F(GLES2DecoderTest, GenTexturesValidArgs) {
  EXPECT_CALL(*gl_, GenTextures(1, _))
      .WillOnce(SetArgumentPointee<1>(kNewServiceId));
  GetSharedMemoryAs<GLuint*>()[0] = kNewClientId;
  SpecializedSetup<GenTextures, 0>();
  GenTextures cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GetServiceId(kNewClientId), kNewServiceId);
}

TEST_F(GLES2DecoderTest, GenTexturesInvalidArgs) {
  EXPECT_CALL(*gl_, GenTextures(_, _)).Times(0);
  GetSharedMemoryAs<GLuint*>()[0] = client_texture_id_;
  SpecializedSetup<GenTextures, 0>();
  GenTextures cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GenTexturesImmediateValidArgs) {
  EXPECT_CALL(*gl_, GenTextures(1, _))
      .WillOnce(SetArgumentPointee<1>(kNewServiceId));
  GenTexturesImmediate& cmd = *GetImmediateAs<GenTexturesImmediate>();
  GLuint temp = kNewClientId;
  SpecializedSetup<GenTexturesImmediate, 0>();
  cmd.Init(1, &temp);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GetServiceId(kNewClientId), kNewServiceId);
}

TEST_F(GLES2DecoderTest, GenTexturesImmediateInvalidArgs) {
  EXPECT_CALL(*gl_, GenTextures(_, _)).Times(0);
  GenTexturesImmediate& cmd = *GetImmediateAs<GenTexturesImmediate>();
  SpecializedSetup<GenTexturesImmediate, 0>();
  cmd.Init(1, &client_texture_id_);
  EXPECT_EQ(error::kInvalidArguments,
            ExecuteImmediateCmd(cmd, sizeof(&client_texture_id_)));
}
// TODO(gman): GetActiveAttrib

// TODO(gman): GetActiveUniform

// TODO(gman): GetAttachedShaders

// TODO(gman): GetAttribLocation

// TODO(gman): GetAttribLocationImmediate


TEST_F(GLES2DecoderTest, GetBooleanvValidArgs) {
  EXPECT_CALL(
      *gl_, GetBooleanv(
          1, reinterpret_cast<GLboolean*>(shared_memory_address_)));
  SpecializedSetup<GetBooleanv, 0>();
  GetBooleanv cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetBooleanvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, GetBooleanv(_, _)).Times(0);
  SpecializedSetup<GetBooleanv, 0>();
  GetBooleanv cmd;
  cmd.Init(1, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetBooleanvInvalidArgs1_1) {
  EXPECT_CALL(*gl_, GetBooleanv(_, _)).Times(0);
  SpecializedSetup<GetBooleanv, 0>();
  GetBooleanv cmd;
  cmd.Init(1, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetBufferParameterivValidArgs) {
  EXPECT_CALL(
      *gl_, GetBufferParameteriv(
          GL_ARRAY_BUFFER, GL_BUFFER_SIZE, reinterpret_cast<GLint*>(
              shared_memory_address_)));
  SpecializedSetup<GetBufferParameteriv, 0>();
  GetBufferParameteriv cmd;
  cmd.Init(
      GL_ARRAY_BUFFER, GL_BUFFER_SIZE, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetBufferParameterivInvalidArgs0_0) {
  EXPECT_CALL(*gl_, GetBufferParameteriv(_, _, _)).Times(0);
  SpecializedSetup<GetBufferParameteriv, 0>();
  GetBufferParameteriv cmd;
  cmd.Init(
      GL_RENDERBUFFER, GL_BUFFER_SIZE, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetBufferParameterivInvalidArgs1_0) {
  EXPECT_CALL(*gl_, GetBufferParameteriv(_, _, _)).Times(0);
  SpecializedSetup<GetBufferParameteriv, 0>();
  GetBufferParameteriv cmd;
  cmd.Init(
      GL_ARRAY_BUFFER, GL_PIXEL_PACK_BUFFER, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetBufferParameterivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, GetBufferParameteriv(_, _, _)).Times(0);
  SpecializedSetup<GetBufferParameteriv, 0>();
  GetBufferParameteriv cmd;
  cmd.Init(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetBufferParameterivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, GetBufferParameteriv(_, _, _)).Times(0);
  SpecializedSetup<GetBufferParameteriv, 0>();
  GetBufferParameteriv cmd;
  cmd.Init(
      GL_ARRAY_BUFFER, GL_BUFFER_SIZE, shared_memory_id_,
      kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetErrorValidArgs) {
  EXPECT_CALL(*gl_, GetError());
  SpecializedSetup<GetError, 0>();
  GetError cmd;
  cmd.Init(shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetFloatvValidArgs) {
  EXPECT_CALL(
      *gl_, GetFloatv(1, reinterpret_cast<GLfloat*>(shared_memory_address_)));
  SpecializedSetup<GetFloatv, 0>();
  GetFloatv cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetFloatvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, GetFloatv(_, _)).Times(0);
  SpecializedSetup<GetFloatv, 0>();
  GetFloatv cmd;
  cmd.Init(1, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetFloatvInvalidArgs1_1) {
  EXPECT_CALL(*gl_, GetFloatv(_, _)).Times(0);
  SpecializedSetup<GetFloatv, 0>();
  GetFloatv cmd;
  cmd.Init(1, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetFramebufferAttachmentParameterivValidArgs) {
  EXPECT_CALL(
      *gl_, GetFramebufferAttachmentParameterivEXT(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, reinterpret_cast<GLint*>(
              shared_memory_address_)));
  SpecializedSetup<GetFramebufferAttachmentParameteriv, 0>();
  GetFramebufferAttachmentParameteriv cmd;
  cmd.Init(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
      GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetFramebufferAttachmentParameterivInvalidArgs3_0) {
  EXPECT_CALL(
      *gl_, GetFramebufferAttachmentParameterivEXT(_, _, _, _)).Times(0);
  SpecializedSetup<GetFramebufferAttachmentParameteriv, 0>();
  GetFramebufferAttachmentParameteriv cmd;
  cmd.Init(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
      GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetFramebufferAttachmentParameterivInvalidArgs3_1) {
  EXPECT_CALL(
      *gl_, GetFramebufferAttachmentParameterivEXT(_, _, _, _)).Times(0);
  SpecializedSetup<GetFramebufferAttachmentParameteriv, 0>();
  GetFramebufferAttachmentParameteriv cmd;
  cmd.Init(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
      GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, shared_memory_id_,
      kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetIntegervValidArgs) {
  EXPECT_CALL(
      *gl_, GetIntegerv(1, reinterpret_cast<GLint*>(shared_memory_address_)));
  SpecializedSetup<GetIntegerv, 0>();
  GetIntegerv cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetIntegervInvalidArgs1_0) {
  EXPECT_CALL(*gl_, GetIntegerv(_, _)).Times(0);
  SpecializedSetup<GetIntegerv, 0>();
  GetIntegerv cmd;
  cmd.Init(1, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetIntegervInvalidArgs1_1) {
  EXPECT_CALL(*gl_, GetIntegerv(_, _)).Times(0);
  SpecializedSetup<GetIntegerv, 0>();
  GetIntegerv cmd;
  cmd.Init(1, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetProgramivValidArgs) {
  EXPECT_CALL(
      *gl_, GetProgramiv(
          kServiceProgramId, GL_DELETE_STATUS, reinterpret_cast<GLint*>(
              shared_memory_address_)));
  SpecializedSetup<GetProgramiv, 0>();
  GetProgramiv cmd;
  cmd.Init(
      client_program_id_, GL_DELETE_STATUS, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetProgramivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, GetProgramiv(_, _, _)).Times(0);
  SpecializedSetup<GetProgramiv, 0>();
  GetProgramiv cmd;
  cmd.Init(client_program_id_, GL_DELETE_STATUS, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetProgramivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, GetProgramiv(_, _, _)).Times(0);
  SpecializedSetup<GetProgramiv, 0>();
  GetProgramiv cmd;
  cmd.Init(
      client_program_id_, GL_DELETE_STATUS, shared_memory_id_,
      kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}
// TODO(gman): GetProgramInfoLog


TEST_F(GLES2DecoderTest, GetRenderbufferParameterivValidArgs) {
  EXPECT_CALL(
      *gl_, GetRenderbufferParameterivEXT(
          GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, reinterpret_cast<GLint*>(
              shared_memory_address_)));
  SpecializedSetup<GetRenderbufferParameteriv, 0>();
  GetRenderbufferParameteriv cmd;
  cmd.Init(
      GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetRenderbufferParameterivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, GetRenderbufferParameterivEXT(_, _, _)).Times(0);
  SpecializedSetup<GetRenderbufferParameteriv, 0>();
  GetRenderbufferParameteriv cmd;
  cmd.Init(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetRenderbufferParameterivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, GetRenderbufferParameterivEXT(_, _, _)).Times(0);
  SpecializedSetup<GetRenderbufferParameteriv, 0>();
  GetRenderbufferParameteriv cmd;
  cmd.Init(
      GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, shared_memory_id_,
      kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetShaderivValidArgs) {
  EXPECT_CALL(
      *gl_, GetShaderiv(
          kServiceShaderId, GL_SHADER_TYPE, reinterpret_cast<GLint*>(
              shared_memory_address_)));
  SpecializedSetup<GetShaderiv, 0>();
  GetShaderiv cmd;
  cmd.Init(
      client_shader_id_, GL_SHADER_TYPE, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetShaderivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, GetShaderiv(_, _, _)).Times(0);
  SpecializedSetup<GetShaderiv, 0>();
  GetShaderiv cmd;
  cmd.Init(client_shader_id_, GL_SHADER_TYPE, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetShaderivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, GetShaderiv(_, _, _)).Times(0);
  SpecializedSetup<GetShaderiv, 0>();
  GetShaderiv cmd;
  cmd.Init(
      client_shader_id_, GL_SHADER_TYPE, shared_memory_id_,
      kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}
// TODO(gman): GetShaderInfoLog

// TODO(gman): GetShaderPrecisionFormat

// TODO(gman): GetShaderSource


TEST_F(GLES2DecoderTest, GetStringValidArgs) {
  EXPECT_CALL(*gl_, GetString(GL_VENDOR));
  SpecializedSetup<GetString, 0>();
  GetString cmd;
  cmd.Init(GL_VENDOR);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetTexParameterfvValidArgs) {
  EXPECT_CALL(
      *gl_, GetTexParameterfv(
          GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, reinterpret_cast<GLfloat*>(
              shared_memory_address_)));
  SpecializedSetup<GetTexParameterfv, 0>();
  GetTexParameterfv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetTexParameterfvInvalidArgs0_0) {
  EXPECT_CALL(*gl_, GetTexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<GetTexParameterfv, 0>();
  GetTexParameterfv cmd;
  cmd.Init(
      GL_PROXY_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetTexParameterfvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, GetTexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<GetTexParameterfv, 0>();
  GetTexParameterfv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_GENERATE_MIPMAP, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetTexParameterfvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, GetTexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<GetTexParameterfv, 0>();
  GetTexParameterfv cmd;
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetTexParameterfvInvalidArgs2_1) {
  EXPECT_CALL(*gl_, GetTexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<GetTexParameterfv, 0>();
  GetTexParameterfv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetTexParameterivValidArgs) {
  EXPECT_CALL(
      *gl_, GetTexParameteriv(
          GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, reinterpret_cast<GLint*>(
              shared_memory_address_)));
  SpecializedSetup<GetTexParameteriv, 0>();
  GetTexParameteriv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetTexParameterivInvalidArgs0_0) {
  EXPECT_CALL(*gl_, GetTexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<GetTexParameteriv, 0>();
  GetTexParameteriv cmd;
  cmd.Init(
      GL_PROXY_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetTexParameterivInvalidArgs1_0) {
  EXPECT_CALL(*gl_, GetTexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<GetTexParameteriv, 0>();
  GetTexParameteriv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_GENERATE_MIPMAP, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetTexParameterivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, GetTexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<GetTexParameteriv, 0>();
  GetTexParameteriv cmd;
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetTexParameterivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, GetTexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<GetTexParameteriv, 0>();
  GetTexParameteriv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}
// TODO(gman): GetUniformfv

// TODO(gman): GetUniformiv

// TODO(gman): GetUniformLocation

// TODO(gman): GetUniformLocationImmediate


TEST_F(GLES2DecoderTest, GetVertexAttribfvValidArgs) {
  EXPECT_CALL(
      *gl_, GetVertexAttribfv(
          1, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, reinterpret_cast<GLfloat*>(
              shared_memory_address_)));
  SpecializedSetup<GetVertexAttribfv, 0>();
  GetVertexAttribfv cmd;
  cmd.Init(
      1, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetVertexAttribfvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, GetVertexAttribfv(_, _, _)).Times(0);
  SpecializedSetup<GetVertexAttribfv, 0>();
  GetVertexAttribfv cmd;
  cmd.Init(
      1, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetVertexAttribfvInvalidArgs2_1) {
  EXPECT_CALL(*gl_, GetVertexAttribfv(_, _, _)).Times(0);
  SpecializedSetup<GetVertexAttribfv, 0>();
  GetVertexAttribfv cmd;
  cmd.Init(
      1, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, shared_memory_id_,
      kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetVertexAttribivValidArgs) {
  EXPECT_CALL(
      *gl_, GetVertexAttribiv(
          1, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, reinterpret_cast<GLint*>(
              shared_memory_address_)));
  SpecializedSetup<GetVertexAttribiv, 0>();
  GetVertexAttribiv cmd;
  cmd.Init(
      1, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetVertexAttribivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, GetVertexAttribiv(_, _, _)).Times(0);
  SpecializedSetup<GetVertexAttribiv, 0>();
  GetVertexAttribiv cmd;
  cmd.Init(
      1, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, GetVertexAttribivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, GetVertexAttribiv(_, _, _)).Times(0);
  SpecializedSetup<GetVertexAttribiv, 0>();
  GetVertexAttribiv cmd;
  cmd.Init(
      1, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, shared_memory_id_,
      kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}
// TODO(gman): GetVertexAttribPointerv


TEST_F(GLES2DecoderTest, HintValidArgs) {
  EXPECT_CALL(*gl_, Hint(GL_GENERATE_MIPMAP_HINT, GL_FASTEST));
  SpecializedSetup<Hint, 0>();
  Hint cmd;
  cmd.Init(GL_GENERATE_MIPMAP_HINT, GL_FASTEST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, HintInvalidArgs0_0) {
  EXPECT_CALL(*gl_, Hint(_, _)).Times(0);
  SpecializedSetup<Hint, 0>();
  Hint cmd;
  cmd.Init(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, IsBufferValidArgs) {
  EXPECT_CALL(*gl_, IsBuffer(kServiceBufferId));
  SpecializedSetup<IsBuffer, 0>();
  IsBuffer cmd;
  cmd.Init(client_buffer_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, IsEnabledValidArgs) {
  EXPECT_CALL(*gl_, IsEnabled(GL_BLEND));
  SpecializedSetup<IsEnabled, 0>();
  IsEnabled cmd;
  cmd.Init(GL_BLEND, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, IsEnabledInvalidArgs0_0) {
  EXPECT_CALL(*gl_, IsEnabled(_)).Times(0);
  SpecializedSetup<IsEnabled, 0>();
  IsEnabled cmd;
  cmd.Init(GL_CLIP_PLANE0, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, IsEnabledInvalidArgs0_1) {
  EXPECT_CALL(*gl_, IsEnabled(_)).Times(0);
  SpecializedSetup<IsEnabled, 0>();
  IsEnabled cmd;
  cmd.Init(GL_POINT_SPRITE, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, IsFramebufferValidArgs) {
  EXPECT_CALL(*gl_, IsFramebufferEXT(kServiceFramebufferId));
  SpecializedSetup<IsFramebuffer, 0>();
  IsFramebuffer cmd;
  cmd.Init(client_framebuffer_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, IsProgramValidArgs) {
  EXPECT_CALL(*gl_, IsProgram(kServiceProgramId));
  SpecializedSetup<IsProgram, 0>();
  IsProgram cmd;
  cmd.Init(client_program_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, IsRenderbufferValidArgs) {
  EXPECT_CALL(*gl_, IsRenderbufferEXT(kServiceRenderbufferId));
  SpecializedSetup<IsRenderbuffer, 0>();
  IsRenderbuffer cmd;
  cmd.Init(client_renderbuffer_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, IsShaderValidArgs) {
  EXPECT_CALL(*gl_, IsShader(kServiceShaderId));
  SpecializedSetup<IsShader, 0>();
  IsShader cmd;
  cmd.Init(client_shader_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, IsTextureValidArgs) {
  EXPECT_CALL(*gl_, IsTexture(kServiceTextureId));
  SpecializedSetup<IsTexture, 0>();
  IsTexture cmd;
  cmd.Init(client_texture_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, LineWidthValidArgs) {
  EXPECT_CALL(*gl_, LineWidth(1));
  SpecializedSetup<LineWidth, 0>();
  LineWidth cmd;
  cmd.Init(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, LinkProgramValidArgs) {
  EXPECT_CALL(*gl_, LinkProgram(kServiceProgramId));
  SpecializedSetup<LinkProgram, 0>();
  LinkProgram cmd;
  cmd.Init(client_program_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}
// TODO(gman): PixelStorei


TEST_F(GLES2DecoderTest, PolygonOffsetValidArgs) {
  EXPECT_CALL(*gl_, PolygonOffset(1, 2));
  SpecializedSetup<PolygonOffset, 0>();
  PolygonOffset cmd;
  cmd.Init(1, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}
// TODO(gman): ReadPixels


TEST_F(GLES2DecoderTest, RenderbufferStorageValidArgs) {
  EXPECT_CALL(*gl_, RenderbufferStorageEXT(GL_RENDERBUFFER, GL_RGBA4, 3, 4));
  SpecializedSetup<RenderbufferStorage, 0>();
  RenderbufferStorage cmd;
  cmd.Init(GL_RENDERBUFFER, GL_RGBA4, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, SampleCoverageValidArgs) {
  EXPECT_CALL(*gl_, SampleCoverage(1, 2));
  SpecializedSetup<SampleCoverage, 0>();
  SampleCoverage cmd;
  cmd.Init(1, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, ScissorValidArgs) {
  EXPECT_CALL(*gl_, Scissor(1, 2, 3, 4));
  SpecializedSetup<Scissor, 0>();
  Scissor cmd;
  cmd.Init(1, 2, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}
// TODO(gman): ShaderSource

// TODO(gman): ShaderSourceImmediate


TEST_F(GLES2DecoderTest, StencilFuncValidArgs) {
  EXPECT_CALL(*gl_, StencilFunc(GL_NEVER, 2, 3));
  SpecializedSetup<StencilFunc, 0>();
  StencilFunc cmd;
  cmd.Init(GL_NEVER, 2, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, StencilFuncSeparateValidArgs) {
  EXPECT_CALL(*gl_, StencilFuncSeparate(GL_FRONT, GL_NEVER, 3, 4));
  SpecializedSetup<StencilFuncSeparate, 0>();
  StencilFuncSeparate cmd;
  cmd.Init(GL_FRONT, GL_NEVER, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, StencilMaskValidArgs) {
  EXPECT_CALL(*gl_, StencilMask(1));
  SpecializedSetup<StencilMask, 0>();
  StencilMask cmd;
  cmd.Init(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, StencilMaskSeparateValidArgs) {
  EXPECT_CALL(*gl_, StencilMaskSeparate(GL_FRONT, 2));
  SpecializedSetup<StencilMaskSeparate, 0>();
  StencilMaskSeparate cmd;
  cmd.Init(GL_FRONT, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, StencilOpValidArgs) {
  EXPECT_CALL(*gl_, StencilOp(GL_KEEP, GL_KEEP, GL_KEEP));
  SpecializedSetup<StencilOp, 0>();
  StencilOp cmd;
  cmd.Init(GL_KEEP, GL_KEEP, GL_KEEP);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, StencilOpSeparateValidArgs) {
  EXPECT_CALL(*gl_, StencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_KEEP));
  SpecializedSetup<StencilOpSeparate, 0>();
  StencilOpSeparate cmd;
  cmd.Init(GL_FRONT, GL_KEEP, GL_KEEP, GL_KEEP);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}
// TODO(gman): TexImage2D

// TODO(gman): TexImage2DImmediate


TEST_F(GLES2DecoderTest, TexParameterfValidArgs) {
  EXPECT_CALL(*gl_, TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 3));
  SpecializedSetup<TexParameterf, 0>();
  TexParameterf cmd;
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, TexParameterfInvalidArgs0_0) {
  EXPECT_CALL(*gl_, TexParameterf(_, _, _)).Times(0);
  SpecializedSetup<TexParameterf, 0>();
  TexParameterf cmd;
  cmd.Init(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, TexParameterfInvalidArgs0_1) {
  EXPECT_CALL(*gl_, TexParameterf(_, _, _)).Times(0);
  SpecializedSetup<TexParameterf, 0>();
  TexParameterf cmd;
  cmd.Init(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, TexParameterfInvalidArgs1_0) {
  EXPECT_CALL(*gl_, TexParameterf(_, _, _)).Times(0);
  SpecializedSetup<TexParameterf, 0>();
  TexParameterf cmd;
  cmd.Init(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, TexParameterfvValidArgs) {
  EXPECT_CALL(
      *gl_, TexParameterfv(
          GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
          reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<TexParameterfv, 0>();
  TexParameterfv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, TexParameterfvInvalidArgs0_0) {
  EXPECT_CALL(*gl_, TexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<TexParameterfv, 0>();
  TexParameterfv cmd;
  cmd.Init(
      GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, TexParameterfvInvalidArgs0_1) {
  EXPECT_CALL(*gl_, TexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<TexParameterfv, 0>();
  TexParameterfv cmd;
  cmd.Init(
      GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, TexParameterfvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, TexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<TexParameterfv, 0>();
  TexParameterfv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_GENERATE_MIPMAP, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, TexParameterfvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, TexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<TexParameterfv, 0>();
  TexParameterfv cmd;
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, TexParameterfvInvalidArgs2_1) {
  EXPECT_CALL(*gl_, TexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<TexParameterfv, 0>();
  TexParameterfv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, TexParameterfvImmediateValidArgs) {
  TexParameterfvImmediate& cmd = *GetImmediateAs<TexParameterfvImmediate>();
  EXPECT_CALL(
      *gl_,
      TexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<TexParameterfvImmediate, 0>();
  GLfloat temp[1] = { 0, };
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderTest, TexParameterfvImmediateInvalidArgs0_0) {
  TexParameterfvImmediate& cmd = *GetImmediateAs<TexParameterfvImmediate>();
  EXPECT_CALL(*gl_, TexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<TexParameterfvImmediate, 0>();
  GLfloat temp[1] = { 0, };
  cmd.Init(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderTest, TexParameterfvImmediateInvalidArgs0_1) {
  TexParameterfvImmediate& cmd = *GetImmediateAs<TexParameterfvImmediate>();
  EXPECT_CALL(*gl_, TexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<TexParameterfvImmediate, 0>();
  GLfloat temp[1] = { 0, };
  cmd.Init(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderTest, TexParameterfvImmediateInvalidArgs1_0) {
  TexParameterfvImmediate& cmd = *GetImmediateAs<TexParameterfvImmediate>();
  EXPECT_CALL(*gl_, TexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<TexParameterfvImmediate, 0>();
  GLfloat temp[1] = { 0, };
  cmd.Init(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderTest, TexParameteriValidArgs) {
  EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 3));
  SpecializedSetup<TexParameteri, 0>();
  TexParameteri cmd;
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, TexParameteriInvalidArgs0_0) {
  EXPECT_CALL(*gl_, TexParameteri(_, _, _)).Times(0);
  SpecializedSetup<TexParameteri, 0>();
  TexParameteri cmd;
  cmd.Init(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, TexParameteriInvalidArgs0_1) {
  EXPECT_CALL(*gl_, TexParameteri(_, _, _)).Times(0);
  SpecializedSetup<TexParameteri, 0>();
  TexParameteri cmd;
  cmd.Init(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, TexParameteriInvalidArgs1_0) {
  EXPECT_CALL(*gl_, TexParameteri(_, _, _)).Times(0);
  SpecializedSetup<TexParameteri, 0>();
  TexParameteri cmd;
  cmd.Init(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, TexParameterivValidArgs) {
  EXPECT_CALL(
      *gl_, TexParameteriv(
          GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, reinterpret_cast<const GLint*>(
              shared_memory_address_)));
  SpecializedSetup<TexParameteriv, 0>();
  TexParameteriv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, TexParameterivInvalidArgs0_0) {
  EXPECT_CALL(*gl_, TexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<TexParameteriv, 0>();
  TexParameteriv cmd;
  cmd.Init(
      GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, TexParameterivInvalidArgs0_1) {
  EXPECT_CALL(*gl_, TexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<TexParameteriv, 0>();
  TexParameteriv cmd;
  cmd.Init(
      GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, TexParameterivInvalidArgs1_0) {
  EXPECT_CALL(*gl_, TexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<TexParameteriv, 0>();
  TexParameteriv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_GENERATE_MIPMAP, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, TexParameterivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, TexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<TexParameteriv, 0>();
  TexParameteriv cmd;
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, TexParameterivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, TexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<TexParameteriv, 0>();
  TexParameteriv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, TexParameterivImmediateValidArgs) {
  TexParameterivImmediate& cmd = *GetImmediateAs<TexParameterivImmediate>();
  EXPECT_CALL(
      *gl_,
      TexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
          reinterpret_cast<GLint*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<TexParameterivImmediate, 0>();
  GLint temp[1] = { 0, };
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderTest, TexParameterivImmediateInvalidArgs0_0) {
  TexParameterivImmediate& cmd = *GetImmediateAs<TexParameterivImmediate>();
  EXPECT_CALL(*gl_, TexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<TexParameterivImmediate, 0>();
  GLint temp[1] = { 0, };
  cmd.Init(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderTest, TexParameterivImmediateInvalidArgs0_1) {
  TexParameterivImmediate& cmd = *GetImmediateAs<TexParameterivImmediate>();
  EXPECT_CALL(*gl_, TexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<TexParameterivImmediate, 0>();
  GLint temp[1] = { 0, };
  cmd.Init(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderTest, TexParameterivImmediateInvalidArgs1_0) {
  TexParameterivImmediate& cmd = *GetImmediateAs<TexParameterivImmediate>();
  EXPECT_CALL(*gl_, TexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<TexParameterivImmediate, 0>();
  GLint temp[1] = { 0, };
  cmd.Init(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}
// TODO(gman): TexSubImage2D

// TODO(gman): TexSubImage2DImmediate


TEST_F(GLES2DecoderTest, Uniform1fValidArgs) {
  EXPECT_CALL(*gl_, Uniform1f(1, 2));
  SpecializedSetup<Uniform1f, 0>();
  Uniform1f cmd;
  cmd.Init(1, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform1fvValidArgs) {
  EXPECT_CALL(
      *gl_, Uniform1fv(
          1, 2, reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<Uniform1fv, 0>();
  Uniform1fv cmd;
  cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform1fvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Uniform1fv(_, _, _)).Times(0);
  SpecializedSetup<Uniform1fv, 0>();
  Uniform1fv cmd;
  cmd.Init(1, 2, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform1fvInvalidArgs2_1) {
  EXPECT_CALL(*gl_, Uniform1fv(_, _, _)).Times(0);
  SpecializedSetup<Uniform1fv, 0>();
  Uniform1fv cmd;
  cmd.Init(1, 2, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform1fvImmediateValidArgs) {
  Uniform1fvImmediate& cmd = *GetImmediateAs<Uniform1fvImmediate>();
  EXPECT_CALL(
      *gl_,
      Uniform1fv(1, 2,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<Uniform1fvImmediate, 0>();
  GLfloat temp[1 * 2] = { 0, };
  cmd.Init(1, 2, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderTest, Uniform1iValidArgs) {
  EXPECT_CALL(*gl_, Uniform1i(1, 2));
  SpecializedSetup<Uniform1i, 0>();
  Uniform1i cmd;
  cmd.Init(1, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform1ivValidArgs) {
  EXPECT_CALL(
      *gl_, Uniform1iv(
          1, 2, reinterpret_cast<const GLint*>(shared_memory_address_)));
  SpecializedSetup<Uniform1iv, 0>();
  Uniform1iv cmd;
  cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform1ivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Uniform1iv(_, _, _)).Times(0);
  SpecializedSetup<Uniform1iv, 0>();
  Uniform1iv cmd;
  cmd.Init(1, 2, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform1ivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, Uniform1iv(_, _, _)).Times(0);
  SpecializedSetup<Uniform1iv, 0>();
  Uniform1iv cmd;
  cmd.Init(1, 2, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform1ivImmediateValidArgs) {
  Uniform1ivImmediate& cmd = *GetImmediateAs<Uniform1ivImmediate>();
  EXPECT_CALL(
      *gl_,
      Uniform1iv(1, 2,
          reinterpret_cast<GLint*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<Uniform1ivImmediate, 0>();
  GLint temp[1 * 2] = { 0, };
  cmd.Init(1, 2, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderTest, Uniform2fValidArgs) {
  EXPECT_CALL(*gl_, Uniform2f(1, 2, 3));
  SpecializedSetup<Uniform2f, 0>();
  Uniform2f cmd;
  cmd.Init(1, 2, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform2fvValidArgs) {
  EXPECT_CALL(
      *gl_, Uniform2fv(
          1, 2, reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<Uniform2fv, 0>();
  Uniform2fv cmd;
  cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform2fvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Uniform2fv(_, _, _)).Times(0);
  SpecializedSetup<Uniform2fv, 0>();
  Uniform2fv cmd;
  cmd.Init(1, 2, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform2fvInvalidArgs2_1) {
  EXPECT_CALL(*gl_, Uniform2fv(_, _, _)).Times(0);
  SpecializedSetup<Uniform2fv, 0>();
  Uniform2fv cmd;
  cmd.Init(1, 2, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform2fvImmediateValidArgs) {
  Uniform2fvImmediate& cmd = *GetImmediateAs<Uniform2fvImmediate>();
  EXPECT_CALL(
      *gl_,
      Uniform2fv(1, 2,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<Uniform2fvImmediate, 0>();
  GLfloat temp[2 * 2] = { 0, };
  cmd.Init(1, 2, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderTest, Uniform2iValidArgs) {
  EXPECT_CALL(*gl_, Uniform2i(1, 2, 3));
  SpecializedSetup<Uniform2i, 0>();
  Uniform2i cmd;
  cmd.Init(1, 2, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform2ivValidArgs) {
  EXPECT_CALL(
      *gl_, Uniform2iv(
          1, 2, reinterpret_cast<const GLint*>(shared_memory_address_)));
  SpecializedSetup<Uniform2iv, 0>();
  Uniform2iv cmd;
  cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform2ivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Uniform2iv(_, _, _)).Times(0);
  SpecializedSetup<Uniform2iv, 0>();
  Uniform2iv cmd;
  cmd.Init(1, 2, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform2ivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, Uniform2iv(_, _, _)).Times(0);
  SpecializedSetup<Uniform2iv, 0>();
  Uniform2iv cmd;
  cmd.Init(1, 2, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform2ivImmediateValidArgs) {
  Uniform2ivImmediate& cmd = *GetImmediateAs<Uniform2ivImmediate>();
  EXPECT_CALL(
      *gl_,
      Uniform2iv(1, 2,
          reinterpret_cast<GLint*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<Uniform2ivImmediate, 0>();
  GLint temp[2 * 2] = { 0, };
  cmd.Init(1, 2, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderTest, Uniform3fValidArgs) {
  EXPECT_CALL(*gl_, Uniform3f(1, 2, 3, 4));
  SpecializedSetup<Uniform3f, 0>();
  Uniform3f cmd;
  cmd.Init(1, 2, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform3fvValidArgs) {
  EXPECT_CALL(
      *gl_, Uniform3fv(
          1, 2, reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<Uniform3fv, 0>();
  Uniform3fv cmd;
  cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform3fvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Uniform3fv(_, _, _)).Times(0);
  SpecializedSetup<Uniform3fv, 0>();
  Uniform3fv cmd;
  cmd.Init(1, 2, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform3fvInvalidArgs2_1) {
  EXPECT_CALL(*gl_, Uniform3fv(_, _, _)).Times(0);
  SpecializedSetup<Uniform3fv, 0>();
  Uniform3fv cmd;
  cmd.Init(1, 2, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform3fvImmediateValidArgs) {
  Uniform3fvImmediate& cmd = *GetImmediateAs<Uniform3fvImmediate>();
  EXPECT_CALL(
      *gl_,
      Uniform3fv(1, 2,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<Uniform3fvImmediate, 0>();
  GLfloat temp[3 * 2] = { 0, };
  cmd.Init(1, 2, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderTest, Uniform3iValidArgs) {
  EXPECT_CALL(*gl_, Uniform3i(1, 2, 3, 4));
  SpecializedSetup<Uniform3i, 0>();
  Uniform3i cmd;
  cmd.Init(1, 2, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform3ivValidArgs) {
  EXPECT_CALL(
      *gl_, Uniform3iv(
          1, 2, reinterpret_cast<const GLint*>(shared_memory_address_)));
  SpecializedSetup<Uniform3iv, 0>();
  Uniform3iv cmd;
  cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform3ivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Uniform3iv(_, _, _)).Times(0);
  SpecializedSetup<Uniform3iv, 0>();
  Uniform3iv cmd;
  cmd.Init(1, 2, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform3ivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, Uniform3iv(_, _, _)).Times(0);
  SpecializedSetup<Uniform3iv, 0>();
  Uniform3iv cmd;
  cmd.Init(1, 2, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform3ivImmediateValidArgs) {
  Uniform3ivImmediate& cmd = *GetImmediateAs<Uniform3ivImmediate>();
  EXPECT_CALL(
      *gl_,
      Uniform3iv(1, 2,
          reinterpret_cast<GLint*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<Uniform3ivImmediate, 0>();
  GLint temp[3 * 2] = { 0, };
  cmd.Init(1, 2, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderTest, Uniform4fValidArgs) {
  EXPECT_CALL(*gl_, Uniform4f(1, 2, 3, 4, 5));
  SpecializedSetup<Uniform4f, 0>();
  Uniform4f cmd;
  cmd.Init(1, 2, 3, 4, 5);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform4fvValidArgs) {
  EXPECT_CALL(
      *gl_, Uniform4fv(
          1, 2, reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<Uniform4fv, 0>();
  Uniform4fv cmd;
  cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform4fvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Uniform4fv(_, _, _)).Times(0);
  SpecializedSetup<Uniform4fv, 0>();
  Uniform4fv cmd;
  cmd.Init(1, 2, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform4fvInvalidArgs2_1) {
  EXPECT_CALL(*gl_, Uniform4fv(_, _, _)).Times(0);
  SpecializedSetup<Uniform4fv, 0>();
  Uniform4fv cmd;
  cmd.Init(1, 2, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform4fvImmediateValidArgs) {
  Uniform4fvImmediate& cmd = *GetImmediateAs<Uniform4fvImmediate>();
  EXPECT_CALL(
      *gl_,
      Uniform4fv(1, 2,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<Uniform4fvImmediate, 0>();
  GLfloat temp[4 * 2] = { 0, };
  cmd.Init(1, 2, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderTest, Uniform4iValidArgs) {
  EXPECT_CALL(*gl_, Uniform4i(1, 2, 3, 4, 5));
  SpecializedSetup<Uniform4i, 0>();
  Uniform4i cmd;
  cmd.Init(1, 2, 3, 4, 5);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform4ivValidArgs) {
  EXPECT_CALL(
      *gl_, Uniform4iv(
          1, 2, reinterpret_cast<const GLint*>(shared_memory_address_)));
  SpecializedSetup<Uniform4iv, 0>();
  Uniform4iv cmd;
  cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform4ivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Uniform4iv(_, _, _)).Times(0);
  SpecializedSetup<Uniform4iv, 0>();
  Uniform4iv cmd;
  cmd.Init(1, 2, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform4ivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, Uniform4iv(_, _, _)).Times(0);
  SpecializedSetup<Uniform4iv, 0>();
  Uniform4iv cmd;
  cmd.Init(1, 2, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, Uniform4ivImmediateValidArgs) {
  Uniform4ivImmediate& cmd = *GetImmediateAs<Uniform4ivImmediate>();
  EXPECT_CALL(
      *gl_,
      Uniform4iv(1, 2,
          reinterpret_cast<GLint*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<Uniform4ivImmediate, 0>();
  GLint temp[4 * 2] = { 0, };
  cmd.Init(1, 2, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderTest, UniformMatrix2fvValidArgs) {
  EXPECT_CALL(
      *gl_, UniformMatrix2fv(
          1, 2, false, reinterpret_cast<const GLfloat*>(
              shared_memory_address_)));
  SpecializedSetup<UniformMatrix2fv, 0>();
  UniformMatrix2fv cmd;
  cmd.Init(1, 2, false, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, UniformMatrix2fvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, UniformMatrix2fv(_, _, _, _)).Times(0);
  SpecializedSetup<UniformMatrix2fv, 0>();
  UniformMatrix2fv cmd;
  cmd.Init(1, 2, true, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, UniformMatrix2fvInvalidArgs3_0) {
  EXPECT_CALL(*gl_, UniformMatrix2fv(_, _, _, _)).Times(0);
  SpecializedSetup<UniformMatrix2fv, 0>();
  UniformMatrix2fv cmd;
  cmd.Init(1, 2, false, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, UniformMatrix2fvInvalidArgs3_1) {
  EXPECT_CALL(*gl_, UniformMatrix2fv(_, _, _, _)).Times(0);
  SpecializedSetup<UniformMatrix2fv, 0>();
  UniformMatrix2fv cmd;
  cmd.Init(1, 2, false, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, UniformMatrix2fvImmediateValidArgs) {
  UniformMatrix2fvImmediate& cmd =
      *GetImmediateAs<UniformMatrix2fvImmediate>();
  EXPECT_CALL(
      *gl_,
      UniformMatrix2fv(1, 2, false,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<UniformMatrix2fvImmediate, 0>();
  GLfloat temp[4 * 2] = { 0, };
  cmd.Init(1, 2, false, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderTest, UniformMatrix2fvImmediateInvalidArgs2_0) {
  UniformMatrix2fvImmediate& cmd =
      *GetImmediateAs<UniformMatrix2fvImmediate>();
  EXPECT_CALL(*gl_, UniformMatrix2fv(_, _, _, _)).Times(0);
  SpecializedSetup<UniformMatrix2fvImmediate, 0>();
  GLfloat temp[4 * 2] = { 0, };
  cmd.Init(1, 2, true, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderTest, UniformMatrix3fvValidArgs) {
  EXPECT_CALL(
      *gl_, UniformMatrix3fv(
          1, 2, false, reinterpret_cast<const GLfloat*>(
              shared_memory_address_)));
  SpecializedSetup<UniformMatrix3fv, 0>();
  UniformMatrix3fv cmd;
  cmd.Init(1, 2, false, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, UniformMatrix3fvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, UniformMatrix3fv(_, _, _, _)).Times(0);
  SpecializedSetup<UniformMatrix3fv, 0>();
  UniformMatrix3fv cmd;
  cmd.Init(1, 2, true, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, UniformMatrix3fvInvalidArgs3_0) {
  EXPECT_CALL(*gl_, UniformMatrix3fv(_, _, _, _)).Times(0);
  SpecializedSetup<UniformMatrix3fv, 0>();
  UniformMatrix3fv cmd;
  cmd.Init(1, 2, false, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, UniformMatrix3fvInvalidArgs3_1) {
  EXPECT_CALL(*gl_, UniformMatrix3fv(_, _, _, _)).Times(0);
  SpecializedSetup<UniformMatrix3fv, 0>();
  UniformMatrix3fv cmd;
  cmd.Init(1, 2, false, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, UniformMatrix3fvImmediateValidArgs) {
  UniformMatrix3fvImmediate& cmd =
      *GetImmediateAs<UniformMatrix3fvImmediate>();
  EXPECT_CALL(
      *gl_,
      UniformMatrix3fv(1, 2, false,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<UniformMatrix3fvImmediate, 0>();
  GLfloat temp[9 * 2] = { 0, };
  cmd.Init(1, 2, false, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderTest, UniformMatrix3fvImmediateInvalidArgs2_0) {
  UniformMatrix3fvImmediate& cmd =
      *GetImmediateAs<UniformMatrix3fvImmediate>();
  EXPECT_CALL(*gl_, UniformMatrix3fv(_, _, _, _)).Times(0);
  SpecializedSetup<UniformMatrix3fvImmediate, 0>();
  GLfloat temp[9 * 2] = { 0, };
  cmd.Init(1, 2, true, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderTest, UniformMatrix4fvValidArgs) {
  EXPECT_CALL(
      *gl_, UniformMatrix4fv(
          1, 2, false, reinterpret_cast<const GLfloat*>(
              shared_memory_address_)));
  SpecializedSetup<UniformMatrix4fv, 0>();
  UniformMatrix4fv cmd;
  cmd.Init(1, 2, false, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, UniformMatrix4fvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, UniformMatrix4fv(_, _, _, _)).Times(0);
  SpecializedSetup<UniformMatrix4fv, 0>();
  UniformMatrix4fv cmd;
  cmd.Init(1, 2, true, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, UniformMatrix4fvInvalidArgs3_0) {
  EXPECT_CALL(*gl_, UniformMatrix4fv(_, _, _, _)).Times(0);
  SpecializedSetup<UniformMatrix4fv, 0>();
  UniformMatrix4fv cmd;
  cmd.Init(1, 2, false, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, UniformMatrix4fvInvalidArgs3_1) {
  EXPECT_CALL(*gl_, UniformMatrix4fv(_, _, _, _)).Times(0);
  SpecializedSetup<UniformMatrix4fv, 0>();
  UniformMatrix4fv cmd;
  cmd.Init(1, 2, false, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, UniformMatrix4fvImmediateValidArgs) {
  UniformMatrix4fvImmediate& cmd =
      *GetImmediateAs<UniformMatrix4fvImmediate>();
  EXPECT_CALL(
      *gl_,
      UniformMatrix4fv(1, 2, false,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<UniformMatrix4fvImmediate, 0>();
  GLfloat temp[16 * 2] = { 0, };
  cmd.Init(1, 2, false, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderTest, UniformMatrix4fvImmediateInvalidArgs2_0) {
  UniformMatrix4fvImmediate& cmd =
      *GetImmediateAs<UniformMatrix4fvImmediate>();
  EXPECT_CALL(*gl_, UniformMatrix4fv(_, _, _, _)).Times(0);
  SpecializedSetup<UniformMatrix4fvImmediate, 0>();
  GLfloat temp[16 * 2] = { 0, };
  cmd.Init(1, 2, true, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}
// TODO(gman): UseProgram

TEST_F(GLES2DecoderTest, ValidateProgramValidArgs) {
  EXPECT_CALL(*gl_, ValidateProgram(kServiceProgramId));
  SpecializedSetup<ValidateProgram, 0>();
  ValidateProgram cmd;
  cmd.Init(client_program_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, VertexAttrib1fValidArgs) {
  EXPECT_CALL(*gl_, VertexAttrib1f(1, 2));
  SpecializedSetup<VertexAttrib1f, 0>();
  VertexAttrib1f cmd;
  cmd.Init(1, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, VertexAttrib1fvValidArgs) {
  EXPECT_CALL(
      *gl_, VertexAttrib1fv(
          1, reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<VertexAttrib1fv, 0>();
  VertexAttrib1fv cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, VertexAttrib1fvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, VertexAttrib1fv(_, _)).Times(0);
  SpecializedSetup<VertexAttrib1fv, 0>();
  VertexAttrib1fv cmd;
  cmd.Init(1, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, VertexAttrib1fvInvalidArgs1_1) {
  EXPECT_CALL(*gl_, VertexAttrib1fv(_, _)).Times(0);
  SpecializedSetup<VertexAttrib1fv, 0>();
  VertexAttrib1fv cmd;
  cmd.Init(1, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, VertexAttrib1fvImmediateValidArgs) {
  VertexAttrib1fvImmediate& cmd = *GetImmediateAs<VertexAttrib1fvImmediate>();
  EXPECT_CALL(
      *gl_,
      VertexAttrib1fv(1,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<VertexAttrib1fvImmediate, 0>();
  GLfloat temp[1] = { 0, };
  cmd.Init(1, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderTest, VertexAttrib2fValidArgs) {
  EXPECT_CALL(*gl_, VertexAttrib2f(1, 2, 3));
  SpecializedSetup<VertexAttrib2f, 0>();
  VertexAttrib2f cmd;
  cmd.Init(1, 2, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, VertexAttrib2fvValidArgs) {
  EXPECT_CALL(
      *gl_, VertexAttrib2fv(
          1, reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<VertexAttrib2fv, 0>();
  VertexAttrib2fv cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, VertexAttrib2fvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, VertexAttrib2fv(_, _)).Times(0);
  SpecializedSetup<VertexAttrib2fv, 0>();
  VertexAttrib2fv cmd;
  cmd.Init(1, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, VertexAttrib2fvInvalidArgs1_1) {
  EXPECT_CALL(*gl_, VertexAttrib2fv(_, _)).Times(0);
  SpecializedSetup<VertexAttrib2fv, 0>();
  VertexAttrib2fv cmd;
  cmd.Init(1, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, VertexAttrib2fvImmediateValidArgs) {
  VertexAttrib2fvImmediate& cmd = *GetImmediateAs<VertexAttrib2fvImmediate>();
  EXPECT_CALL(
      *gl_,
      VertexAttrib2fv(1,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<VertexAttrib2fvImmediate, 0>();
  GLfloat temp[2] = { 0, };
  cmd.Init(1, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderTest, VertexAttrib3fValidArgs) {
  EXPECT_CALL(*gl_, VertexAttrib3f(1, 2, 3, 4));
  SpecializedSetup<VertexAttrib3f, 0>();
  VertexAttrib3f cmd;
  cmd.Init(1, 2, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, VertexAttrib3fvValidArgs) {
  EXPECT_CALL(
      *gl_, VertexAttrib3fv(
          1, reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<VertexAttrib3fv, 0>();
  VertexAttrib3fv cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, VertexAttrib3fvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, VertexAttrib3fv(_, _)).Times(0);
  SpecializedSetup<VertexAttrib3fv, 0>();
  VertexAttrib3fv cmd;
  cmd.Init(1, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, VertexAttrib3fvInvalidArgs1_1) {
  EXPECT_CALL(*gl_, VertexAttrib3fv(_, _)).Times(0);
  SpecializedSetup<VertexAttrib3fv, 0>();
  VertexAttrib3fv cmd;
  cmd.Init(1, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, VertexAttrib3fvImmediateValidArgs) {
  VertexAttrib3fvImmediate& cmd = *GetImmediateAs<VertexAttrib3fvImmediate>();
  EXPECT_CALL(
      *gl_,
      VertexAttrib3fv(1,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<VertexAttrib3fvImmediate, 0>();
  GLfloat temp[3] = { 0, };
  cmd.Init(1, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_F(GLES2DecoderTest, VertexAttrib4fValidArgs) {
  EXPECT_CALL(*gl_, VertexAttrib4f(1, 2, 3, 4, 5));
  SpecializedSetup<VertexAttrib4f, 0>();
  VertexAttrib4f cmd;
  cmd.Init(1, 2, 3, 4, 5);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, VertexAttrib4fvValidArgs) {
  EXPECT_CALL(
      *gl_, VertexAttrib4fv(
          1, reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<VertexAttrib4fv, 0>();
  VertexAttrib4fv cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, VertexAttrib4fvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, VertexAttrib4fv(_, _)).Times(0);
  SpecializedSetup<VertexAttrib4fv, 0>();
  VertexAttrib4fv cmd;
  cmd.Init(1, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, VertexAttrib4fvInvalidArgs1_1) {
  EXPECT_CALL(*gl_, VertexAttrib4fv(_, _)).Times(0);
  SpecializedSetup<VertexAttrib4fv, 0>();
  VertexAttrib4fv cmd;
  cmd.Init(1, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest, VertexAttrib4fvImmediateValidArgs) {
  VertexAttrib4fvImmediate& cmd = *GetImmediateAs<VertexAttrib4fvImmediate>();
  EXPECT_CALL(
      *gl_,
      VertexAttrib4fv(1,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<VertexAttrib4fvImmediate, 0>();
  GLfloat temp[4] = { 0, };
  cmd.Init(1, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
}
// TODO(gman): VertexAttribPointer


TEST_F(GLES2DecoderTest, ViewportValidArgs) {
  EXPECT_CALL(*gl_, Viewport(1, 2, 3, 4));
  SpecializedSetup<Viewport, 0>();
  Viewport cmd;
  cmd.Init(1, 2, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}
// TODO(gman): SwapBuffers
#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_AUTOGEN_H_

