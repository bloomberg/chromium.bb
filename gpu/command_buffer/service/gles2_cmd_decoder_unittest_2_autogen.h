// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated. DO NOT EDIT!

// It is included by gles2_cmd_decoder_unittest_2.cc
#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_2_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_2_AUTOGEN_H_


TEST_F(GLES2DecoderTest2, GetVertexAttribivValidArgs) {
  SpecializedSetup<GetVertexAttribiv, 0>(true);
  typedef GetVertexAttribiv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->size = 0;
  GetVertexAttribiv cmd;
  cmd.Init(
      1, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(
                GL_VERTEX_ATTRIB_ARRAY_NORMALIZED),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, GetVertexAttribivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, GetVertexAttribiv(_, _, _)).Times(0);
  SpecializedSetup<GetVertexAttribiv, 0>(false);
  GetVertexAttribiv::Result* result =
      static_cast<GetVertexAttribiv::Result*>(shared_memory_address_);
  result->size = 0;
  GetVertexAttribiv cmd;
  cmd.Init(1, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_F(GLES2DecoderTest2, GetVertexAttribivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, GetVertexAttribiv(_, _, _)).Times(0);
  SpecializedSetup<GetVertexAttribiv, 0>(false);
  GetVertexAttribiv::Result* result =
      static_cast<GetVertexAttribiv::Result*>(shared_memory_address_);
  result->size = 0;
  GetVertexAttribiv cmd;
  cmd.Init(
      1, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, shared_memory_id_,
      kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}
// TODO(gman): GetVertexAttribPointerv


TEST_F(GLES2DecoderTest2, HintValidArgs) {
  EXPECT_CALL(*gl_, Hint(GL_GENERATE_MIPMAP_HINT, GL_FASTEST));
  SpecializedSetup<Hint, 0>(true);
  Hint cmd;
  cmd.Init(GL_GENERATE_MIPMAP_HINT, GL_FASTEST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, HintInvalidArgs0_0) {
  EXPECT_CALL(*gl_, Hint(_, _)).Times(0);
  SpecializedSetup<Hint, 0>(false);
  Hint cmd;
  cmd.Init(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, IsBufferValidArgs) {
  SpecializedSetup<IsBuffer, 0>(true);
  IsBuffer cmd;
  cmd.Init(client_buffer_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, IsBufferInvalidArgsBadSharedMemoryId) {
  SpecializedSetup<IsBuffer, 0>(false);
  IsBuffer cmd;
  cmd.Init(client_buffer_id_, kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(client_buffer_id_, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, IsEnabledValidArgs) {
  EXPECT_CALL(*gl_, IsEnabled(GL_BLEND));
  SpecializedSetup<IsEnabled, 0>(true);
  IsEnabled cmd;
  cmd.Init(GL_BLEND, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, IsEnabledInvalidArgs0_0) {
  EXPECT_CALL(*gl_, IsEnabled(_)).Times(0);
  SpecializedSetup<IsEnabled, 0>(false);
  IsEnabled cmd;
  cmd.Init(GL_CLIP_PLANE0, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, IsEnabledInvalidArgs0_1) {
  EXPECT_CALL(*gl_, IsEnabled(_)).Times(0);
  SpecializedSetup<IsEnabled, 0>(false);
  IsEnabled cmd;
  cmd.Init(GL_POINT_SPRITE, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, IsEnabledInvalidArgsBadSharedMemoryId) {
  EXPECT_CALL(*gl_, IsEnabled(GL_BLEND)).Times(0);
  SpecializedSetup<IsEnabled, 0>(false);
  IsEnabled cmd;
  cmd.Init(GL_BLEND, kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(GL_BLEND, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, IsFramebufferValidArgs) {
  SpecializedSetup<IsFramebuffer, 0>(true);
  IsFramebuffer cmd;
  cmd.Init(client_framebuffer_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, IsFramebufferInvalidArgsBadSharedMemoryId) {
  SpecializedSetup<IsFramebuffer, 0>(false);
  IsFramebuffer cmd;
  cmd.Init(
      client_framebuffer_id_, kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(
      client_framebuffer_id_, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, IsProgramValidArgs) {
  SpecializedSetup<IsProgram, 0>(true);
  IsProgram cmd;
  cmd.Init(client_program_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, IsProgramInvalidArgsBadSharedMemoryId) {
  SpecializedSetup<IsProgram, 0>(false);
  IsProgram cmd;
  cmd.Init(client_program_id_, kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, IsRenderbufferValidArgs) {
  SpecializedSetup<IsRenderbuffer, 0>(true);
  IsRenderbuffer cmd;
  cmd.Init(client_renderbuffer_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, IsRenderbufferInvalidArgsBadSharedMemoryId) {
  SpecializedSetup<IsRenderbuffer, 0>(false);
  IsRenderbuffer cmd;
  cmd.Init(
      client_renderbuffer_id_, kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(
      client_renderbuffer_id_, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, IsShaderValidArgs) {
  SpecializedSetup<IsShader, 0>(true);
  IsShader cmd;
  cmd.Init(client_shader_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, IsShaderInvalidArgsBadSharedMemoryId) {
  SpecializedSetup<IsShader, 0>(false);
  IsShader cmd;
  cmd.Init(client_shader_id_, kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(client_shader_id_, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, IsTextureValidArgs) {
  SpecializedSetup<IsTexture, 0>(true);
  IsTexture cmd;
  cmd.Init(client_texture_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, IsTextureInvalidArgsBadSharedMemoryId) {
  SpecializedSetup<IsTexture, 0>(false);
  IsTexture cmd;
  cmd.Init(client_texture_id_, kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(client_texture_id_, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, LineWidthValidArgs) {
  EXPECT_CALL(*gl_, LineWidth(1));
  SpecializedSetup<LineWidth, 0>(true);
  LineWidth cmd;
  cmd.Init(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, LinkProgramValidArgs) {
  EXPECT_CALL(*gl_, LinkProgram(kServiceProgramId));
  SpecializedSetup<LinkProgram, 0>(true);
  LinkProgram cmd;
  cmd.Init(client_program_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
// TODO(gman): PixelStorei


TEST_F(GLES2DecoderTest2, PolygonOffsetValidArgs) {
  EXPECT_CALL(*gl_, PolygonOffset(1, 2));
  SpecializedSetup<PolygonOffset, 0>(true);
  PolygonOffset cmd;
  cmd.Init(1, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
// TODO(gman): ReadPixels

// TODO(gman): ReleaseShaderCompiler

TEST_F(GLES2DecoderTest2, RenderbufferStorageValidArgs) {
  SpecializedSetup<RenderbufferStorage, 0>(true);
  RenderbufferStorage cmd;
  cmd.Init(GL_RENDERBUFFER, GL_RGBA4, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, RenderbufferStorageInvalidArgs0_0) {
  EXPECT_CALL(*gl_, RenderbufferStorageEXT(_, _, _, _)).Times(0);
  SpecializedSetup<RenderbufferStorage, 0>(false);
  RenderbufferStorage cmd;
  cmd.Init(GL_FRAMEBUFFER, GL_RGBA4, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, RenderbufferStorageInvalidArgs2_0) {
  EXPECT_CALL(*gl_, RenderbufferStorageEXT(_, _, _, _)).Times(0);
  SpecializedSetup<RenderbufferStorage, 0>(false);
  RenderbufferStorage cmd;
  cmd.Init(GL_RENDERBUFFER, GL_RGBA4, -1, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderTest2, RenderbufferStorageInvalidArgs3_0) {
  EXPECT_CALL(*gl_, RenderbufferStorageEXT(_, _, _, _)).Times(0);
  SpecializedSetup<RenderbufferStorage, 0>(false);
  RenderbufferStorage cmd;
  cmd.Init(GL_RENDERBUFFER, GL_RGBA4, 3, -1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderTest2, SampleCoverageValidArgs) {
  EXPECT_CALL(*gl_, SampleCoverage(1, 2));
  SpecializedSetup<SampleCoverage, 0>(true);
  SampleCoverage cmd;
  cmd.Init(1, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, ScissorValidArgs) {
  EXPECT_CALL(*gl_, Scissor(1, 2, 3, 4));
  SpecializedSetup<Scissor, 0>(true);
  Scissor cmd;
  cmd.Init(1, 2, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, ScissorInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Scissor(_, _, _, _)).Times(0);
  SpecializedSetup<Scissor, 0>(false);
  Scissor cmd;
  cmd.Init(1, 2, -1, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderTest2, ScissorInvalidArgs3_0) {
  EXPECT_CALL(*gl_, Scissor(_, _, _, _)).Times(0);
  SpecializedSetup<Scissor, 0>(false);
  Scissor cmd;
  cmd.Init(1, 2, 3, -1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}
// TODO(gman): ShaderBinary

// TODO(gman): ShaderSource

// TODO(gman): ShaderSourceImmediate

// TODO(gman): ShaderSourceBucket

TEST_F(GLES2DecoderTest2, StencilFuncValidArgs) {
  EXPECT_CALL(*gl_, StencilFunc(GL_NEVER, 2, 3));
  SpecializedSetup<StencilFunc, 0>(true);
  StencilFunc cmd;
  cmd.Init(GL_NEVER, 2, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, StencilFuncSeparateValidArgs) {
  EXPECT_CALL(*gl_, StencilFuncSeparate(GL_FRONT, GL_NEVER, 3, 4));
  SpecializedSetup<StencilFuncSeparate, 0>(true);
  StencilFuncSeparate cmd;
  cmd.Init(GL_FRONT, GL_NEVER, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, StencilMaskValidArgs) {
  EXPECT_CALL(*gl_, StencilMask(1));
  SpecializedSetup<StencilMask, 0>(true);
  StencilMask cmd;
  cmd.Init(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, StencilMaskSeparateValidArgs) {
  EXPECT_CALL(*gl_, StencilMaskSeparate(GL_FRONT, 2));
  SpecializedSetup<StencilMaskSeparate, 0>(true);
  StencilMaskSeparate cmd;
  cmd.Init(GL_FRONT, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, StencilOpValidArgs) {
  EXPECT_CALL(*gl_, StencilOp(GL_KEEP, GL_KEEP, GL_KEEP));
  SpecializedSetup<StencilOp, 0>(true);
  StencilOp cmd;
  cmd.Init(GL_KEEP, GL_KEEP, GL_KEEP);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, StencilOpSeparateValidArgs) {
  EXPECT_CALL(*gl_, StencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_KEEP));
  SpecializedSetup<StencilOpSeparate, 0>(true);
  StencilOpSeparate cmd;
  cmd.Init(GL_FRONT, GL_KEEP, GL_KEEP, GL_KEEP);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
// TODO(gman): TexImage2D

// TODO(gman): TexImage2DImmediate


TEST_F(GLES2DecoderTest2, TexParameterfValidArgs) {
  EXPECT_CALL(*gl_, TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 3));
  SpecializedSetup<TexParameterf, 0>(true);
  TexParameterf cmd;
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterfInvalidArgs0_0) {
  EXPECT_CALL(*gl_, TexParameterf(_, _, _)).Times(0);
  SpecializedSetup<TexParameterf, 0>(false);
  TexParameterf cmd;
  cmd.Init(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterfInvalidArgs0_1) {
  EXPECT_CALL(*gl_, TexParameterf(_, _, _)).Times(0);
  SpecializedSetup<TexParameterf, 0>(false);
  TexParameterf cmd;
  cmd.Init(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterfInvalidArgs1_0) {
  EXPECT_CALL(*gl_, TexParameterf(_, _, _)).Times(0);
  SpecializedSetup<TexParameterf, 0>(false);
  TexParameterf cmd;
  cmd.Init(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterfvValidArgs) {
  EXPECT_CALL(
      *gl_, TexParameterfv(
          GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
          reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<TexParameterfv, 0>(true);
  TexParameterfv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterfvInvalidArgs0_0) {
  EXPECT_CALL(*gl_, TexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<TexParameterfv, 0>(false);
  TexParameterfv cmd;
  cmd.Init(
      GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterfvInvalidArgs0_1) {
  EXPECT_CALL(*gl_, TexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<TexParameterfv, 0>(false);
  TexParameterfv cmd;
  cmd.Init(
      GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterfvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, TexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<TexParameterfv, 0>(false);
  TexParameterfv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_GENERATE_MIPMAP, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterfvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, TexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<TexParameterfv, 0>(false);
  TexParameterfv cmd;
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, TexParameterfvInvalidArgs2_1) {
  EXPECT_CALL(*gl_, TexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<TexParameterfv, 0>(false);
  TexParameterfv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, TexParameterfvImmediateValidArgs) {
  TexParameterfvImmediate& cmd = *GetImmediateAs<TexParameterfvImmediate>();
  EXPECT_CALL(
      *gl_,
      TexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<TexParameterfvImmediate, 0>(true);
  GLfloat temp[1] = { 0, };
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterfvImmediateInvalidArgs0_0) {
  TexParameterfvImmediate& cmd = *GetImmediateAs<TexParameterfvImmediate>();
  EXPECT_CALL(*gl_, TexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<TexParameterfvImmediate, 0>(false);
  GLfloat temp[1] = { 0, };
  cmd.Init(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterfvImmediateInvalidArgs0_1) {
  TexParameterfvImmediate& cmd = *GetImmediateAs<TexParameterfvImmediate>();
  EXPECT_CALL(*gl_, TexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<TexParameterfvImmediate, 0>(false);
  GLfloat temp[1] = { 0, };
  cmd.Init(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterfvImmediateInvalidArgs1_0) {
  TexParameterfvImmediate& cmd = *GetImmediateAs<TexParameterfvImmediate>();
  EXPECT_CALL(*gl_, TexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<TexParameterfvImmediate, 0>(false);
  GLfloat temp[1] = { 0, };
  cmd.Init(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameteriValidArgs) {
  EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 3));
  SpecializedSetup<TexParameteri, 0>(true);
  TexParameteri cmd;
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameteriInvalidArgs0_0) {
  EXPECT_CALL(*gl_, TexParameteri(_, _, _)).Times(0);
  SpecializedSetup<TexParameteri, 0>(false);
  TexParameteri cmd;
  cmd.Init(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameteriInvalidArgs0_1) {
  EXPECT_CALL(*gl_, TexParameteri(_, _, _)).Times(0);
  SpecializedSetup<TexParameteri, 0>(false);
  TexParameteri cmd;
  cmd.Init(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameteriInvalidArgs1_0) {
  EXPECT_CALL(*gl_, TexParameteri(_, _, _)).Times(0);
  SpecializedSetup<TexParameteri, 0>(false);
  TexParameteri cmd;
  cmd.Init(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterivValidArgs) {
  EXPECT_CALL(
      *gl_, TexParameteriv(
          GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, reinterpret_cast<const GLint*>(
              shared_memory_address_)));
  SpecializedSetup<TexParameteriv, 0>(true);
  TexParameteriv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterivInvalidArgs0_0) {
  EXPECT_CALL(*gl_, TexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<TexParameteriv, 0>(false);
  TexParameteriv cmd;
  cmd.Init(
      GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterivInvalidArgs0_1) {
  EXPECT_CALL(*gl_, TexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<TexParameteriv, 0>(false);
  TexParameteriv cmd;
  cmd.Init(
      GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterivInvalidArgs1_0) {
  EXPECT_CALL(*gl_, TexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<TexParameteriv, 0>(false);
  TexParameteriv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_GENERATE_MIPMAP, shared_memory_id_,
      shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, TexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<TexParameteriv, 0>(false);
  TexParameteriv cmd;
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, TexParameterivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, TexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<TexParameteriv, 0>(false);
  TexParameteriv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, TexParameterivImmediateValidArgs) {
  TexParameterivImmediate& cmd = *GetImmediateAs<TexParameterivImmediate>();
  EXPECT_CALL(
      *gl_,
      TexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
          reinterpret_cast<GLint*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<TexParameterivImmediate, 0>(true);
  GLint temp[1] = { 0, };
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterivImmediateInvalidArgs0_0) {
  TexParameterivImmediate& cmd = *GetImmediateAs<TexParameterivImmediate>();
  EXPECT_CALL(*gl_, TexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<TexParameterivImmediate, 0>(false);
  GLint temp[1] = { 0, };
  cmd.Init(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterivImmediateInvalidArgs0_1) {
  TexParameterivImmediate& cmd = *GetImmediateAs<TexParameterivImmediate>();
  EXPECT_CALL(*gl_, TexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<TexParameterivImmediate, 0>(false);
  GLint temp[1] = { 0, };
  cmd.Init(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterivImmediateInvalidArgs1_0) {
  TexParameterivImmediate& cmd = *GetImmediateAs<TexParameterivImmediate>();
  EXPECT_CALL(*gl_, TexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<TexParameterivImmediate, 0>(false);
  GLint temp[1] = { 0, };
  cmd.Init(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}
// TODO(gman): TexSubImage2D

// TODO(gman): TexSubImage2DImmediate


TEST_F(GLES2DecoderTest2, Uniform1fValidArgs) {
  EXPECT_CALL(*gl_, Uniform1fv(1, 1, _));
  SpecializedSetup<Uniform1f, 0>(true);
  Uniform1f cmd;
  cmd.Init(1, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform1fvValidArgs) {
  EXPECT_CALL(
      *gl_, Uniform1fv(
          1, 2, reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<Uniform1fv, 0>(true);
  Uniform1fv cmd;
  cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform1fvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, Uniform1fv(_, _, _)).Times(0);
  SpecializedSetup<Uniform1fv, 0>(false);
  Uniform1fv cmd;
  cmd.Init(1, -1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform1fvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Uniform1fv(_, _, _)).Times(0);
  SpecializedSetup<Uniform1fv, 0>(false);
  Uniform1fv cmd;
  cmd.Init(1, 2, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, Uniform1fvInvalidArgs2_1) {
  EXPECT_CALL(*gl_, Uniform1fv(_, _, _)).Times(0);
  SpecializedSetup<Uniform1fv, 0>(false);
  Uniform1fv cmd;
  cmd.Init(1, 2, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, Uniform1fvImmediateValidArgs) {
  Uniform1fvImmediate& cmd = *GetImmediateAs<Uniform1fvImmediate>();
  EXPECT_CALL(
      *gl_,
      Uniform1fv(1, 2,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<Uniform1fvImmediate, 0>(true);
  GLfloat temp[1 * 2] = { 0, };
  cmd.Init(1, 2, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
// TODO(gman): Uniform1i
// TODO(gman): Uniform1iv
// TODO(gman): Uniform1ivImmediate

TEST_F(GLES2DecoderTest2, Uniform2fValidArgs) {
  EXPECT_CALL(*gl_, Uniform2fv(1, 1, _));
  SpecializedSetup<Uniform2f, 0>(true);
  Uniform2f cmd;
  cmd.Init(1, 2, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform2fvValidArgs) {
  EXPECT_CALL(
      *gl_, Uniform2fv(
          1, 2, reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<Uniform2fv, 0>(true);
  Uniform2fv cmd;
  cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform2fvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, Uniform2fv(_, _, _)).Times(0);
  SpecializedSetup<Uniform2fv, 0>(false);
  Uniform2fv cmd;
  cmd.Init(1, -1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform2fvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Uniform2fv(_, _, _)).Times(0);
  SpecializedSetup<Uniform2fv, 0>(false);
  Uniform2fv cmd;
  cmd.Init(1, 2, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, Uniform2fvInvalidArgs2_1) {
  EXPECT_CALL(*gl_, Uniform2fv(_, _, _)).Times(0);
  SpecializedSetup<Uniform2fv, 0>(false);
  Uniform2fv cmd;
  cmd.Init(1, 2, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, Uniform2fvImmediateValidArgs) {
  Uniform2fvImmediate& cmd = *GetImmediateAs<Uniform2fvImmediate>();
  EXPECT_CALL(
      *gl_,
      Uniform2fv(1, 2,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<Uniform2fvImmediate, 0>(true);
  GLfloat temp[2 * 2] = { 0, };
  cmd.Init(1, 2, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform2iValidArgs) {
  EXPECT_CALL(*gl_, Uniform2i(1, 2, 3));
  SpecializedSetup<Uniform2i, 0>(true);
  Uniform2i cmd;
  cmd.Init(1, 2, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform2ivValidArgs) {
  EXPECT_CALL(
      *gl_, Uniform2iv(
          1, 2, reinterpret_cast<const GLint*>(shared_memory_address_)));
  SpecializedSetup<Uniform2iv, 0>(true);
  Uniform2iv cmd;
  cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform2ivInvalidArgs1_0) {
  EXPECT_CALL(*gl_, Uniform2iv(_, _, _)).Times(0);
  SpecializedSetup<Uniform2iv, 0>(false);
  Uniform2iv cmd;
  cmd.Init(1, -1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform2ivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Uniform2iv(_, _, _)).Times(0);
  SpecializedSetup<Uniform2iv, 0>(false);
  Uniform2iv cmd;
  cmd.Init(1, 2, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, Uniform2ivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, Uniform2iv(_, _, _)).Times(0);
  SpecializedSetup<Uniform2iv, 0>(false);
  Uniform2iv cmd;
  cmd.Init(1, 2, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, Uniform2ivImmediateValidArgs) {
  Uniform2ivImmediate& cmd = *GetImmediateAs<Uniform2ivImmediate>();
  EXPECT_CALL(
      *gl_,
      Uniform2iv(1, 2,
          reinterpret_cast<GLint*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<Uniform2ivImmediate, 0>(true);
  GLint temp[2 * 2] = { 0, };
  cmd.Init(1, 2, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform3fValidArgs) {
  EXPECT_CALL(*gl_, Uniform3fv(1, 1, _));
  SpecializedSetup<Uniform3f, 0>(true);
  Uniform3f cmd;
  cmd.Init(1, 2, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform3fvValidArgs) {
  EXPECT_CALL(
      *gl_, Uniform3fv(
          1, 2, reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<Uniform3fv, 0>(true);
  Uniform3fv cmd;
  cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform3fvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, Uniform3fv(_, _, _)).Times(0);
  SpecializedSetup<Uniform3fv, 0>(false);
  Uniform3fv cmd;
  cmd.Init(1, -1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform3fvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Uniform3fv(_, _, _)).Times(0);
  SpecializedSetup<Uniform3fv, 0>(false);
  Uniform3fv cmd;
  cmd.Init(1, 2, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, Uniform3fvInvalidArgs2_1) {
  EXPECT_CALL(*gl_, Uniform3fv(_, _, _)).Times(0);
  SpecializedSetup<Uniform3fv, 0>(false);
  Uniform3fv cmd;
  cmd.Init(1, 2, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, Uniform3fvImmediateValidArgs) {
  Uniform3fvImmediate& cmd = *GetImmediateAs<Uniform3fvImmediate>();
  EXPECT_CALL(
      *gl_,
      Uniform3fv(1, 2,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<Uniform3fvImmediate, 0>(true);
  GLfloat temp[3 * 2] = { 0, };
  cmd.Init(1, 2, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform3iValidArgs) {
  EXPECT_CALL(*gl_, Uniform3i(1, 2, 3, 4));
  SpecializedSetup<Uniform3i, 0>(true);
  Uniform3i cmd;
  cmd.Init(1, 2, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform3ivValidArgs) {
  EXPECT_CALL(
      *gl_, Uniform3iv(
          1, 2, reinterpret_cast<const GLint*>(shared_memory_address_)));
  SpecializedSetup<Uniform3iv, 0>(true);
  Uniform3iv cmd;
  cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform3ivInvalidArgs1_0) {
  EXPECT_CALL(*gl_, Uniform3iv(_, _, _)).Times(0);
  SpecializedSetup<Uniform3iv, 0>(false);
  Uniform3iv cmd;
  cmd.Init(1, -1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform3ivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Uniform3iv(_, _, _)).Times(0);
  SpecializedSetup<Uniform3iv, 0>(false);
  Uniform3iv cmd;
  cmd.Init(1, 2, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, Uniform3ivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, Uniform3iv(_, _, _)).Times(0);
  SpecializedSetup<Uniform3iv, 0>(false);
  Uniform3iv cmd;
  cmd.Init(1, 2, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, Uniform3ivImmediateValidArgs) {
  Uniform3ivImmediate& cmd = *GetImmediateAs<Uniform3ivImmediate>();
  EXPECT_CALL(
      *gl_,
      Uniform3iv(1, 2,
          reinterpret_cast<GLint*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<Uniform3ivImmediate, 0>(true);
  GLint temp[3 * 2] = { 0, };
  cmd.Init(1, 2, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform4fValidArgs) {
  EXPECT_CALL(*gl_, Uniform4fv(1, 1, _));
  SpecializedSetup<Uniform4f, 0>(true);
  Uniform4f cmd;
  cmd.Init(1, 2, 3, 4, 5);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform4fvValidArgs) {
  EXPECT_CALL(
      *gl_, Uniform4fv(
          1, 2, reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<Uniform4fv, 0>(true);
  Uniform4fv cmd;
  cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform4fvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, Uniform4fv(_, _, _)).Times(0);
  SpecializedSetup<Uniform4fv, 0>(false);
  Uniform4fv cmd;
  cmd.Init(1, -1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform4fvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Uniform4fv(_, _, _)).Times(0);
  SpecializedSetup<Uniform4fv, 0>(false);
  Uniform4fv cmd;
  cmd.Init(1, 2, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, Uniform4fvInvalidArgs2_1) {
  EXPECT_CALL(*gl_, Uniform4fv(_, _, _)).Times(0);
  SpecializedSetup<Uniform4fv, 0>(false);
  Uniform4fv cmd;
  cmd.Init(1, 2, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, Uniform4fvImmediateValidArgs) {
  Uniform4fvImmediate& cmd = *GetImmediateAs<Uniform4fvImmediate>();
  EXPECT_CALL(
      *gl_,
      Uniform4fv(1, 2,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<Uniform4fvImmediate, 0>(true);
  GLfloat temp[4 * 2] = { 0, };
  cmd.Init(1, 2, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform4iValidArgs) {
  EXPECT_CALL(*gl_, Uniform4i(1, 2, 3, 4, 5));
  SpecializedSetup<Uniform4i, 0>(true);
  Uniform4i cmd;
  cmd.Init(1, 2, 3, 4, 5);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform4ivValidArgs) {
  EXPECT_CALL(
      *gl_, Uniform4iv(
          1, 2, reinterpret_cast<const GLint*>(shared_memory_address_)));
  SpecializedSetup<Uniform4iv, 0>(true);
  Uniform4iv cmd;
  cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform4ivInvalidArgs1_0) {
  EXPECT_CALL(*gl_, Uniform4iv(_, _, _)).Times(0);
  SpecializedSetup<Uniform4iv, 0>(false);
  Uniform4iv cmd;
  cmd.Init(1, -1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform4ivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Uniform4iv(_, _, _)).Times(0);
  SpecializedSetup<Uniform4iv, 0>(false);
  Uniform4iv cmd;
  cmd.Init(1, 2, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, Uniform4ivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, Uniform4iv(_, _, _)).Times(0);
  SpecializedSetup<Uniform4iv, 0>(false);
  Uniform4iv cmd;
  cmd.Init(1, 2, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, Uniform4ivImmediateValidArgs) {
  Uniform4ivImmediate& cmd = *GetImmediateAs<Uniform4ivImmediate>();
  EXPECT_CALL(
      *gl_,
      Uniform4iv(1, 2,
          reinterpret_cast<GLint*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<Uniform4ivImmediate, 0>(true);
  GLint temp[4 * 2] = { 0, };
  cmd.Init(1, 2, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix2fvValidArgs) {
  EXPECT_CALL(
      *gl_, UniformMatrix2fv(
          1, 2, false, reinterpret_cast<const GLfloat*>(
              shared_memory_address_)));
  SpecializedSetup<UniformMatrix2fv, 0>(true);
  UniformMatrix2fv cmd;
  cmd.Init(1, 2, false, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix2fvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, UniformMatrix2fv(_, _, _, _)).Times(0);
  SpecializedSetup<UniformMatrix2fv, 0>(false);
  UniformMatrix2fv cmd;
  cmd.Init(1, -1, false, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix2fvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, UniformMatrix2fv(_, _, _, _)).Times(0);
  SpecializedSetup<UniformMatrix2fv, 0>(false);
  UniformMatrix2fv cmd;
  cmd.Init(1, 2, true, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix2fvInvalidArgs3_0) {
  EXPECT_CALL(*gl_, UniformMatrix2fv(_, _, _, _)).Times(0);
  SpecializedSetup<UniformMatrix2fv, 0>(false);
  UniformMatrix2fv cmd;
  cmd.Init(1, 2, false, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, UniformMatrix2fvInvalidArgs3_1) {
  EXPECT_CALL(*gl_, UniformMatrix2fv(_, _, _, _)).Times(0);
  SpecializedSetup<UniformMatrix2fv, 0>(false);
  UniformMatrix2fv cmd;
  cmd.Init(1, 2, false, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, UniformMatrix2fvImmediateValidArgs) {
  UniformMatrix2fvImmediate& cmd =
      *GetImmediateAs<UniformMatrix2fvImmediate>();
  EXPECT_CALL(
      *gl_,
      UniformMatrix2fv(1, 2, false,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<UniformMatrix2fvImmediate, 0>(true);
  GLfloat temp[4 * 2] = { 0, };
  cmd.Init(1, 2, false, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix2fvImmediateInvalidArgs2_0) {
  UniformMatrix2fvImmediate& cmd =
      *GetImmediateAs<UniformMatrix2fvImmediate>();
  EXPECT_CALL(*gl_, UniformMatrix2fv(_, _, _, _)).Times(0);
  SpecializedSetup<UniformMatrix2fvImmediate, 0>(false);
  GLfloat temp[4 * 2] = { 0, };
  cmd.Init(1, 2, true, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix3fvValidArgs) {
  EXPECT_CALL(
      *gl_, UniformMatrix3fv(
          1, 2, false, reinterpret_cast<const GLfloat*>(
              shared_memory_address_)));
  SpecializedSetup<UniformMatrix3fv, 0>(true);
  UniformMatrix3fv cmd;
  cmd.Init(1, 2, false, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix3fvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, UniformMatrix3fv(_, _, _, _)).Times(0);
  SpecializedSetup<UniformMatrix3fv, 0>(false);
  UniformMatrix3fv cmd;
  cmd.Init(1, -1, false, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix3fvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, UniformMatrix3fv(_, _, _, _)).Times(0);
  SpecializedSetup<UniformMatrix3fv, 0>(false);
  UniformMatrix3fv cmd;
  cmd.Init(1, 2, true, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix3fvInvalidArgs3_0) {
  EXPECT_CALL(*gl_, UniformMatrix3fv(_, _, _, _)).Times(0);
  SpecializedSetup<UniformMatrix3fv, 0>(false);
  UniformMatrix3fv cmd;
  cmd.Init(1, 2, false, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, UniformMatrix3fvInvalidArgs3_1) {
  EXPECT_CALL(*gl_, UniformMatrix3fv(_, _, _, _)).Times(0);
  SpecializedSetup<UniformMatrix3fv, 0>(false);
  UniformMatrix3fv cmd;
  cmd.Init(1, 2, false, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, UniformMatrix3fvImmediateValidArgs) {
  UniformMatrix3fvImmediate& cmd =
      *GetImmediateAs<UniformMatrix3fvImmediate>();
  EXPECT_CALL(
      *gl_,
      UniformMatrix3fv(1, 2, false,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<UniformMatrix3fvImmediate, 0>(true);
  GLfloat temp[9 * 2] = { 0, };
  cmd.Init(1, 2, false, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix3fvImmediateInvalidArgs2_0) {
  UniformMatrix3fvImmediate& cmd =
      *GetImmediateAs<UniformMatrix3fvImmediate>();
  EXPECT_CALL(*gl_, UniformMatrix3fv(_, _, _, _)).Times(0);
  SpecializedSetup<UniformMatrix3fvImmediate, 0>(false);
  GLfloat temp[9 * 2] = { 0, };
  cmd.Init(1, 2, true, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix4fvValidArgs) {
  EXPECT_CALL(
      *gl_, UniformMatrix4fv(
          1, 2, false, reinterpret_cast<const GLfloat*>(
              shared_memory_address_)));
  SpecializedSetup<UniformMatrix4fv, 0>(true);
  UniformMatrix4fv cmd;
  cmd.Init(1, 2, false, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix4fvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, UniformMatrix4fv(_, _, _, _)).Times(0);
  SpecializedSetup<UniformMatrix4fv, 0>(false);
  UniformMatrix4fv cmd;
  cmd.Init(1, -1, false, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix4fvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, UniformMatrix4fv(_, _, _, _)).Times(0);
  SpecializedSetup<UniformMatrix4fv, 0>(false);
  UniformMatrix4fv cmd;
  cmd.Init(1, 2, true, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix4fvInvalidArgs3_0) {
  EXPECT_CALL(*gl_, UniformMatrix4fv(_, _, _, _)).Times(0);
  SpecializedSetup<UniformMatrix4fv, 0>(false);
  UniformMatrix4fv cmd;
  cmd.Init(1, 2, false, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, UniformMatrix4fvInvalidArgs3_1) {
  EXPECT_CALL(*gl_, UniformMatrix4fv(_, _, _, _)).Times(0);
  SpecializedSetup<UniformMatrix4fv, 0>(false);
  UniformMatrix4fv cmd;
  cmd.Init(1, 2, false, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, UniformMatrix4fvImmediateValidArgs) {
  UniformMatrix4fvImmediate& cmd =
      *GetImmediateAs<UniformMatrix4fvImmediate>();
  EXPECT_CALL(
      *gl_,
      UniformMatrix4fv(1, 2, false,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<UniformMatrix4fvImmediate, 0>(true);
  GLfloat temp[16 * 2] = { 0, };
  cmd.Init(1, 2, false, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix4fvImmediateInvalidArgs2_0) {
  UniformMatrix4fvImmediate& cmd =
      *GetImmediateAs<UniformMatrix4fvImmediate>();
  EXPECT_CALL(*gl_, UniformMatrix4fv(_, _, _, _)).Times(0);
  SpecializedSetup<UniformMatrix4fvImmediate, 0>(false);
  GLfloat temp[16 * 2] = { 0, };
  cmd.Init(1, 2, true, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}
// TODO(gman): UseProgram

TEST_F(GLES2DecoderTest2, ValidateProgramValidArgs) {
  EXPECT_CALL(*gl_, ValidateProgram(kServiceProgramId));
  SpecializedSetup<ValidateProgram, 0>(true);
  ValidateProgram cmd;
  cmd.Init(client_program_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, VertexAttrib1fValidArgs) {
  EXPECT_CALL(*gl_, VertexAttrib1f(1, 2));
  SpecializedSetup<VertexAttrib1f, 0>(true);
  VertexAttrib1f cmd;
  cmd.Init(1, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, VertexAttrib1fvValidArgs) {
  EXPECT_CALL(
      *gl_, VertexAttrib1fv(
          1, reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<VertexAttrib1fv, 0>(true);
  VertexAttrib1fv cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, VertexAttrib1fvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, VertexAttrib1fv(_, _)).Times(0);
  SpecializedSetup<VertexAttrib1fv, 0>(false);
  VertexAttrib1fv cmd;
  cmd.Init(1, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, VertexAttrib1fvInvalidArgs1_1) {
  EXPECT_CALL(*gl_, VertexAttrib1fv(_, _)).Times(0);
  SpecializedSetup<VertexAttrib1fv, 0>(false);
  VertexAttrib1fv cmd;
  cmd.Init(1, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, VertexAttrib1fvImmediateValidArgs) {
  VertexAttrib1fvImmediate& cmd = *GetImmediateAs<VertexAttrib1fvImmediate>();
  EXPECT_CALL(
      *gl_,
      VertexAttrib1fv(1,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<VertexAttrib1fvImmediate, 0>(true);
  GLfloat temp[1] = { 0, };
  cmd.Init(1, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, VertexAttrib2fValidArgs) {
  EXPECT_CALL(*gl_, VertexAttrib2f(1, 2, 3));
  SpecializedSetup<VertexAttrib2f, 0>(true);
  VertexAttrib2f cmd;
  cmd.Init(1, 2, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, VertexAttrib2fvValidArgs) {
  EXPECT_CALL(
      *gl_, VertexAttrib2fv(
          1, reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<VertexAttrib2fv, 0>(true);
  VertexAttrib2fv cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, VertexAttrib2fvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, VertexAttrib2fv(_, _)).Times(0);
  SpecializedSetup<VertexAttrib2fv, 0>(false);
  VertexAttrib2fv cmd;
  cmd.Init(1, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, VertexAttrib2fvInvalidArgs1_1) {
  EXPECT_CALL(*gl_, VertexAttrib2fv(_, _)).Times(0);
  SpecializedSetup<VertexAttrib2fv, 0>(false);
  VertexAttrib2fv cmd;
  cmd.Init(1, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, VertexAttrib2fvImmediateValidArgs) {
  VertexAttrib2fvImmediate& cmd = *GetImmediateAs<VertexAttrib2fvImmediate>();
  EXPECT_CALL(
      *gl_,
      VertexAttrib2fv(1,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<VertexAttrib2fvImmediate, 0>(true);
  GLfloat temp[2] = { 0, };
  cmd.Init(1, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, VertexAttrib3fValidArgs) {
  EXPECT_CALL(*gl_, VertexAttrib3f(1, 2, 3, 4));
  SpecializedSetup<VertexAttrib3f, 0>(true);
  VertexAttrib3f cmd;
  cmd.Init(1, 2, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, VertexAttrib3fvValidArgs) {
  EXPECT_CALL(
      *gl_, VertexAttrib3fv(
          1, reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<VertexAttrib3fv, 0>(true);
  VertexAttrib3fv cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, VertexAttrib3fvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, VertexAttrib3fv(_, _)).Times(0);
  SpecializedSetup<VertexAttrib3fv, 0>(false);
  VertexAttrib3fv cmd;
  cmd.Init(1, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, VertexAttrib3fvInvalidArgs1_1) {
  EXPECT_CALL(*gl_, VertexAttrib3fv(_, _)).Times(0);
  SpecializedSetup<VertexAttrib3fv, 0>(false);
  VertexAttrib3fv cmd;
  cmd.Init(1, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, VertexAttrib3fvImmediateValidArgs) {
  VertexAttrib3fvImmediate& cmd = *GetImmediateAs<VertexAttrib3fvImmediate>();
  EXPECT_CALL(
      *gl_,
      VertexAttrib3fv(1,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<VertexAttrib3fvImmediate, 0>(true);
  GLfloat temp[3] = { 0, };
  cmd.Init(1, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, VertexAttrib4fValidArgs) {
  EXPECT_CALL(*gl_, VertexAttrib4f(1, 2, 3, 4, 5));
  SpecializedSetup<VertexAttrib4f, 0>(true);
  VertexAttrib4f cmd;
  cmd.Init(1, 2, 3, 4, 5);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, VertexAttrib4fvValidArgs) {
  EXPECT_CALL(
      *gl_, VertexAttrib4fv(
          1, reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<VertexAttrib4fv, 0>(true);
  VertexAttrib4fv cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, VertexAttrib4fvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, VertexAttrib4fv(_, _)).Times(0);
  SpecializedSetup<VertexAttrib4fv, 0>(false);
  VertexAttrib4fv cmd;
  cmd.Init(1, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, VertexAttrib4fvInvalidArgs1_1) {
  EXPECT_CALL(*gl_, VertexAttrib4fv(_, _)).Times(0);
  SpecializedSetup<VertexAttrib4fv, 0>(false);
  VertexAttrib4fv cmd;
  cmd.Init(1, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, VertexAttrib4fvImmediateValidArgs) {
  VertexAttrib4fvImmediate& cmd = *GetImmediateAs<VertexAttrib4fvImmediate>();
  EXPECT_CALL(
      *gl_,
      VertexAttrib4fv(1,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<VertexAttrib4fvImmediate, 0>(true);
  GLfloat temp[4] = { 0, };
  cmd.Init(1, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
// TODO(gman): VertexAttribPointer


TEST_F(GLES2DecoderTest2, ViewportValidArgs) {
  EXPECT_CALL(*gl_, Viewport(1, 2, 3, 4));
  SpecializedSetup<Viewport, 0>(true);
  Viewport cmd;
  cmd.Init(1, 2, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, ViewportInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Viewport(_, _, _, _)).Times(0);
  SpecializedSetup<Viewport, 0>(false);
  Viewport cmd;
  cmd.Init(1, 2, -1, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderTest2, ViewportInvalidArgs3_0) {
  EXPECT_CALL(*gl_, Viewport(_, _, _, _)).Times(0);
  SpecializedSetup<Viewport, 0>(false);
  Viewport cmd;
  cmd.Init(1, 2, 3, -1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}
// TODO(gman): BlitFramebufferEXT
// TODO(gman): RenderbufferStorageMultisampleEXT
// TODO(gman): SwapBuffers
// TODO(gman): GetMaxValueInBufferCHROMIUM
// TODO(gman): GenSharedIdsCHROMIUM

// TODO(gman): DeleteSharedIdsCHROMIUM

// TODO(gman): RegisterSharedIdsCHROMIUM

// TODO(gman): CommandBufferEnableCHROMIUM

// TODO(gman): CopyTextureToParentTextureCHROMIUM
// TODO(gman): ResizeCHROMIUM
// TODO(gman): GetRequestableExtensionsCHROMIUM

// TODO(gman): RequestExtensionCHROMIUM

#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_2_AUTOGEN_H_

