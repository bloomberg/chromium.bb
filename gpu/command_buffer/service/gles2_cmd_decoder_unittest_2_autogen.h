// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// DO NOT EDIT!

// It is included by gles2_cmd_decoder_unittest_2.cc
#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_2_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_2_AUTOGEN_H_


TEST_F(GLES2DecoderTest2, GetVertexAttribivValidArgs) {
  SpecializedSetup<cmds::GetVertexAttribiv, 0>(true);
  typedef cmds::GetVertexAttribiv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetVertexAttribiv cmd;
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
  SpecializedSetup<cmds::GetVertexAttribiv, 0>(false);
  cmds::GetVertexAttribiv::Result* result =
      static_cast<cmds::GetVertexAttribiv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetVertexAttribiv cmd;
  cmd.Init(1, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_F(GLES2DecoderTest2, GetVertexAttribivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, GetVertexAttribiv(_, _, _)).Times(0);
  SpecializedSetup<cmds::GetVertexAttribiv, 0>(false);
  cmds::GetVertexAttribiv::Result* result =
      static_cast<cmds::GetVertexAttribiv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetVertexAttribiv cmd;
  cmd.Init(
      1, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, shared_memory_id_,
      kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}
// TODO(gman): GetVertexAttribPointerv


TEST_F(GLES2DecoderTest2, HintValidArgs) {
  EXPECT_CALL(*gl_, Hint(GL_GENERATE_MIPMAP_HINT, GL_FASTEST));
  SpecializedSetup<cmds::Hint, 0>(true);
  cmds::Hint cmd;
  cmd.Init(GL_GENERATE_MIPMAP_HINT, GL_FASTEST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, HintInvalidArgs0_0) {
  EXPECT_CALL(*gl_, Hint(_, _)).Times(0);
  SpecializedSetup<cmds::Hint, 0>(false);
  cmds::Hint cmd;
  cmd.Init(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, IsBufferValidArgs) {
  SpecializedSetup<cmds::IsBuffer, 0>(true);
  cmds::IsBuffer cmd;
  cmd.Init(client_buffer_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, IsBufferInvalidArgsBadSharedMemoryId) {
  SpecializedSetup<cmds::IsBuffer, 0>(false);
  cmds::IsBuffer cmd;
  cmd.Init(client_buffer_id_, kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(client_buffer_id_, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, IsEnabledValidArgs) {
  SpecializedSetup<cmds::IsEnabled, 0>(true);
  cmds::IsEnabled cmd;
  cmd.Init(GL_BLEND, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, IsEnabledInvalidArgs0_0) {
  EXPECT_CALL(*gl_, IsEnabled(_)).Times(0);
  SpecializedSetup<cmds::IsEnabled, 0>(false);
  cmds::IsEnabled cmd;
  cmd.Init(GL_CLIP_PLANE0, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, IsEnabledInvalidArgs0_1) {
  EXPECT_CALL(*gl_, IsEnabled(_)).Times(0);
  SpecializedSetup<cmds::IsEnabled, 0>(false);
  cmds::IsEnabled cmd;
  cmd.Init(GL_POINT_SPRITE, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, IsEnabledInvalidArgsBadSharedMemoryId) {
  SpecializedSetup<cmds::IsEnabled, 0>(false);
  cmds::IsEnabled cmd;
  cmd.Init(GL_BLEND, kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(GL_BLEND, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, IsFramebufferValidArgs) {
  SpecializedSetup<cmds::IsFramebuffer, 0>(true);
  cmds::IsFramebuffer cmd;
  cmd.Init(client_framebuffer_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, IsFramebufferInvalidArgsBadSharedMemoryId) {
  SpecializedSetup<cmds::IsFramebuffer, 0>(false);
  cmds::IsFramebuffer cmd;
  cmd.Init(
      client_framebuffer_id_, kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(
      client_framebuffer_id_, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, IsProgramValidArgs) {
  SpecializedSetup<cmds::IsProgram, 0>(true);
  cmds::IsProgram cmd;
  cmd.Init(client_program_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, IsProgramInvalidArgsBadSharedMemoryId) {
  SpecializedSetup<cmds::IsProgram, 0>(false);
  cmds::IsProgram cmd;
  cmd.Init(client_program_id_, kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, IsRenderbufferValidArgs) {
  SpecializedSetup<cmds::IsRenderbuffer, 0>(true);
  cmds::IsRenderbuffer cmd;
  cmd.Init(client_renderbuffer_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, IsRenderbufferInvalidArgsBadSharedMemoryId) {
  SpecializedSetup<cmds::IsRenderbuffer, 0>(false);
  cmds::IsRenderbuffer cmd;
  cmd.Init(
      client_renderbuffer_id_, kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(
      client_renderbuffer_id_, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, IsShaderValidArgs) {
  SpecializedSetup<cmds::IsShader, 0>(true);
  cmds::IsShader cmd;
  cmd.Init(client_shader_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, IsShaderInvalidArgsBadSharedMemoryId) {
  SpecializedSetup<cmds::IsShader, 0>(false);
  cmds::IsShader cmd;
  cmd.Init(client_shader_id_, kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(client_shader_id_, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, IsTextureValidArgs) {
  SpecializedSetup<cmds::IsTexture, 0>(true);
  cmds::IsTexture cmd;
  cmd.Init(client_texture_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, IsTextureInvalidArgsBadSharedMemoryId) {
  SpecializedSetup<cmds::IsTexture, 0>(false);
  cmds::IsTexture cmd;
  cmd.Init(client_texture_id_, kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(client_texture_id_, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, LineWidthValidArgs) {
  EXPECT_CALL(*gl_, LineWidth(0.5f));
  SpecializedSetup<cmds::LineWidth, 0>(true);
  cmds::LineWidth cmd;
  cmd.Init(0.5f);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, LineWidthInvalidValue0_0) {
  SpecializedSetup<cmds::LineWidth, 0>(false);
  cmds::LineWidth cmd;
  cmd.Init(0.0f);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderTest2, LinkProgramValidArgs) {
  EXPECT_CALL(*gl_, LinkProgram(kServiceProgramId));
  SpecializedSetup<cmds::LinkProgram, 0>(true);
  cmds::LinkProgram cmd;
  cmd.Init(client_program_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
// TODO(gman): PixelStorei


TEST_F(GLES2DecoderTest2, PolygonOffsetValidArgs) {
  EXPECT_CALL(*gl_, PolygonOffset(1, 2));
  SpecializedSetup<cmds::PolygonOffset, 0>(true);
  cmds::PolygonOffset cmd;
  cmd.Init(1, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
// TODO(gman): ReadPixels

// TODO(gman): ReleaseShaderCompiler

TEST_F(GLES2DecoderTest2, RenderbufferStorageValidArgs) {
  SpecializedSetup<cmds::RenderbufferStorage, 0>(true);
  cmds::RenderbufferStorage cmd;
  cmd.Init(GL_RENDERBUFFER, GL_RGBA4, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, RenderbufferStorageInvalidArgs0_0) {
  EXPECT_CALL(*gl_, RenderbufferStorageEXT(_, _, _, _)).Times(0);
  SpecializedSetup<cmds::RenderbufferStorage, 0>(false);
  cmds::RenderbufferStorage cmd;
  cmd.Init(GL_FRAMEBUFFER, GL_RGBA4, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, RenderbufferStorageInvalidArgs2_0) {
  EXPECT_CALL(*gl_, RenderbufferStorageEXT(_, _, _, _)).Times(0);
  SpecializedSetup<cmds::RenderbufferStorage, 0>(false);
  cmds::RenderbufferStorage cmd;
  cmd.Init(GL_RENDERBUFFER, GL_RGBA4, -1, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderTest2, RenderbufferStorageInvalidArgs3_0) {
  EXPECT_CALL(*gl_, RenderbufferStorageEXT(_, _, _, _)).Times(0);
  SpecializedSetup<cmds::RenderbufferStorage, 0>(false);
  cmds::RenderbufferStorage cmd;
  cmd.Init(GL_RENDERBUFFER, GL_RGBA4, 3, -1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderTest2, SampleCoverageValidArgs) {
  EXPECT_CALL(*gl_, SampleCoverage(1, true));
  SpecializedSetup<cmds::SampleCoverage, 0>(true);
  cmds::SampleCoverage cmd;
  cmd.Init(1, true);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, ScissorValidArgs) {
  EXPECT_CALL(*gl_, Scissor(1, 2, 3, 4));
  SpecializedSetup<cmds::Scissor, 0>(true);
  cmds::Scissor cmd;
  cmd.Init(1, 2, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, ScissorInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Scissor(_, _, _, _)).Times(0);
  SpecializedSetup<cmds::Scissor, 0>(false);
  cmds::Scissor cmd;
  cmd.Init(1, 2, -1, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderTest2, ScissorInvalidArgs3_0) {
  EXPECT_CALL(*gl_, Scissor(_, _, _, _)).Times(0);
  SpecializedSetup<cmds::Scissor, 0>(false);
  cmds::Scissor cmd;
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
  SpecializedSetup<cmds::StencilFunc, 0>(true);
  cmds::StencilFunc cmd;
  cmd.Init(GL_NEVER, 2, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, StencilFuncSeparateValidArgs) {
  EXPECT_CALL(*gl_, StencilFuncSeparate(GL_FRONT, GL_NEVER, 3, 4));
  SpecializedSetup<cmds::StencilFuncSeparate, 0>(true);
  cmds::StencilFuncSeparate cmd;
  cmd.Init(GL_FRONT, GL_NEVER, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, StencilMaskValidArgs) {
  SpecializedSetup<cmds::StencilMask, 0>(true);
  cmds::StencilMask cmd;
  cmd.Init(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, StencilMaskSeparateValidArgs) {
  SpecializedSetup<cmds::StencilMaskSeparate, 0>(true);
  cmds::StencilMaskSeparate cmd;
  cmd.Init(GL_FRONT, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, StencilOpValidArgs) {
  EXPECT_CALL(*gl_, StencilOp(GL_KEEP, GL_INCR, GL_KEEP));
  SpecializedSetup<cmds::StencilOp, 0>(true);
  cmds::StencilOp cmd;
  cmd.Init(GL_KEEP, GL_INCR, GL_KEEP);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, StencilOpSeparateValidArgs) {
  EXPECT_CALL(*gl_, StencilOpSeparate(GL_FRONT, GL_INCR, GL_KEEP, GL_KEEP));
  SpecializedSetup<cmds::StencilOpSeparate, 0>(true);
  cmds::StencilOpSeparate cmd;
  cmd.Init(GL_FRONT, GL_INCR, GL_KEEP, GL_KEEP);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
// TODO(gman): TexImage2D

// TODO(gman): TexImage2DImmediate


TEST_F(GLES2DecoderTest2, TexParameterfValidArgs) {
  EXPECT_CALL(
      *gl_, TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
  SpecializedSetup<cmds::TexParameterf, 0>(true);
  cmds::TexParameterf cmd;
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterfInvalidArgs0_0) {
  EXPECT_CALL(*gl_, TexParameterf(_, _, _)).Times(0);
  SpecializedSetup<cmds::TexParameterf, 0>(false);
  cmds::TexParameterf cmd;
  cmd.Init(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterfInvalidArgs0_1) {
  EXPECT_CALL(*gl_, TexParameterf(_, _, _)).Times(0);
  SpecializedSetup<cmds::TexParameterf, 0>(false);
  cmds::TexParameterf cmd;
  cmd.Init(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterfInvalidArgs1_0) {
  EXPECT_CALL(*gl_, TexParameterf(_, _, _)).Times(0);
  SpecializedSetup<cmds::TexParameterf, 0>(false);
  cmds::TexParameterf cmd;
  cmd.Init(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterfvValidArgs) {
  EXPECT_CALL(
      *gl_, TexParameterfv(
          GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
          reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<cmds::TexParameterfv, 0>(true);
  cmds::TexParameterfv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      shared_memory_offset_);
  GetSharedMemoryAs<GLfloat*>()[0] = GL_NEAREST;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterfvInvalidArgs0_0) {
  EXPECT_CALL(*gl_, TexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<cmds::TexParameterfv, 0>(false);
  cmds::TexParameterfv cmd;
  cmd.Init(
      GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      shared_memory_offset_);
  GetSharedMemoryAs<GLfloat*>()[0] = GL_NEAREST;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterfvInvalidArgs0_1) {
  EXPECT_CALL(*gl_, TexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<cmds::TexParameterfv, 0>(false);
  cmds::TexParameterfv cmd;
  cmd.Init(
      GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      shared_memory_offset_);
  GetSharedMemoryAs<GLfloat*>()[0] = GL_NEAREST;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterfvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, TexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<cmds::TexParameterfv, 0>(false);
  cmds::TexParameterfv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_GENERATE_MIPMAP, shared_memory_id_,
      shared_memory_offset_);
  GetSharedMemoryAs<GLfloat*>()[0] = GL_NEAREST;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterfvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, TexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<cmds::TexParameterfv, 0>(false);
  cmds::TexParameterfv cmd;
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, kInvalidSharedMemoryId, 0);
  GetSharedMemoryAs<GLfloat*>()[0] = GL_NEAREST;
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, TexParameterfvInvalidArgs2_1) {
  EXPECT_CALL(*gl_, TexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<cmds::TexParameterfv, 0>(false);
  cmds::TexParameterfv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      kInvalidSharedMemoryOffset);
  GetSharedMemoryAs<GLfloat*>()[0] = GL_NEAREST;
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, TexParameterfvImmediateValidArgs) {
  cmds::TexParameterfvImmediate& cmd =
      *GetImmediateAs<cmds::TexParameterfvImmediate>();
  EXPECT_CALL(
      *gl_,
      TexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<cmds::TexParameterfvImmediate, 0>(true);
  GLfloat temp[1] = { GL_NEAREST, };
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterfvImmediateInvalidArgs0_0) {
  cmds::TexParameterfvImmediate& cmd =
      *GetImmediateAs<cmds::TexParameterfvImmediate>();
  EXPECT_CALL(*gl_, TexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<cmds::TexParameterfvImmediate, 0>(false);
  GLfloat temp[1] = { GL_NEAREST, };
  cmd.Init(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterfvImmediateInvalidArgs0_1) {
  cmds::TexParameterfvImmediate& cmd =
      *GetImmediateAs<cmds::TexParameterfvImmediate>();
  EXPECT_CALL(*gl_, TexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<cmds::TexParameterfvImmediate, 0>(false);
  GLfloat temp[1] = { GL_NEAREST, };
  cmd.Init(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterfvImmediateInvalidArgs1_0) {
  cmds::TexParameterfvImmediate& cmd =
      *GetImmediateAs<cmds::TexParameterfvImmediate>();
  EXPECT_CALL(*gl_, TexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<cmds::TexParameterfvImmediate, 0>(false);
  GLfloat temp[1] = { GL_NEAREST, };
  cmd.Init(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameteriValidArgs) {
  EXPECT_CALL(
      *gl_, TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
  SpecializedSetup<cmds::TexParameteri, 0>(true);
  cmds::TexParameteri cmd;
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameteriInvalidArgs0_0) {
  EXPECT_CALL(*gl_, TexParameteri(_, _, _)).Times(0);
  SpecializedSetup<cmds::TexParameteri, 0>(false);
  cmds::TexParameteri cmd;
  cmd.Init(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameteriInvalidArgs0_1) {
  EXPECT_CALL(*gl_, TexParameteri(_, _, _)).Times(0);
  SpecializedSetup<cmds::TexParameteri, 0>(false);
  cmds::TexParameteri cmd;
  cmd.Init(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameteriInvalidArgs1_0) {
  EXPECT_CALL(*gl_, TexParameteri(_, _, _)).Times(0);
  SpecializedSetup<cmds::TexParameteri, 0>(false);
  cmds::TexParameteri cmd;
  cmd.Init(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterivValidArgs) {
  EXPECT_CALL(
      *gl_, TexParameteriv(
          GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, reinterpret_cast<const GLint*>(
              shared_memory_address_)));
  SpecializedSetup<cmds::TexParameteriv, 0>(true);
  cmds::TexParameteriv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      shared_memory_offset_);
  GetSharedMemoryAs<GLint*>()[0] = GL_NEAREST;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterivInvalidArgs0_0) {
  EXPECT_CALL(*gl_, TexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<cmds::TexParameteriv, 0>(false);
  cmds::TexParameteriv cmd;
  cmd.Init(
      GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      shared_memory_offset_);
  GetSharedMemoryAs<GLint*>()[0] = GL_NEAREST;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterivInvalidArgs0_1) {
  EXPECT_CALL(*gl_, TexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<cmds::TexParameteriv, 0>(false);
  cmds::TexParameteriv cmd;
  cmd.Init(
      GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      shared_memory_offset_);
  GetSharedMemoryAs<GLint*>()[0] = GL_NEAREST;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterivInvalidArgs1_0) {
  EXPECT_CALL(*gl_, TexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<cmds::TexParameteriv, 0>(false);
  cmds::TexParameteriv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_GENERATE_MIPMAP, shared_memory_id_,
      shared_memory_offset_);
  GetSharedMemoryAs<GLint*>()[0] = GL_NEAREST;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, TexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<cmds::TexParameteriv, 0>(false);
  cmds::TexParameteriv cmd;
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, kInvalidSharedMemoryId, 0);
  GetSharedMemoryAs<GLint*>()[0] = GL_NEAREST;
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, TexParameterivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, TexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<cmds::TexParameteriv, 0>(false);
  cmds::TexParameteriv cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
      kInvalidSharedMemoryOffset);
  GetSharedMemoryAs<GLint*>()[0] = GL_NEAREST;
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, TexParameterivImmediateValidArgs) {
  cmds::TexParameterivImmediate& cmd =
      *GetImmediateAs<cmds::TexParameterivImmediate>();
  EXPECT_CALL(
      *gl_,
      TexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
          reinterpret_cast<GLint*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<cmds::TexParameterivImmediate, 0>(true);
  GLint temp[1] = { GL_NEAREST, };
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterivImmediateInvalidArgs0_0) {
  cmds::TexParameterivImmediate& cmd =
      *GetImmediateAs<cmds::TexParameterivImmediate>();
  EXPECT_CALL(*gl_, TexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<cmds::TexParameterivImmediate, 0>(false);
  GLint temp[1] = { GL_NEAREST, };
  cmd.Init(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterivImmediateInvalidArgs0_1) {
  cmds::TexParameterivImmediate& cmd =
      *GetImmediateAs<cmds::TexParameterivImmediate>();
  EXPECT_CALL(*gl_, TexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<cmds::TexParameterivImmediate, 0>(false);
  GLint temp[1] = { GL_NEAREST, };
  cmd.Init(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderTest2, TexParameterivImmediateInvalidArgs1_0) {
  cmds::TexParameterivImmediate& cmd =
      *GetImmediateAs<cmds::TexParameterivImmediate>();
  EXPECT_CALL(*gl_, TexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<cmds::TexParameterivImmediate, 0>(false);
  GLint temp[1] = { GL_NEAREST, };
  cmd.Init(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}
// TODO(gman): TexSubImage2D

// TODO(gman): TexSubImage2DImmediate


TEST_F(GLES2DecoderTest2, Uniform1fValidArgs) {
  EXPECT_CALL(*gl_, Uniform1fv(1, 1, _));
  SpecializedSetup<cmds::Uniform1f, 0>(true);
  cmds::Uniform1f cmd;
  cmd.Init(1, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform1fvValidArgs) {
  EXPECT_CALL(
      *gl_, Uniform1fv(
          1, 2, reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<cmds::Uniform1fv, 0>(true);
  cmds::Uniform1fv cmd;
  cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform1fvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, Uniform1fv(_, _, _)).Times(0);
  SpecializedSetup<cmds::Uniform1fv, 0>(false);
  cmds::Uniform1fv cmd;
  cmd.Init(1, -1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform1fvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Uniform1fv(_, _, _)).Times(0);
  SpecializedSetup<cmds::Uniform1fv, 0>(false);
  cmds::Uniform1fv cmd;
  cmd.Init(1, 2, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, Uniform1fvInvalidArgs2_1) {
  EXPECT_CALL(*gl_, Uniform1fv(_, _, _)).Times(0);
  SpecializedSetup<cmds::Uniform1fv, 0>(false);
  cmds::Uniform1fv cmd;
  cmd.Init(1, 2, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, Uniform1fvValidArgsCountTooLarge) {
  EXPECT_CALL(
      *gl_, Uniform1fv(
          3, 3, reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<cmds::Uniform1fv, 0>(true);
  cmds::Uniform1fv cmd;
  cmd.Init(
      ProgramManager::MakeFakeLocation(
          1, 1), 5, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform1fvImmediateValidArgs) {
  cmds::Uniform1fvImmediate& cmd =
      *GetImmediateAs<cmds::Uniform1fvImmediate>();
  EXPECT_CALL(
      *gl_,
      Uniform1fv(1, 2,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<cmds::Uniform1fvImmediate, 0>(true);
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
  SpecializedSetup<cmds::Uniform2f, 0>(true);
  cmds::Uniform2f cmd;
  cmd.Init(1, 2, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform2fvValidArgs) {
  EXPECT_CALL(
      *gl_, Uniform2fv(
          1, 2, reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<cmds::Uniform2fv, 0>(true);
  cmds::Uniform2fv cmd;
  cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform2fvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, Uniform2fv(_, _, _)).Times(0);
  SpecializedSetup<cmds::Uniform2fv, 0>(false);
  cmds::Uniform2fv cmd;
  cmd.Init(1, -1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform2fvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Uniform2fv(_, _, _)).Times(0);
  SpecializedSetup<cmds::Uniform2fv, 0>(false);
  cmds::Uniform2fv cmd;
  cmd.Init(1, 2, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, Uniform2fvInvalidArgs2_1) {
  EXPECT_CALL(*gl_, Uniform2fv(_, _, _)).Times(0);
  SpecializedSetup<cmds::Uniform2fv, 0>(false);
  cmds::Uniform2fv cmd;
  cmd.Init(1, 2, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, Uniform2fvValidArgsCountTooLarge) {
  EXPECT_CALL(
      *gl_, Uniform2fv(
          3, 3, reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<cmds::Uniform2fv, 0>(true);
  cmds::Uniform2fv cmd;
  cmd.Init(
      ProgramManager::MakeFakeLocation(
          1, 1), 5, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform2fvImmediateValidArgs) {
  cmds::Uniform2fvImmediate& cmd =
      *GetImmediateAs<cmds::Uniform2fvImmediate>();
  EXPECT_CALL(
      *gl_,
      Uniform2fv(1, 2,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<cmds::Uniform2fvImmediate, 0>(true);
  GLfloat temp[2 * 2] = { 0, };
  cmd.Init(1, 2, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform2iValidArgs) {
  EXPECT_CALL(*gl_, Uniform2iv(1, 1, _));
  SpecializedSetup<cmds::Uniform2i, 0>(true);
  cmds::Uniform2i cmd;
  cmd.Init(1, 2, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform2ivValidArgs) {
  EXPECT_CALL(
      *gl_, Uniform2iv(
          1, 2, reinterpret_cast<const GLint*>(shared_memory_address_)));
  SpecializedSetup<cmds::Uniform2iv, 0>(true);
  cmds::Uniform2iv cmd;
  cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform2ivInvalidArgs1_0) {
  EXPECT_CALL(*gl_, Uniform2iv(_, _, _)).Times(0);
  SpecializedSetup<cmds::Uniform2iv, 0>(false);
  cmds::Uniform2iv cmd;
  cmd.Init(1, -1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform2ivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Uniform2iv(_, _, _)).Times(0);
  SpecializedSetup<cmds::Uniform2iv, 0>(false);
  cmds::Uniform2iv cmd;
  cmd.Init(1, 2, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, Uniform2ivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, Uniform2iv(_, _, _)).Times(0);
  SpecializedSetup<cmds::Uniform2iv, 0>(false);
  cmds::Uniform2iv cmd;
  cmd.Init(1, 2, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, Uniform2ivValidArgsCountTooLarge) {
  EXPECT_CALL(
      *gl_, Uniform2iv(
          3, 3, reinterpret_cast<const GLint*>(shared_memory_address_)));
  SpecializedSetup<cmds::Uniform2iv, 0>(true);
  cmds::Uniform2iv cmd;
  cmd.Init(
      ProgramManager::MakeFakeLocation(
          1, 1), 5, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform2ivImmediateValidArgs) {
  cmds::Uniform2ivImmediate& cmd =
      *GetImmediateAs<cmds::Uniform2ivImmediate>();
  EXPECT_CALL(
      *gl_,
      Uniform2iv(1, 2,
          reinterpret_cast<GLint*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<cmds::Uniform2ivImmediate, 0>(true);
  GLint temp[2 * 2] = { 0, };
  cmd.Init(1, 2, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform3fValidArgs) {
  EXPECT_CALL(*gl_, Uniform3fv(1, 1, _));
  SpecializedSetup<cmds::Uniform3f, 0>(true);
  cmds::Uniform3f cmd;
  cmd.Init(1, 2, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform3fvValidArgs) {
  EXPECT_CALL(
      *gl_, Uniform3fv(
          1, 2, reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<cmds::Uniform3fv, 0>(true);
  cmds::Uniform3fv cmd;
  cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform3fvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, Uniform3fv(_, _, _)).Times(0);
  SpecializedSetup<cmds::Uniform3fv, 0>(false);
  cmds::Uniform3fv cmd;
  cmd.Init(1, -1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform3fvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Uniform3fv(_, _, _)).Times(0);
  SpecializedSetup<cmds::Uniform3fv, 0>(false);
  cmds::Uniform3fv cmd;
  cmd.Init(1, 2, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, Uniform3fvInvalidArgs2_1) {
  EXPECT_CALL(*gl_, Uniform3fv(_, _, _)).Times(0);
  SpecializedSetup<cmds::Uniform3fv, 0>(false);
  cmds::Uniform3fv cmd;
  cmd.Init(1, 2, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, Uniform3fvValidArgsCountTooLarge) {
  EXPECT_CALL(
      *gl_, Uniform3fv(
          3, 3, reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<cmds::Uniform3fv, 0>(true);
  cmds::Uniform3fv cmd;
  cmd.Init(
      ProgramManager::MakeFakeLocation(
          1, 1), 5, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform3fvImmediateValidArgs) {
  cmds::Uniform3fvImmediate& cmd =
      *GetImmediateAs<cmds::Uniform3fvImmediate>();
  EXPECT_CALL(
      *gl_,
      Uniform3fv(1, 2,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<cmds::Uniform3fvImmediate, 0>(true);
  GLfloat temp[3 * 2] = { 0, };
  cmd.Init(1, 2, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform3iValidArgs) {
  EXPECT_CALL(*gl_, Uniform3iv(1, 1, _));
  SpecializedSetup<cmds::Uniform3i, 0>(true);
  cmds::Uniform3i cmd;
  cmd.Init(1, 2, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform3ivValidArgs) {
  EXPECT_CALL(
      *gl_, Uniform3iv(
          1, 2, reinterpret_cast<const GLint*>(shared_memory_address_)));
  SpecializedSetup<cmds::Uniform3iv, 0>(true);
  cmds::Uniform3iv cmd;
  cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform3ivInvalidArgs1_0) {
  EXPECT_CALL(*gl_, Uniform3iv(_, _, _)).Times(0);
  SpecializedSetup<cmds::Uniform3iv, 0>(false);
  cmds::Uniform3iv cmd;
  cmd.Init(1, -1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform3ivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Uniform3iv(_, _, _)).Times(0);
  SpecializedSetup<cmds::Uniform3iv, 0>(false);
  cmds::Uniform3iv cmd;
  cmd.Init(1, 2, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, Uniform3ivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, Uniform3iv(_, _, _)).Times(0);
  SpecializedSetup<cmds::Uniform3iv, 0>(false);
  cmds::Uniform3iv cmd;
  cmd.Init(1, 2, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, Uniform3ivValidArgsCountTooLarge) {
  EXPECT_CALL(
      *gl_, Uniform3iv(
          3, 3, reinterpret_cast<const GLint*>(shared_memory_address_)));
  SpecializedSetup<cmds::Uniform3iv, 0>(true);
  cmds::Uniform3iv cmd;
  cmd.Init(
      ProgramManager::MakeFakeLocation(
          1, 1), 5, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform3ivImmediateValidArgs) {
  cmds::Uniform3ivImmediate& cmd =
      *GetImmediateAs<cmds::Uniform3ivImmediate>();
  EXPECT_CALL(
      *gl_,
      Uniform3iv(1, 2,
          reinterpret_cast<GLint*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<cmds::Uniform3ivImmediate, 0>(true);
  GLint temp[3 * 2] = { 0, };
  cmd.Init(1, 2, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform4fValidArgs) {
  EXPECT_CALL(*gl_, Uniform4fv(1, 1, _));
  SpecializedSetup<cmds::Uniform4f, 0>(true);
  cmds::Uniform4f cmd;
  cmd.Init(1, 2, 3, 4, 5);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform4fvValidArgs) {
  EXPECT_CALL(
      *gl_, Uniform4fv(
          1, 2, reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<cmds::Uniform4fv, 0>(true);
  cmds::Uniform4fv cmd;
  cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform4fvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, Uniform4fv(_, _, _)).Times(0);
  SpecializedSetup<cmds::Uniform4fv, 0>(false);
  cmds::Uniform4fv cmd;
  cmd.Init(1, -1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform4fvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Uniform4fv(_, _, _)).Times(0);
  SpecializedSetup<cmds::Uniform4fv, 0>(false);
  cmds::Uniform4fv cmd;
  cmd.Init(1, 2, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, Uniform4fvInvalidArgs2_1) {
  EXPECT_CALL(*gl_, Uniform4fv(_, _, _)).Times(0);
  SpecializedSetup<cmds::Uniform4fv, 0>(false);
  cmds::Uniform4fv cmd;
  cmd.Init(1, 2, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, Uniform4fvValidArgsCountTooLarge) {
  EXPECT_CALL(
      *gl_, Uniform4fv(
          3, 3, reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<cmds::Uniform4fv, 0>(true);
  cmds::Uniform4fv cmd;
  cmd.Init(
      ProgramManager::MakeFakeLocation(
          1, 1), 5, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform4fvImmediateValidArgs) {
  cmds::Uniform4fvImmediate& cmd =
      *GetImmediateAs<cmds::Uniform4fvImmediate>();
  EXPECT_CALL(
      *gl_,
      Uniform4fv(1, 2,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<cmds::Uniform4fvImmediate, 0>(true);
  GLfloat temp[4 * 2] = { 0, };
  cmd.Init(1, 2, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform4iValidArgs) {
  EXPECT_CALL(*gl_, Uniform4iv(1, 1, _));
  SpecializedSetup<cmds::Uniform4i, 0>(true);
  cmds::Uniform4i cmd;
  cmd.Init(1, 2, 3, 4, 5);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform4ivValidArgs) {
  EXPECT_CALL(
      *gl_, Uniform4iv(
          1, 2, reinterpret_cast<const GLint*>(shared_memory_address_)));
  SpecializedSetup<cmds::Uniform4iv, 0>(true);
  cmds::Uniform4iv cmd;
  cmd.Init(1, 2, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform4ivInvalidArgs1_0) {
  EXPECT_CALL(*gl_, Uniform4iv(_, _, _)).Times(0);
  SpecializedSetup<cmds::Uniform4iv, 0>(false);
  cmds::Uniform4iv cmd;
  cmd.Init(1, -1, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform4ivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Uniform4iv(_, _, _)).Times(0);
  SpecializedSetup<cmds::Uniform4iv, 0>(false);
  cmds::Uniform4iv cmd;
  cmd.Init(1, 2, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, Uniform4ivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, Uniform4iv(_, _, _)).Times(0);
  SpecializedSetup<cmds::Uniform4iv, 0>(false);
  cmds::Uniform4iv cmd;
  cmd.Init(1, 2, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, Uniform4ivValidArgsCountTooLarge) {
  EXPECT_CALL(
      *gl_, Uniform4iv(
          3, 3, reinterpret_cast<const GLint*>(shared_memory_address_)));
  SpecializedSetup<cmds::Uniform4iv, 0>(true);
  cmds::Uniform4iv cmd;
  cmd.Init(
      ProgramManager::MakeFakeLocation(
          1, 1), 5, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, Uniform4ivImmediateValidArgs) {
  cmds::Uniform4ivImmediate& cmd =
      *GetImmediateAs<cmds::Uniform4ivImmediate>();
  EXPECT_CALL(
      *gl_,
      Uniform4iv(1, 2,
          reinterpret_cast<GLint*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<cmds::Uniform4ivImmediate, 0>(true);
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
  SpecializedSetup<cmds::UniformMatrix2fv, 0>(true);
  cmds::UniformMatrix2fv cmd;
  cmd.Init(1, 2, false, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix2fvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, UniformMatrix2fv(_, _, _, _)).Times(0);
  SpecializedSetup<cmds::UniformMatrix2fv, 0>(false);
  cmds::UniformMatrix2fv cmd;
  cmd.Init(1, -1, false, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix2fvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, UniformMatrix2fv(_, _, _, _)).Times(0);
  SpecializedSetup<cmds::UniformMatrix2fv, 0>(false);
  cmds::UniformMatrix2fv cmd;
  cmd.Init(1, 2, true, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix2fvInvalidArgs3_0) {
  EXPECT_CALL(*gl_, UniformMatrix2fv(_, _, _, _)).Times(0);
  SpecializedSetup<cmds::UniformMatrix2fv, 0>(false);
  cmds::UniformMatrix2fv cmd;
  cmd.Init(1, 2, false, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, UniformMatrix2fvInvalidArgs3_1) {
  EXPECT_CALL(*gl_, UniformMatrix2fv(_, _, _, _)).Times(0);
  SpecializedSetup<cmds::UniformMatrix2fv, 0>(false);
  cmds::UniformMatrix2fv cmd;
  cmd.Init(1, 2, false, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, UniformMatrix2fvValidArgsCountTooLarge) {
  EXPECT_CALL(
      *gl_, UniformMatrix2fv(
          3, 3, false, reinterpret_cast<const GLfloat*>(
              shared_memory_address_)));
  SpecializedSetup<cmds::UniformMatrix2fv, 0>(true);
  cmds::UniformMatrix2fv cmd;
  cmd.Init(
      ProgramManager::MakeFakeLocation(
          1, 1), 5, false, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix2fvImmediateValidArgs) {
  cmds::UniformMatrix2fvImmediate& cmd =
      *GetImmediateAs<cmds::UniformMatrix2fvImmediate>();
  EXPECT_CALL(
      *gl_,
      UniformMatrix2fv(1, 2, false,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<cmds::UniformMatrix2fvImmediate, 0>(true);
  GLfloat temp[4 * 2] = { 0, };
  cmd.Init(1, 2, false, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix2fvImmediateInvalidArgs2_0) {
  cmds::UniformMatrix2fvImmediate& cmd =
      *GetImmediateAs<cmds::UniformMatrix2fvImmediate>();
  EXPECT_CALL(*gl_, UniformMatrix2fv(_, _, _, _)).Times(0);
  SpecializedSetup<cmds::UniformMatrix2fvImmediate, 0>(false);
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
  SpecializedSetup<cmds::UniformMatrix3fv, 0>(true);
  cmds::UniformMatrix3fv cmd;
  cmd.Init(1, 2, false, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix3fvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, UniformMatrix3fv(_, _, _, _)).Times(0);
  SpecializedSetup<cmds::UniformMatrix3fv, 0>(false);
  cmds::UniformMatrix3fv cmd;
  cmd.Init(1, -1, false, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix3fvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, UniformMatrix3fv(_, _, _, _)).Times(0);
  SpecializedSetup<cmds::UniformMatrix3fv, 0>(false);
  cmds::UniformMatrix3fv cmd;
  cmd.Init(1, 2, true, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix3fvInvalidArgs3_0) {
  EXPECT_CALL(*gl_, UniformMatrix3fv(_, _, _, _)).Times(0);
  SpecializedSetup<cmds::UniformMatrix3fv, 0>(false);
  cmds::UniformMatrix3fv cmd;
  cmd.Init(1, 2, false, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, UniformMatrix3fvInvalidArgs3_1) {
  EXPECT_CALL(*gl_, UniformMatrix3fv(_, _, _, _)).Times(0);
  SpecializedSetup<cmds::UniformMatrix3fv, 0>(false);
  cmds::UniformMatrix3fv cmd;
  cmd.Init(1, 2, false, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, UniformMatrix3fvValidArgsCountTooLarge) {
  EXPECT_CALL(
      *gl_, UniformMatrix3fv(
          3, 3, false, reinterpret_cast<const GLfloat*>(
              shared_memory_address_)));
  SpecializedSetup<cmds::UniformMatrix3fv, 0>(true);
  cmds::UniformMatrix3fv cmd;
  cmd.Init(
      ProgramManager::MakeFakeLocation(
          1, 1), 5, false, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix3fvImmediateValidArgs) {
  cmds::UniformMatrix3fvImmediate& cmd =
      *GetImmediateAs<cmds::UniformMatrix3fvImmediate>();
  EXPECT_CALL(
      *gl_,
      UniformMatrix3fv(1, 2, false,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<cmds::UniformMatrix3fvImmediate, 0>(true);
  GLfloat temp[9 * 2] = { 0, };
  cmd.Init(1, 2, false, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix3fvImmediateInvalidArgs2_0) {
  cmds::UniformMatrix3fvImmediate& cmd =
      *GetImmediateAs<cmds::UniformMatrix3fvImmediate>();
  EXPECT_CALL(*gl_, UniformMatrix3fv(_, _, _, _)).Times(0);
  SpecializedSetup<cmds::UniformMatrix3fvImmediate, 0>(false);
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
  SpecializedSetup<cmds::UniformMatrix4fv, 0>(true);
  cmds::UniformMatrix4fv cmd;
  cmd.Init(1, 2, false, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix4fvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, UniformMatrix4fv(_, _, _, _)).Times(0);
  SpecializedSetup<cmds::UniformMatrix4fv, 0>(false);
  cmds::UniformMatrix4fv cmd;
  cmd.Init(1, -1, false, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix4fvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, UniformMatrix4fv(_, _, _, _)).Times(0);
  SpecializedSetup<cmds::UniformMatrix4fv, 0>(false);
  cmds::UniformMatrix4fv cmd;
  cmd.Init(1, 2, true, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix4fvInvalidArgs3_0) {
  EXPECT_CALL(*gl_, UniformMatrix4fv(_, _, _, _)).Times(0);
  SpecializedSetup<cmds::UniformMatrix4fv, 0>(false);
  cmds::UniformMatrix4fv cmd;
  cmd.Init(1, 2, false, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, UniformMatrix4fvInvalidArgs3_1) {
  EXPECT_CALL(*gl_, UniformMatrix4fv(_, _, _, _)).Times(0);
  SpecializedSetup<cmds::UniformMatrix4fv, 0>(false);
  cmds::UniformMatrix4fv cmd;
  cmd.Init(1, 2, false, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, UniformMatrix4fvValidArgsCountTooLarge) {
  EXPECT_CALL(
      *gl_, UniformMatrix4fv(
          3, 3, false, reinterpret_cast<const GLfloat*>(
              shared_memory_address_)));
  SpecializedSetup<cmds::UniformMatrix4fv, 0>(true);
  cmds::UniformMatrix4fv cmd;
  cmd.Init(
      ProgramManager::MakeFakeLocation(
          1, 1), 5, false, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix4fvImmediateValidArgs) {
  cmds::UniformMatrix4fvImmediate& cmd =
      *GetImmediateAs<cmds::UniformMatrix4fvImmediate>();
  EXPECT_CALL(
      *gl_,
      UniformMatrix4fv(1, 2, false,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<cmds::UniformMatrix4fvImmediate, 0>(true);
  GLfloat temp[16 * 2] = { 0, };
  cmd.Init(1, 2, false, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, UniformMatrix4fvImmediateInvalidArgs2_0) {
  cmds::UniformMatrix4fvImmediate& cmd =
      *GetImmediateAs<cmds::UniformMatrix4fvImmediate>();
  EXPECT_CALL(*gl_, UniformMatrix4fv(_, _, _, _)).Times(0);
  SpecializedSetup<cmds::UniformMatrix4fvImmediate, 0>(false);
  GLfloat temp[16 * 2] = { 0, };
  cmd.Init(1, 2, true, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}
// TODO(gman): UseProgram

TEST_F(GLES2DecoderTest2, ValidateProgramValidArgs) {
  EXPECT_CALL(*gl_, ValidateProgram(kServiceProgramId));
  SpecializedSetup<cmds::ValidateProgram, 0>(true);
  cmds::ValidateProgram cmd;
  cmd.Init(client_program_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, VertexAttrib1fValidArgs) {
  EXPECT_CALL(*gl_, VertexAttrib1f(1, 2));
  SpecializedSetup<cmds::VertexAttrib1f, 0>(true);
  cmds::VertexAttrib1f cmd;
  cmd.Init(1, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, VertexAttrib1fvValidArgs) {
  EXPECT_CALL(
      *gl_, VertexAttrib1fv(
          1, reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<cmds::VertexAttrib1fv, 0>(true);
  cmds::VertexAttrib1fv cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  GetSharedMemoryAs<GLfloat*>()[0] = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, VertexAttrib1fvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, VertexAttrib1fv(_, _)).Times(0);
  SpecializedSetup<cmds::VertexAttrib1fv, 0>(false);
  cmds::VertexAttrib1fv cmd;
  cmd.Init(1, kInvalidSharedMemoryId, 0);
  GetSharedMemoryAs<GLfloat*>()[0] = 0;
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, VertexAttrib1fvInvalidArgs1_1) {
  EXPECT_CALL(*gl_, VertexAttrib1fv(_, _)).Times(0);
  SpecializedSetup<cmds::VertexAttrib1fv, 0>(false);
  cmds::VertexAttrib1fv cmd;
  cmd.Init(1, shared_memory_id_, kInvalidSharedMemoryOffset);
  GetSharedMemoryAs<GLfloat*>()[0] = 0;
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, VertexAttrib1fvImmediateValidArgs) {
  cmds::VertexAttrib1fvImmediate& cmd =
      *GetImmediateAs<cmds::VertexAttrib1fvImmediate>();
  EXPECT_CALL(
      *gl_,
      VertexAttrib1fv(1,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<cmds::VertexAttrib1fvImmediate, 0>(true);
  GLfloat temp[1] = { 0, };
  cmd.Init(1, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, VertexAttrib2fValidArgs) {
  EXPECT_CALL(*gl_, VertexAttrib2f(1, 2, 3));
  SpecializedSetup<cmds::VertexAttrib2f, 0>(true);
  cmds::VertexAttrib2f cmd;
  cmd.Init(1, 2, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, VertexAttrib2fvValidArgs) {
  EXPECT_CALL(
      *gl_, VertexAttrib2fv(
          1, reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<cmds::VertexAttrib2fv, 0>(true);
  cmds::VertexAttrib2fv cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  GetSharedMemoryAs<GLfloat*>()[0] = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, VertexAttrib2fvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, VertexAttrib2fv(_, _)).Times(0);
  SpecializedSetup<cmds::VertexAttrib2fv, 0>(false);
  cmds::VertexAttrib2fv cmd;
  cmd.Init(1, kInvalidSharedMemoryId, 0);
  GetSharedMemoryAs<GLfloat*>()[0] = 0;
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, VertexAttrib2fvInvalidArgs1_1) {
  EXPECT_CALL(*gl_, VertexAttrib2fv(_, _)).Times(0);
  SpecializedSetup<cmds::VertexAttrib2fv, 0>(false);
  cmds::VertexAttrib2fv cmd;
  cmd.Init(1, shared_memory_id_, kInvalidSharedMemoryOffset);
  GetSharedMemoryAs<GLfloat*>()[0] = 0;
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, VertexAttrib2fvImmediateValidArgs) {
  cmds::VertexAttrib2fvImmediate& cmd =
      *GetImmediateAs<cmds::VertexAttrib2fvImmediate>();
  EXPECT_CALL(
      *gl_,
      VertexAttrib2fv(1,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<cmds::VertexAttrib2fvImmediate, 0>(true);
  GLfloat temp[2] = { 0, };
  cmd.Init(1, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, VertexAttrib3fValidArgs) {
  EXPECT_CALL(*gl_, VertexAttrib3f(1, 2, 3, 4));
  SpecializedSetup<cmds::VertexAttrib3f, 0>(true);
  cmds::VertexAttrib3f cmd;
  cmd.Init(1, 2, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, VertexAttrib3fvValidArgs) {
  EXPECT_CALL(
      *gl_, VertexAttrib3fv(
          1, reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<cmds::VertexAttrib3fv, 0>(true);
  cmds::VertexAttrib3fv cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  GetSharedMemoryAs<GLfloat*>()[0] = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, VertexAttrib3fvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, VertexAttrib3fv(_, _)).Times(0);
  SpecializedSetup<cmds::VertexAttrib3fv, 0>(false);
  cmds::VertexAttrib3fv cmd;
  cmd.Init(1, kInvalidSharedMemoryId, 0);
  GetSharedMemoryAs<GLfloat*>()[0] = 0;
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, VertexAttrib3fvInvalidArgs1_1) {
  EXPECT_CALL(*gl_, VertexAttrib3fv(_, _)).Times(0);
  SpecializedSetup<cmds::VertexAttrib3fv, 0>(false);
  cmds::VertexAttrib3fv cmd;
  cmd.Init(1, shared_memory_id_, kInvalidSharedMemoryOffset);
  GetSharedMemoryAs<GLfloat*>()[0] = 0;
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, VertexAttrib3fvImmediateValidArgs) {
  cmds::VertexAttrib3fvImmediate& cmd =
      *GetImmediateAs<cmds::VertexAttrib3fvImmediate>();
  EXPECT_CALL(
      *gl_,
      VertexAttrib3fv(1,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<cmds::VertexAttrib3fvImmediate, 0>(true);
  GLfloat temp[3] = { 0, };
  cmd.Init(1, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, VertexAttrib4fValidArgs) {
  EXPECT_CALL(*gl_, VertexAttrib4f(1, 2, 3, 4, 5));
  SpecializedSetup<cmds::VertexAttrib4f, 0>(true);
  cmds::VertexAttrib4f cmd;
  cmd.Init(1, 2, 3, 4, 5);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, VertexAttrib4fvValidArgs) {
  EXPECT_CALL(
      *gl_, VertexAttrib4fv(
          1, reinterpret_cast<const GLfloat*>(shared_memory_address_)));
  SpecializedSetup<cmds::VertexAttrib4fv, 0>(true);
  cmds::VertexAttrib4fv cmd;
  cmd.Init(1, shared_memory_id_, shared_memory_offset_);
  GetSharedMemoryAs<GLfloat*>()[0] = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, VertexAttrib4fvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, VertexAttrib4fv(_, _)).Times(0);
  SpecializedSetup<cmds::VertexAttrib4fv, 0>(false);
  cmds::VertexAttrib4fv cmd;
  cmd.Init(1, kInvalidSharedMemoryId, 0);
  GetSharedMemoryAs<GLfloat*>()[0] = 0;
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, VertexAttrib4fvInvalidArgs1_1) {
  EXPECT_CALL(*gl_, VertexAttrib4fv(_, _)).Times(0);
  SpecializedSetup<cmds::VertexAttrib4fv, 0>(false);
  cmds::VertexAttrib4fv cmd;
  cmd.Init(1, shared_memory_id_, kInvalidSharedMemoryOffset);
  GetSharedMemoryAs<GLfloat*>()[0] = 0;
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderTest2, VertexAttrib4fvImmediateValidArgs) {
  cmds::VertexAttrib4fvImmediate& cmd =
      *GetImmediateAs<cmds::VertexAttrib4fvImmediate>();
  EXPECT_CALL(
      *gl_,
      VertexAttrib4fv(1,
          reinterpret_cast<GLfloat*>(ImmediateDataAddress(&cmd))));
  SpecializedSetup<cmds::VertexAttrib4fvImmediate, 0>(true);
  GLfloat temp[4] = { 0, };
  cmd.Init(1, &temp[0]);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
// TODO(gman): VertexAttribPointer


TEST_F(GLES2DecoderTest2, ViewportValidArgs) {
  EXPECT_CALL(*gl_, Viewport(1, 2, 3, 4));
  SpecializedSetup<cmds::Viewport, 0>(true);
  cmds::Viewport cmd;
  cmd.Init(1, 2, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderTest2, ViewportInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Viewport(_, _, _, _)).Times(0);
  SpecializedSetup<cmds::Viewport, 0>(false);
  cmds::Viewport cmd;
  cmd.Init(1, 2, -1, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderTest2, ViewportInvalidArgs3_0) {
  EXPECT_CALL(*gl_, Viewport(_, _, _, _)).Times(0);
  SpecializedSetup<cmds::Viewport, 0>(false);
  cmds::Viewport cmd;
  cmd.Init(1, 2, 3, -1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}
// TODO(gman): BlitFramebufferEXT
// TODO(gman): RenderbufferStorageMultisampleEXT
// TODO(gman): TexStorage2DEXT
// TODO(gman): GenQueriesEXT
// TODO(gman): GenQueriesEXTImmediate
// TODO(gman): DeleteQueriesEXT
// TODO(gman): DeleteQueriesEXTImmediate
// TODO(gman): BeginQueryEXT

// TODO(gman): EndQueryEXT

// TODO(gman): InsertEventMarkerEXT

// TODO(gman): PushGroupMarkerEXT


TEST_F(GLES2DecoderTest2, PopGroupMarkerEXTValidArgs) {
  SpecializedSetup<cmds::PopGroupMarkerEXT, 0>(true);
  cmds::PopGroupMarkerEXT cmd;
  cmd.Init();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
// TODO(gman): GenVertexArraysOES
#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_2_AUTOGEN_H_

