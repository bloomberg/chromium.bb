// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// DO NOT EDIT!

// This file contains unit tests for gles2 commmands
// It is included by gles2_cmd_format_test.cc

#ifndef GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_FORMAT_TEST_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_FORMAT_TEST_AUTOGEN_H_

TEST_F(GLES2FormatTest, ActiveTexture) {
  ActiveTexture& cmd = *GetBufferAs<ActiveTexture>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32>(ActiveTexture::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.texture);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, AttachShader) {
  AttachShader& cmd = *GetBufferAs<AttachShader>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32>(AttachShader::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.shader);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BindAttribLocation) {
  BindAttribLocation& cmd = *GetBufferAs<BindAttribLocation>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLuint>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14),
      static_cast<uint32>(15));
  EXPECT_EQ(static_cast<uint32>(BindAttribLocation::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.index);
  EXPECT_EQ(static_cast<uint32>(13), cmd.name_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.name_shm_offset);
  EXPECT_EQ(static_cast<uint32>(15), cmd.data_size);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}


TEST_F(GLES2FormatTest, BindAttribLocationImmediate) {
  BindAttribLocationImmediate& cmd =
      *GetBufferAs<BindAttribLocationImmediate>();
  static const char* const test_str = "test string";
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLuint>(12),
      test_str,
      strlen(test_str));
  EXPECT_EQ(static_cast<uint32>(BindAttribLocationImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(strlen(test_str)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +
                RoundSizeToMultipleOfEntries(strlen(test_str)));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.index);
  EXPECT_EQ(static_cast<uint32>(strlen(test_str)), cmd.data_size);
  EXPECT_EQ(0, memcmp(test_str, ImmediateDataAddress(&cmd), strlen(test_str)));
  CheckBytesWritten(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(strlen(test_str)),
      sizeof(cmd) + strlen(test_str));
}

TEST_F(GLES2FormatTest, BindAttribLocationBucket) {
  BindAttribLocationBucket& cmd = *GetBufferAs<BindAttribLocationBucket>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLuint>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(BindAttribLocationBucket::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.index);
  EXPECT_EQ(static_cast<uint32>(13), cmd.name_bucket_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BindBuffer) {
  BindBuffer& cmd = *GetBufferAs<BindBuffer>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32>(BindBuffer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.buffer);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BindFramebuffer) {
  BindFramebuffer& cmd = *GetBufferAs<BindFramebuffer>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32>(BindFramebuffer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.framebuffer);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BindRenderbuffer) {
  BindRenderbuffer& cmd = *GetBufferAs<BindRenderbuffer>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32>(BindRenderbuffer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.renderbuffer);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BindTexture) {
  BindTexture& cmd = *GetBufferAs<BindTexture>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32>(BindTexture::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.texture);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BlendColor) {
  BlendColor& cmd = *GetBufferAs<BlendColor>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLclampf>(11),
      static_cast<GLclampf>(12),
      static_cast<GLclampf>(13),
      static_cast<GLclampf>(14));
  EXPECT_EQ(static_cast<uint32>(BlendColor::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLclampf>(11), cmd.red);
  EXPECT_EQ(static_cast<GLclampf>(12), cmd.green);
  EXPECT_EQ(static_cast<GLclampf>(13), cmd.blue);
  EXPECT_EQ(static_cast<GLclampf>(14), cmd.alpha);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BlendEquation) {
  BlendEquation& cmd = *GetBufferAs<BlendEquation>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32>(BlendEquation::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BlendEquationSeparate) {
  BlendEquationSeparate& cmd = *GetBufferAs<BlendEquationSeparate>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12));
  EXPECT_EQ(static_cast<uint32>(BlendEquationSeparate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.modeRGB);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.modeAlpha);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BlendFunc) {
  BlendFunc& cmd = *GetBufferAs<BlendFunc>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12));
  EXPECT_EQ(static_cast<uint32>(BlendFunc::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.sfactor);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.dfactor);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BlendFuncSeparate) {
  BlendFuncSeparate& cmd = *GetBufferAs<BlendFuncSeparate>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLenum>(13),
      static_cast<GLenum>(14));
  EXPECT_EQ(static_cast<uint32>(BlendFuncSeparate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.srcRGB);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.dstRGB);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.srcAlpha);
  EXPECT_EQ(static_cast<GLenum>(14), cmd.dstAlpha);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BufferData) {
  BufferData& cmd = *GetBufferAs<BufferData>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLsizeiptr>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14),
      static_cast<GLenum>(15));
  EXPECT_EQ(static_cast<uint32>(BufferData::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLsizeiptr>(12), cmd.size);
  EXPECT_EQ(static_cast<uint32>(13), cmd.data_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.data_shm_offset);
  EXPECT_EQ(static_cast<GLenum>(15), cmd.usage);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

// TODO(gman): Implement test for BufferDataImmediate
TEST_F(GLES2FormatTest, BufferSubData) {
  BufferSubData& cmd = *GetBufferAs<BufferSubData>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLintptr>(12),
      static_cast<GLsizeiptr>(13),
      static_cast<uint32>(14),
      static_cast<uint32>(15));
  EXPECT_EQ(static_cast<uint32>(BufferSubData::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLintptr>(12), cmd.offset);
  EXPECT_EQ(static_cast<GLsizeiptr>(13), cmd.size);
  EXPECT_EQ(static_cast<uint32>(14), cmd.data_shm_id);
  EXPECT_EQ(static_cast<uint32>(15), cmd.data_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

// TODO(gman): Implement test for BufferSubDataImmediate
TEST_F(GLES2FormatTest, CheckFramebufferStatus) {
  CheckFramebufferStatus& cmd = *GetBufferAs<CheckFramebufferStatus>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(CheckFramebufferStatus::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Clear) {
  Clear& cmd = *GetBufferAs<Clear>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLbitfield>(11));
  EXPECT_EQ(static_cast<uint32>(Clear::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLbitfield>(11), cmd.mask);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ClearColor) {
  ClearColor& cmd = *GetBufferAs<ClearColor>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLclampf>(11),
      static_cast<GLclampf>(12),
      static_cast<GLclampf>(13),
      static_cast<GLclampf>(14));
  EXPECT_EQ(static_cast<uint32>(ClearColor::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLclampf>(11), cmd.red);
  EXPECT_EQ(static_cast<GLclampf>(12), cmd.green);
  EXPECT_EQ(static_cast<GLclampf>(13), cmd.blue);
  EXPECT_EQ(static_cast<GLclampf>(14), cmd.alpha);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ClearDepthf) {
  ClearDepthf& cmd = *GetBufferAs<ClearDepthf>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLclampf>(11));
  EXPECT_EQ(static_cast<uint32>(ClearDepthf::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLclampf>(11), cmd.depth);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ClearStencil) {
  ClearStencil& cmd = *GetBufferAs<ClearStencil>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11));
  EXPECT_EQ(static_cast<uint32>(ClearStencil::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.s);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ColorMask) {
  ColorMask& cmd = *GetBufferAs<ColorMask>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLboolean>(11),
      static_cast<GLboolean>(12),
      static_cast<GLboolean>(13),
      static_cast<GLboolean>(14));
  EXPECT_EQ(static_cast<uint32>(ColorMask::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLboolean>(11), cmd.red);
  EXPECT_EQ(static_cast<GLboolean>(12), cmd.green);
  EXPECT_EQ(static_cast<GLboolean>(13), cmd.blue);
  EXPECT_EQ(static_cast<GLboolean>(14), cmd.alpha);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CompileShader) {
  CompileShader& cmd = *GetBufferAs<CompileShader>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(CompileShader::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CompressedTexImage2D) {
  CompressedTexImage2D& cmd = *GetBufferAs<CompressedTexImage2D>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12),
      static_cast<GLenum>(13),
      static_cast<GLsizei>(14),
      static_cast<GLsizei>(15),
      static_cast<GLint>(16),
      static_cast<GLsizei>(17),
      static_cast<uint32>(18),
      static_cast<uint32>(19));
  EXPECT_EQ(static_cast<uint32>(CompressedTexImage2D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.internalformat);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.height);
  EXPECT_EQ(static_cast<GLint>(16), cmd.border);
  EXPECT_EQ(static_cast<GLsizei>(17), cmd.imageSize);
  EXPECT_EQ(static_cast<uint32>(18), cmd.data_shm_id);
  EXPECT_EQ(static_cast<uint32>(19), cmd.data_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

// TODO(gman): Implement test for CompressedTexImage2DImmediate
TEST_F(GLES2FormatTest, CompressedTexImage2DBucket) {
  CompressedTexImage2DBucket& cmd = *GetBufferAs<CompressedTexImage2DBucket>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12),
      static_cast<GLenum>(13),
      static_cast<GLsizei>(14),
      static_cast<GLsizei>(15),
      static_cast<GLint>(16),
      static_cast<GLuint>(17));
  EXPECT_EQ(static_cast<uint32>(CompressedTexImage2DBucket::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.internalformat);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.height);
  EXPECT_EQ(static_cast<GLint>(16), cmd.border);
  EXPECT_EQ(static_cast<GLuint>(17), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CompressedTexSubImage2D) {
  CompressedTexSubImage2D& cmd = *GetBufferAs<CompressedTexSubImage2D>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12),
      static_cast<GLint>(13),
      static_cast<GLint>(14),
      static_cast<GLsizei>(15),
      static_cast<GLsizei>(16),
      static_cast<GLenum>(17),
      static_cast<GLsizei>(18),
      static_cast<uint32>(19),
      static_cast<uint32>(20));
  EXPECT_EQ(static_cast<uint32>(CompressedTexSubImage2D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLint>(13), cmd.xoffset);
  EXPECT_EQ(static_cast<GLint>(14), cmd.yoffset);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.height);
  EXPECT_EQ(static_cast<GLenum>(17), cmd.format);
  EXPECT_EQ(static_cast<GLsizei>(18), cmd.imageSize);
  EXPECT_EQ(static_cast<uint32>(19), cmd.data_shm_id);
  EXPECT_EQ(static_cast<uint32>(20), cmd.data_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

// TODO(gman): Implement test for CompressedTexSubImage2DImmediate
TEST_F(GLES2FormatTest, CompressedTexSubImage2DBucket) {
  CompressedTexSubImage2DBucket& cmd =
      *GetBufferAs<CompressedTexSubImage2DBucket>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12),
      static_cast<GLint>(13),
      static_cast<GLint>(14),
      static_cast<GLsizei>(15),
      static_cast<GLsizei>(16),
      static_cast<GLenum>(17),
      static_cast<GLuint>(18));
  EXPECT_EQ(static_cast<uint32>(CompressedTexSubImage2DBucket::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLint>(13), cmd.xoffset);
  EXPECT_EQ(static_cast<GLint>(14), cmd.yoffset);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.height);
  EXPECT_EQ(static_cast<GLenum>(17), cmd.format);
  EXPECT_EQ(static_cast<GLuint>(18), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CopyTexImage2D) {
  CopyTexImage2D& cmd = *GetBufferAs<CopyTexImage2D>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12),
      static_cast<GLenum>(13),
      static_cast<GLint>(14),
      static_cast<GLint>(15),
      static_cast<GLsizei>(16),
      static_cast<GLsizei>(17),
      static_cast<GLint>(18));
  EXPECT_EQ(static_cast<uint32>(CopyTexImage2D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.internalformat);
  EXPECT_EQ(static_cast<GLint>(14), cmd.x);
  EXPECT_EQ(static_cast<GLint>(15), cmd.y);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(17), cmd.height);
  EXPECT_EQ(static_cast<GLint>(18), cmd.border);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CopyTexSubImage2D) {
  CopyTexSubImage2D& cmd = *GetBufferAs<CopyTexSubImage2D>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12),
      static_cast<GLint>(13),
      static_cast<GLint>(14),
      static_cast<GLint>(15),
      static_cast<GLint>(16),
      static_cast<GLsizei>(17),
      static_cast<GLsizei>(18));
  EXPECT_EQ(static_cast<uint32>(CopyTexSubImage2D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLint>(13), cmd.xoffset);
  EXPECT_EQ(static_cast<GLint>(14), cmd.yoffset);
  EXPECT_EQ(static_cast<GLint>(15), cmd.x);
  EXPECT_EQ(static_cast<GLint>(16), cmd.y);
  EXPECT_EQ(static_cast<GLsizei>(17), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(18), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CreateProgram) {
  CreateProgram& cmd = *GetBufferAs<CreateProgram>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<uint32>(11));
  EXPECT_EQ(static_cast<uint32>(CreateProgram::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<uint32>(11), cmd.client_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CreateShader) {
  CreateShader& cmd = *GetBufferAs<CreateShader>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<uint32>(12));
  EXPECT_EQ(static_cast<uint32>(CreateShader::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.type);
  EXPECT_EQ(static_cast<uint32>(12), cmd.client_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CullFace) {
  CullFace& cmd = *GetBufferAs<CullFace>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32>(CullFace::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DeleteBuffers) {
  DeleteBuffers& cmd = *GetBufferAs<DeleteBuffers>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(DeleteBuffers::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.buffers_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.buffers_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DeleteBuffersImmediate) {
  static GLuint ids[] = { 12, 23, 34, };
  DeleteBuffersImmediate& cmd = *GetBufferAs<DeleteBuffersImmediate>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLsizei>(arraysize(ids)), ids);
  EXPECT_EQ(static_cast<uint32>(DeleteBuffersImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(arraysize(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(arraysize(ids) * 4u));
  // TODO(gman): Check that ids were inserted;
}

TEST_F(GLES2FormatTest, DeleteFramebuffers) {
  DeleteFramebuffers& cmd = *GetBufferAs<DeleteFramebuffers>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(DeleteFramebuffers::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.framebuffers_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.framebuffers_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DeleteFramebuffersImmediate) {
  static GLuint ids[] = { 12, 23, 34, };
  DeleteFramebuffersImmediate& cmd =
      *GetBufferAs<DeleteFramebuffersImmediate>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLsizei>(arraysize(ids)), ids);
  EXPECT_EQ(static_cast<uint32>(DeleteFramebuffersImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(arraysize(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(arraysize(ids) * 4u));
  // TODO(gman): Check that ids were inserted;
}

TEST_F(GLES2FormatTest, DeleteProgram) {
  DeleteProgram& cmd = *GetBufferAs<DeleteProgram>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(DeleteProgram::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DeleteRenderbuffers) {
  DeleteRenderbuffers& cmd = *GetBufferAs<DeleteRenderbuffers>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(DeleteRenderbuffers::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.renderbuffers_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.renderbuffers_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DeleteRenderbuffersImmediate) {
  static GLuint ids[] = { 12, 23, 34, };
  DeleteRenderbuffersImmediate& cmd =
      *GetBufferAs<DeleteRenderbuffersImmediate>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLsizei>(arraysize(ids)), ids);
  EXPECT_EQ(static_cast<uint32>(DeleteRenderbuffersImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(arraysize(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(arraysize(ids) * 4u));
  // TODO(gman): Check that ids were inserted;
}

TEST_F(GLES2FormatTest, DeleteShader) {
  DeleteShader& cmd = *GetBufferAs<DeleteShader>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(DeleteShader::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DeleteTextures) {
  DeleteTextures& cmd = *GetBufferAs<DeleteTextures>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(DeleteTextures::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.textures_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.textures_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DeleteTexturesImmediate) {
  static GLuint ids[] = { 12, 23, 34, };
  DeleteTexturesImmediate& cmd = *GetBufferAs<DeleteTexturesImmediate>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLsizei>(arraysize(ids)), ids);
  EXPECT_EQ(static_cast<uint32>(DeleteTexturesImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(arraysize(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(arraysize(ids) * 4u));
  // TODO(gman): Check that ids were inserted;
}

TEST_F(GLES2FormatTest, DepthFunc) {
  DepthFunc& cmd = *GetBufferAs<DepthFunc>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32>(DepthFunc::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.func);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DepthMask) {
  DepthMask& cmd = *GetBufferAs<DepthMask>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLboolean>(11));
  EXPECT_EQ(static_cast<uint32>(DepthMask::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLboolean>(11), cmd.flag);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DepthRangef) {
  DepthRangef& cmd = *GetBufferAs<DepthRangef>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLclampf>(11),
      static_cast<GLclampf>(12));
  EXPECT_EQ(static_cast<uint32>(DepthRangef::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLclampf>(11), cmd.zNear);
  EXPECT_EQ(static_cast<GLclampf>(12), cmd.zFar);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DetachShader) {
  DetachShader& cmd = *GetBufferAs<DetachShader>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32>(DetachShader::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.shader);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Disable) {
  Disable& cmd = *GetBufferAs<Disable>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32>(Disable::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.cap);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DisableVertexAttribArray) {
  DisableVertexAttribArray& cmd = *GetBufferAs<DisableVertexAttribArray>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(DisableVertexAttribArray::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.index);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DrawArrays) {
  DrawArrays& cmd = *GetBufferAs<DrawArrays>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12),
      static_cast<GLsizei>(13));
  EXPECT_EQ(static_cast<uint32>(DrawArrays::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  EXPECT_EQ(static_cast<GLint>(12), cmd.first);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DrawElements) {
  DrawElements& cmd = *GetBufferAs<DrawElements>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLsizei>(12),
      static_cast<GLenum>(13),
      static_cast<GLuint>(14));
  EXPECT_EQ(static_cast<uint32>(DrawElements::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.type);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.index_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Enable) {
  Enable& cmd = *GetBufferAs<Enable>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32>(Enable::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.cap);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, EnableVertexAttribArray) {
  EnableVertexAttribArray& cmd = *GetBufferAs<EnableVertexAttribArray>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(EnableVertexAttribArray::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.index);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Finish) {
  Finish& cmd = *GetBufferAs<Finish>();
  void* next_cmd = cmd.Set(
      &cmd);
  EXPECT_EQ(static_cast<uint32>(Finish::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Flush) {
  Flush& cmd = *GetBufferAs<Flush>();
  void* next_cmd = cmd.Set(
      &cmd);
  EXPECT_EQ(static_cast<uint32>(Flush::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, FramebufferRenderbuffer) {
  FramebufferRenderbuffer& cmd = *GetBufferAs<FramebufferRenderbuffer>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLenum>(13),
      static_cast<GLuint>(14));
  EXPECT_EQ(static_cast<uint32>(FramebufferRenderbuffer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.attachment);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.renderbuffertarget);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.renderbuffer);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, FramebufferTexture2D) {
  FramebufferTexture2D& cmd = *GetBufferAs<FramebufferTexture2D>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLenum>(13),
      static_cast<GLuint>(14),
      static_cast<GLint>(15));
  EXPECT_EQ(static_cast<uint32>(FramebufferTexture2D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.attachment);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.textarget);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.texture);
  EXPECT_EQ(static_cast<GLint>(15), cmd.level);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, FrontFace) {
  FrontFace& cmd = *GetBufferAs<FrontFace>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32>(FrontFace::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GenBuffers) {
  GenBuffers& cmd = *GetBufferAs<GenBuffers>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(GenBuffers::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.buffers_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.buffers_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GenBuffersImmediate) {
  static GLuint ids[] = { 12, 23, 34, };
  GenBuffersImmediate& cmd = *GetBufferAs<GenBuffersImmediate>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLsizei>(arraysize(ids)), ids);
  EXPECT_EQ(static_cast<uint32>(GenBuffersImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(arraysize(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(arraysize(ids) * 4u));
  // TODO(gman): Check that ids were inserted;
}

TEST_F(GLES2FormatTest, GenerateMipmap) {
  GenerateMipmap& cmd = *GetBufferAs<GenerateMipmap>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32>(GenerateMipmap::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GenFramebuffers) {
  GenFramebuffers& cmd = *GetBufferAs<GenFramebuffers>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(GenFramebuffers::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.framebuffers_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.framebuffers_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GenFramebuffersImmediate) {
  static GLuint ids[] = { 12, 23, 34, };
  GenFramebuffersImmediate& cmd = *GetBufferAs<GenFramebuffersImmediate>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLsizei>(arraysize(ids)), ids);
  EXPECT_EQ(static_cast<uint32>(GenFramebuffersImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(arraysize(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(arraysize(ids) * 4u));
  // TODO(gman): Check that ids were inserted;
}

TEST_F(GLES2FormatTest, GenRenderbuffers) {
  GenRenderbuffers& cmd = *GetBufferAs<GenRenderbuffers>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(GenRenderbuffers::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.renderbuffers_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.renderbuffers_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GenRenderbuffersImmediate) {
  static GLuint ids[] = { 12, 23, 34, };
  GenRenderbuffersImmediate& cmd = *GetBufferAs<GenRenderbuffersImmediate>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLsizei>(arraysize(ids)), ids);
  EXPECT_EQ(static_cast<uint32>(GenRenderbuffersImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(arraysize(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(arraysize(ids) * 4u));
  // TODO(gman): Check that ids were inserted;
}

TEST_F(GLES2FormatTest, GenTextures) {
  GenTextures& cmd = *GetBufferAs<GenTextures>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(GenTextures::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.textures_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.textures_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GenTexturesImmediate) {
  static GLuint ids[] = { 12, 23, 34, };
  GenTexturesImmediate& cmd = *GetBufferAs<GenTexturesImmediate>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLsizei>(arraysize(ids)), ids);
  EXPECT_EQ(static_cast<uint32>(GenTexturesImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(arraysize(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(arraysize(ids) * 4u));
  // TODO(gman): Check that ids were inserted;
}

TEST_F(GLES2FormatTest, GetActiveAttrib) {
  GetActiveAttrib& cmd = *GetBufferAs<GetActiveAttrib>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLuint>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14),
      static_cast<uint32>(15));
  EXPECT_EQ(static_cast<uint32>(GetActiveAttrib::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.index);
  EXPECT_EQ(static_cast<uint32>(13), cmd.name_bucket_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(15), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetActiveUniform) {
  GetActiveUniform& cmd = *GetBufferAs<GetActiveUniform>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLuint>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14),
      static_cast<uint32>(15));
  EXPECT_EQ(static_cast<uint32>(GetActiveUniform::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.index);
  EXPECT_EQ(static_cast<uint32>(13), cmd.name_bucket_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(15), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetAttachedShaders) {
  GetAttachedShaders& cmd = *GetBufferAs<GetAttachedShaders>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(GetAttachedShaders::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
  EXPECT_EQ(static_cast<uint32>(14), cmd.result_size);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

// TODO(gman): Write test for GetAttribLocation
// TODO(gman): Write test for GetAttribLocationImmediate
// TODO(gman): Write test for GetAttribLocationBucket
TEST_F(GLES2FormatTest, GetBooleanv) {
  GetBooleanv& cmd = *GetBufferAs<GetBooleanv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(GetBooleanv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(12), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetBufferParameteriv) {
  GetBufferParameteriv& cmd = *GetBufferAs<GetBufferParameteriv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(GetBufferParameteriv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetError) {
  GetError& cmd = *GetBufferAs<GetError>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<uint32>(11),
      static_cast<uint32>(12));
  EXPECT_EQ(static_cast<uint32>(GetError::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<uint32>(11), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetFloatv) {
  GetFloatv& cmd = *GetBufferAs<GetFloatv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(GetFloatv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(12), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetFramebufferAttachmentParameteriv) {
  GetFramebufferAttachmentParameteriv& cmd =
      *GetBufferAs<GetFramebufferAttachmentParameteriv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLenum>(13),
      static_cast<uint32>(14),
      static_cast<uint32>(15));
  EXPECT_EQ(static_cast<uint32>(GetFramebufferAttachmentParameteriv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.attachment);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(15), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetIntegerv) {
  GetIntegerv& cmd = *GetBufferAs<GetIntegerv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(GetIntegerv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(12), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetProgramiv) {
  GetProgramiv& cmd = *GetBufferAs<GetProgramiv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(GetProgramiv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetProgramInfoLog) {
  GetProgramInfoLog& cmd = *GetBufferAs<GetProgramInfoLog>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12));
  EXPECT_EQ(static_cast<uint32>(GetProgramInfoLog::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<uint32>(12), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetRenderbufferParameteriv) {
  GetRenderbufferParameteriv& cmd = *GetBufferAs<GetRenderbufferParameteriv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(GetRenderbufferParameteriv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetShaderiv) {
  GetShaderiv& cmd = *GetBufferAs<GetShaderiv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(GetShaderiv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetShaderInfoLog) {
  GetShaderInfoLog& cmd = *GetBufferAs<GetShaderInfoLog>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12));
  EXPECT_EQ(static_cast<uint32>(GetShaderInfoLog::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  EXPECT_EQ(static_cast<uint32>(12), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetShaderPrecisionFormat) {
  GetShaderPrecisionFormat& cmd = *GetBufferAs<GetShaderPrecisionFormat>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(GetShaderPrecisionFormat::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.shadertype);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.precisiontype);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetShaderSource) {
  GetShaderSource& cmd = *GetBufferAs<GetShaderSource>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12));
  EXPECT_EQ(static_cast<uint32>(GetShaderSource::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  EXPECT_EQ(static_cast<uint32>(12), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetString) {
  GetString& cmd = *GetBufferAs<GetString>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<uint32>(12));
  EXPECT_EQ(static_cast<uint32>(GetString::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.name);
  EXPECT_EQ(static_cast<uint32>(12), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetTexParameterfv) {
  GetTexParameterfv& cmd = *GetBufferAs<GetTexParameterfv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(GetTexParameterfv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetTexParameteriv) {
  GetTexParameteriv& cmd = *GetBufferAs<GetTexParameteriv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(GetTexParameteriv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetUniformfv) {
  GetUniformfv& cmd = *GetBufferAs<GetUniformfv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLint>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(GetUniformfv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLint>(12), cmd.location);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetUniformiv) {
  GetUniformiv& cmd = *GetBufferAs<GetUniformiv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLint>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(GetUniformiv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLint>(12), cmd.location);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

// TODO(gman): Write test for GetUniformLocation
// TODO(gman): Write test for GetUniformLocationImmediate
// TODO(gman): Write test for GetUniformLocationBucket
TEST_F(GLES2FormatTest, GetVertexAttribfv) {
  GetVertexAttribfv& cmd = *GetBufferAs<GetVertexAttribfv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(GetVertexAttribfv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.index);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetVertexAttribiv) {
  GetVertexAttribiv& cmd = *GetBufferAs<GetVertexAttribiv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(GetVertexAttribiv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.index);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetVertexAttribPointerv) {
  GetVertexAttribPointerv& cmd = *GetBufferAs<GetVertexAttribPointerv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(GetVertexAttribPointerv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.index);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.pointer_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.pointer_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Hint) {
  Hint& cmd = *GetBufferAs<Hint>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12));
  EXPECT_EQ(static_cast<uint32>(Hint::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.mode);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, IsBuffer) {
  IsBuffer& cmd = *GetBufferAs<IsBuffer>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(IsBuffer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.buffer);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, IsEnabled) {
  IsEnabled& cmd = *GetBufferAs<IsEnabled>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(IsEnabled::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.cap);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, IsFramebuffer) {
  IsFramebuffer& cmd = *GetBufferAs<IsFramebuffer>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(IsFramebuffer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.framebuffer);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, IsProgram) {
  IsProgram& cmd = *GetBufferAs<IsProgram>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(IsProgram::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, IsRenderbuffer) {
  IsRenderbuffer& cmd = *GetBufferAs<IsRenderbuffer>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(IsRenderbuffer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.renderbuffer);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, IsShader) {
  IsShader& cmd = *GetBufferAs<IsShader>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(IsShader::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, IsTexture) {
  IsTexture& cmd = *GetBufferAs<IsTexture>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(IsTexture::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.texture);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, LineWidth) {
  LineWidth& cmd = *GetBufferAs<LineWidth>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLfloat>(11));
  EXPECT_EQ(static_cast<uint32>(LineWidth::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLfloat>(11), cmd.width);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, LinkProgram) {
  LinkProgram& cmd = *GetBufferAs<LinkProgram>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(LinkProgram::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, PixelStorei) {
  PixelStorei& cmd = *GetBufferAs<PixelStorei>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12));
  EXPECT_EQ(static_cast<uint32>(PixelStorei::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.pname);
  EXPECT_EQ(static_cast<GLint>(12), cmd.param);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, PolygonOffset) {
  PolygonOffset& cmd = *GetBufferAs<PolygonOffset>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLfloat>(11),
      static_cast<GLfloat>(12));
  EXPECT_EQ(static_cast<uint32>(PolygonOffset::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLfloat>(11), cmd.factor);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.units);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ReadPixels) {
  ReadPixels& cmd = *GetBufferAs<ReadPixels>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLint>(12),
      static_cast<GLsizei>(13),
      static_cast<GLsizei>(14),
      static_cast<GLenum>(15),
      static_cast<GLenum>(16),
      static_cast<uint32>(17),
      static_cast<uint32>(18),
      static_cast<uint32>(19),
      static_cast<uint32>(20));
  EXPECT_EQ(static_cast<uint32>(ReadPixels::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.x);
  EXPECT_EQ(static_cast<GLint>(12), cmd.y);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.height);
  EXPECT_EQ(static_cast<GLenum>(15), cmd.format);
  EXPECT_EQ(static_cast<GLenum>(16), cmd.type);
  EXPECT_EQ(static_cast<uint32>(17), cmd.pixels_shm_id);
  EXPECT_EQ(static_cast<uint32>(18), cmd.pixels_shm_offset);
  EXPECT_EQ(static_cast<uint32>(19), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(20), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ReleaseShaderCompiler) {
  ReleaseShaderCompiler& cmd = *GetBufferAs<ReleaseShaderCompiler>();
  void* next_cmd = cmd.Set(
      &cmd);
  EXPECT_EQ(static_cast<uint32>(ReleaseShaderCompiler::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, RenderbufferStorage) {
  RenderbufferStorage& cmd = *GetBufferAs<RenderbufferStorage>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLsizei>(13),
      static_cast<GLsizei>(14));
  EXPECT_EQ(static_cast<uint32>(RenderbufferStorage::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.internalformat);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, SampleCoverage) {
  SampleCoverage& cmd = *GetBufferAs<SampleCoverage>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLclampf>(11),
      static_cast<GLboolean>(12));
  EXPECT_EQ(static_cast<uint32>(SampleCoverage::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLclampf>(11), cmd.value);
  EXPECT_EQ(static_cast<GLboolean>(12), cmd.invert);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Scissor) {
  Scissor& cmd = *GetBufferAs<Scissor>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLint>(12),
      static_cast<GLsizei>(13),
      static_cast<GLsizei>(14));
  EXPECT_EQ(static_cast<uint32>(Scissor::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.x);
  EXPECT_EQ(static_cast<GLint>(12), cmd.y);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ShaderBinary) {
  ShaderBinary& cmd = *GetBufferAs<ShaderBinary>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13),
      static_cast<GLenum>(14),
      static_cast<uint32>(15),
      static_cast<uint32>(16),
      static_cast<GLsizei>(17));
  EXPECT_EQ(static_cast<uint32>(ShaderBinary::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.shaders_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.shaders_shm_offset);
  EXPECT_EQ(static_cast<GLenum>(14), cmd.binaryformat);
  EXPECT_EQ(static_cast<uint32>(15), cmd.binary_shm_id);
  EXPECT_EQ(static_cast<uint32>(16), cmd.binary_shm_offset);
  EXPECT_EQ(static_cast<GLsizei>(17), cmd.length);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ShaderSource) {
  ShaderSource& cmd = *GetBufferAs<ShaderSource>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(ShaderSource::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  EXPECT_EQ(static_cast<uint32>(12), cmd.data_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.data_shm_offset);
  EXPECT_EQ(static_cast<uint32>(14), cmd.data_size);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

// TODO(gman): Implement test for ShaderSourceImmediate
TEST_F(GLES2FormatTest, ShaderSourceBucket) {
  ShaderSourceBucket& cmd = *GetBufferAs<ShaderSourceBucket>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12));
  EXPECT_EQ(static_cast<uint32>(ShaderSourceBucket::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  EXPECT_EQ(static_cast<uint32>(12), cmd.data_bucket_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, StencilFunc) {
  StencilFunc& cmd = *GetBufferAs<StencilFunc>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12),
      static_cast<GLuint>(13));
  EXPECT_EQ(static_cast<uint32>(StencilFunc::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.func);
  EXPECT_EQ(static_cast<GLint>(12), cmd.ref);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.mask);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, StencilFuncSeparate) {
  StencilFuncSeparate& cmd = *GetBufferAs<StencilFuncSeparate>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLint>(13),
      static_cast<GLuint>(14));
  EXPECT_EQ(static_cast<uint32>(StencilFuncSeparate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.face);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.func);
  EXPECT_EQ(static_cast<GLint>(13), cmd.ref);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.mask);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, StencilMask) {
  StencilMask& cmd = *GetBufferAs<StencilMask>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(StencilMask::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.mask);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, StencilMaskSeparate) {
  StencilMaskSeparate& cmd = *GetBufferAs<StencilMaskSeparate>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32>(StencilMaskSeparate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.face);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.mask);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, StencilOp) {
  StencilOp& cmd = *GetBufferAs<StencilOp>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLenum>(13));
  EXPECT_EQ(static_cast<uint32>(StencilOp::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.fail);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.zfail);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.zpass);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, StencilOpSeparate) {
  StencilOpSeparate& cmd = *GetBufferAs<StencilOpSeparate>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLenum>(13),
      static_cast<GLenum>(14));
  EXPECT_EQ(static_cast<uint32>(StencilOpSeparate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.face);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.fail);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.zfail);
  EXPECT_EQ(static_cast<GLenum>(14), cmd.zpass);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, TexImage2D) {
  TexImage2D& cmd = *GetBufferAs<TexImage2D>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12),
      static_cast<GLint>(13),
      static_cast<GLsizei>(14),
      static_cast<GLsizei>(15),
      static_cast<GLint>(16),
      static_cast<GLenum>(17),
      static_cast<GLenum>(18),
      static_cast<uint32>(19),
      static_cast<uint32>(20));
  EXPECT_EQ(static_cast<uint32>(TexImage2D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLint>(13), cmd.internalformat);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.height);
  EXPECT_EQ(static_cast<GLint>(16), cmd.border);
  EXPECT_EQ(static_cast<GLenum>(17), cmd.format);
  EXPECT_EQ(static_cast<GLenum>(18), cmd.type);
  EXPECT_EQ(static_cast<uint32>(19), cmd.pixels_shm_id);
  EXPECT_EQ(static_cast<uint32>(20), cmd.pixels_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

// TODO(gman): Implement test for TexImage2DImmediate
TEST_F(GLES2FormatTest, TexParameterf) {
  TexParameterf& cmd = *GetBufferAs<TexParameterf>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLfloat>(13));
  EXPECT_EQ(static_cast<uint32>(TexParameterf::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.param);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, TexParameterfv) {
  TexParameterfv& cmd = *GetBufferAs<TexParameterfv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(TexParameterfv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, TexParameterfvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
  };
  TexParameterfvImmediate& cmd = *GetBufferAs<TexParameterfvImmediate>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      data);
  EXPECT_EQ(static_cast<uint32>(TexParameterfvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, TexParameteri) {
  TexParameteri& cmd = *GetBufferAs<TexParameteri>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLint>(13));
  EXPECT_EQ(static_cast<uint32>(TexParameteri::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<GLint>(13), cmd.param);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, TexParameteriv) {
  TexParameteriv& cmd = *GetBufferAs<TexParameteriv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(TexParameteriv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, TexParameterivImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLint data[] = {
    static_cast<GLint>(kSomeBaseValueToTestWith + 0),
  };
  TexParameterivImmediate& cmd = *GetBufferAs<TexParameterivImmediate>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      data);
  EXPECT_EQ(static_cast<uint32>(TexParameterivImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, TexSubImage2D) {
  TexSubImage2D& cmd = *GetBufferAs<TexSubImage2D>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12),
      static_cast<GLint>(13),
      static_cast<GLint>(14),
      static_cast<GLsizei>(15),
      static_cast<GLsizei>(16),
      static_cast<GLenum>(17),
      static_cast<GLenum>(18),
      static_cast<uint32>(19),
      static_cast<uint32>(20),
      static_cast<GLboolean>(21));
  EXPECT_EQ(static_cast<uint32>(TexSubImage2D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLint>(13), cmd.xoffset);
  EXPECT_EQ(static_cast<GLint>(14), cmd.yoffset);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.height);
  EXPECT_EQ(static_cast<GLenum>(17), cmd.format);
  EXPECT_EQ(static_cast<GLenum>(18), cmd.type);
  EXPECT_EQ(static_cast<uint32>(19), cmd.pixels_shm_id);
  EXPECT_EQ(static_cast<uint32>(20), cmd.pixels_shm_offset);
  EXPECT_EQ(static_cast<GLboolean>(21), cmd.internal);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

// TODO(gman): Implement test for TexSubImage2DImmediate
TEST_F(GLES2FormatTest, Uniform1f) {
  Uniform1f& cmd = *GetBufferAs<Uniform1f>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLfloat>(12));
  EXPECT_EQ(static_cast<uint32>(Uniform1f::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform1fv) {
  Uniform1fv& cmd = *GetBufferAs<Uniform1fv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(Uniform1fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<uint32>(13), cmd.v_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.v_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform1fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
  };
  Uniform1fvImmediate& cmd = *GetBufferAs<Uniform1fvImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLfloat) * 1;
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      data);
  EXPECT_EQ(static_cast<uint32>(Uniform1fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, Uniform1i) {
  Uniform1i& cmd = *GetBufferAs<Uniform1i>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLint>(12));
  EXPECT_EQ(static_cast<uint32>(Uniform1i::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLint>(12), cmd.x);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform1iv) {
  Uniform1iv& cmd = *GetBufferAs<Uniform1iv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(Uniform1iv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<uint32>(13), cmd.v_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.v_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform1ivImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLint data[] = {
    static_cast<GLint>(kSomeBaseValueToTestWith + 0),
    static_cast<GLint>(kSomeBaseValueToTestWith + 1),
  };
  Uniform1ivImmediate& cmd = *GetBufferAs<Uniform1ivImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLint) * 1;
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      data);
  EXPECT_EQ(static_cast<uint32>(Uniform1ivImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, Uniform2f) {
  Uniform2f& cmd = *GetBufferAs<Uniform2f>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLfloat>(12),
      static_cast<GLfloat>(13));
  EXPECT_EQ(static_cast<uint32>(Uniform2f::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.y);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform2fv) {
  Uniform2fv& cmd = *GetBufferAs<Uniform2fv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(Uniform2fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<uint32>(13), cmd.v_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.v_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform2fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
  };
  Uniform2fvImmediate& cmd = *GetBufferAs<Uniform2fvImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLfloat) * 2;
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      data);
  EXPECT_EQ(static_cast<uint32>(Uniform2fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, Uniform2i) {
  Uniform2i& cmd = *GetBufferAs<Uniform2i>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLint>(12),
      static_cast<GLint>(13));
  EXPECT_EQ(static_cast<uint32>(Uniform2i::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLint>(12), cmd.x);
  EXPECT_EQ(static_cast<GLint>(13), cmd.y);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform2iv) {
  Uniform2iv& cmd = *GetBufferAs<Uniform2iv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(Uniform2iv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<uint32>(13), cmd.v_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.v_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform2ivImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLint data[] = {
    static_cast<GLint>(kSomeBaseValueToTestWith + 0),
    static_cast<GLint>(kSomeBaseValueToTestWith + 1),
    static_cast<GLint>(kSomeBaseValueToTestWith + 2),
    static_cast<GLint>(kSomeBaseValueToTestWith + 3),
  };
  Uniform2ivImmediate& cmd = *GetBufferAs<Uniform2ivImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLint) * 2;
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      data);
  EXPECT_EQ(static_cast<uint32>(Uniform2ivImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, Uniform3f) {
  Uniform3f& cmd = *GetBufferAs<Uniform3f>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLfloat>(12),
      static_cast<GLfloat>(13),
      static_cast<GLfloat>(14));
  EXPECT_EQ(static_cast<uint32>(Uniform3f::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.y);
  EXPECT_EQ(static_cast<GLfloat>(14), cmd.z);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform3fv) {
  Uniform3fv& cmd = *GetBufferAs<Uniform3fv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(Uniform3fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<uint32>(13), cmd.v_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.v_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform3fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 4),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 5),
  };
  Uniform3fvImmediate& cmd = *GetBufferAs<Uniform3fvImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLfloat) * 3;
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      data);
  EXPECT_EQ(static_cast<uint32>(Uniform3fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, Uniform3i) {
  Uniform3i& cmd = *GetBufferAs<Uniform3i>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLint>(12),
      static_cast<GLint>(13),
      static_cast<GLint>(14));
  EXPECT_EQ(static_cast<uint32>(Uniform3i::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLint>(12), cmd.x);
  EXPECT_EQ(static_cast<GLint>(13), cmd.y);
  EXPECT_EQ(static_cast<GLint>(14), cmd.z);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform3iv) {
  Uniform3iv& cmd = *GetBufferAs<Uniform3iv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(Uniform3iv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<uint32>(13), cmd.v_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.v_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform3ivImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLint data[] = {
    static_cast<GLint>(kSomeBaseValueToTestWith + 0),
    static_cast<GLint>(kSomeBaseValueToTestWith + 1),
    static_cast<GLint>(kSomeBaseValueToTestWith + 2),
    static_cast<GLint>(kSomeBaseValueToTestWith + 3),
    static_cast<GLint>(kSomeBaseValueToTestWith + 4),
    static_cast<GLint>(kSomeBaseValueToTestWith + 5),
  };
  Uniform3ivImmediate& cmd = *GetBufferAs<Uniform3ivImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLint) * 3;
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      data);
  EXPECT_EQ(static_cast<uint32>(Uniform3ivImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, Uniform4f) {
  Uniform4f& cmd = *GetBufferAs<Uniform4f>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLfloat>(12),
      static_cast<GLfloat>(13),
      static_cast<GLfloat>(14),
      static_cast<GLfloat>(15));
  EXPECT_EQ(static_cast<uint32>(Uniform4f::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.y);
  EXPECT_EQ(static_cast<GLfloat>(14), cmd.z);
  EXPECT_EQ(static_cast<GLfloat>(15), cmd.w);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform4fv) {
  Uniform4fv& cmd = *GetBufferAs<Uniform4fv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(Uniform4fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<uint32>(13), cmd.v_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.v_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform4fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 4),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 5),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 6),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 7),
  };
  Uniform4fvImmediate& cmd = *GetBufferAs<Uniform4fvImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLfloat) * 4;
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      data);
  EXPECT_EQ(static_cast<uint32>(Uniform4fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, Uniform4i) {
  Uniform4i& cmd = *GetBufferAs<Uniform4i>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLint>(12),
      static_cast<GLint>(13),
      static_cast<GLint>(14),
      static_cast<GLint>(15));
  EXPECT_EQ(static_cast<uint32>(Uniform4i::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLint>(12), cmd.x);
  EXPECT_EQ(static_cast<GLint>(13), cmd.y);
  EXPECT_EQ(static_cast<GLint>(14), cmd.z);
  EXPECT_EQ(static_cast<GLint>(15), cmd.w);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform4iv) {
  Uniform4iv& cmd = *GetBufferAs<Uniform4iv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(Uniform4iv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<uint32>(13), cmd.v_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.v_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Uniform4ivImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLint data[] = {
    static_cast<GLint>(kSomeBaseValueToTestWith + 0),
    static_cast<GLint>(kSomeBaseValueToTestWith + 1),
    static_cast<GLint>(kSomeBaseValueToTestWith + 2),
    static_cast<GLint>(kSomeBaseValueToTestWith + 3),
    static_cast<GLint>(kSomeBaseValueToTestWith + 4),
    static_cast<GLint>(kSomeBaseValueToTestWith + 5),
    static_cast<GLint>(kSomeBaseValueToTestWith + 6),
    static_cast<GLint>(kSomeBaseValueToTestWith + 7),
  };
  Uniform4ivImmediate& cmd = *GetBufferAs<Uniform4ivImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLint) * 4;
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      data);
  EXPECT_EQ(static_cast<uint32>(Uniform4ivImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, UniformMatrix2fv) {
  UniformMatrix2fv& cmd = *GetBufferAs<UniformMatrix2fv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<GLboolean>(13),
      static_cast<uint32>(14),
      static_cast<uint32>(15));
  EXPECT_EQ(static_cast<uint32>(UniformMatrix2fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<GLboolean>(13), cmd.transpose);
  EXPECT_EQ(static_cast<uint32>(14), cmd.value_shm_id);
  EXPECT_EQ(static_cast<uint32>(15), cmd.value_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, UniformMatrix2fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 4),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 5),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 6),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 7),
  };
  UniformMatrix2fvImmediate& cmd = *GetBufferAs<UniformMatrix2fvImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLfloat) * 4;
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      static_cast<GLboolean>(3),
      data);
  EXPECT_EQ(static_cast<uint32>(UniformMatrix2fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  EXPECT_EQ(static_cast<GLboolean>(3), cmd.transpose);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, UniformMatrix3fv) {
  UniformMatrix3fv& cmd = *GetBufferAs<UniformMatrix3fv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<GLboolean>(13),
      static_cast<uint32>(14),
      static_cast<uint32>(15));
  EXPECT_EQ(static_cast<uint32>(UniformMatrix3fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<GLboolean>(13), cmd.transpose);
  EXPECT_EQ(static_cast<uint32>(14), cmd.value_shm_id);
  EXPECT_EQ(static_cast<uint32>(15), cmd.value_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, UniformMatrix3fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 4),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 5),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 6),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 7),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 8),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 9),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 10),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 11),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 12),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 13),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 14),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 15),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 16),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 17),
  };
  UniformMatrix3fvImmediate& cmd = *GetBufferAs<UniformMatrix3fvImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLfloat) * 9;
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      static_cast<GLboolean>(3),
      data);
  EXPECT_EQ(static_cast<uint32>(UniformMatrix3fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  EXPECT_EQ(static_cast<GLboolean>(3), cmd.transpose);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, UniformMatrix4fv) {
  UniformMatrix4fv& cmd = *GetBufferAs<UniformMatrix4fv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<GLboolean>(13),
      static_cast<uint32>(14),
      static_cast<uint32>(15));
  EXPECT_EQ(static_cast<uint32>(UniformMatrix4fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<GLboolean>(13), cmd.transpose);
  EXPECT_EQ(static_cast<uint32>(14), cmd.value_shm_id);
  EXPECT_EQ(static_cast<uint32>(15), cmd.value_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, UniformMatrix4fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 4),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 5),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 6),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 7),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 8),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 9),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 10),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 11),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 12),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 13),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 14),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 15),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 16),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 17),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 18),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 19),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 20),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 21),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 22),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 23),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 24),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 25),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 26),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 27),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 28),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 29),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 30),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 31),
  };
  UniformMatrix4fvImmediate& cmd = *GetBufferAs<UniformMatrix4fvImmediate>();
  const GLsizei kNumElements = 2;
  const size_t kExpectedCmdSize =
      sizeof(cmd) + kNumElements * sizeof(GLfloat) * 16;
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      static_cast<GLboolean>(3),
      data);
  EXPECT_EQ(static_cast<uint32>(UniformMatrix4fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(kExpectedCmdSize, cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  EXPECT_EQ(static_cast<GLboolean>(3), cmd.transpose);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, UseProgram) {
  UseProgram& cmd = *GetBufferAs<UseProgram>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(UseProgram::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ValidateProgram) {
  ValidateProgram& cmd = *GetBufferAs<ValidateProgram>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(ValidateProgram::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, VertexAttrib1f) {
  VertexAttrib1f& cmd = *GetBufferAs<VertexAttrib1f>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLfloat>(12));
  EXPECT_EQ(static_cast<uint32>(VertexAttrib1f::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, VertexAttrib1fv) {
  VertexAttrib1fv& cmd = *GetBufferAs<VertexAttrib1fv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(VertexAttrib1fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<uint32>(12), cmd.values_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.values_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, VertexAttrib1fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
  };
  VertexAttrib1fvImmediate& cmd = *GetBufferAs<VertexAttrib1fvImmediate>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      data);
  EXPECT_EQ(static_cast<uint32>(VertexAttrib1fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, VertexAttrib2f) {
  VertexAttrib2f& cmd = *GetBufferAs<VertexAttrib2f>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLfloat>(12),
      static_cast<GLfloat>(13));
  EXPECT_EQ(static_cast<uint32>(VertexAttrib2f::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.y);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, VertexAttrib2fv) {
  VertexAttrib2fv& cmd = *GetBufferAs<VertexAttrib2fv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(VertexAttrib2fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<uint32>(12), cmd.values_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.values_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, VertexAttrib2fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
  };
  VertexAttrib2fvImmediate& cmd = *GetBufferAs<VertexAttrib2fvImmediate>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      data);
  EXPECT_EQ(static_cast<uint32>(VertexAttrib2fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, VertexAttrib3f) {
  VertexAttrib3f& cmd = *GetBufferAs<VertexAttrib3f>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLfloat>(12),
      static_cast<GLfloat>(13),
      static_cast<GLfloat>(14));
  EXPECT_EQ(static_cast<uint32>(VertexAttrib3f::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.y);
  EXPECT_EQ(static_cast<GLfloat>(14), cmd.z);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, VertexAttrib3fv) {
  VertexAttrib3fv& cmd = *GetBufferAs<VertexAttrib3fv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(VertexAttrib3fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<uint32>(12), cmd.values_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.values_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, VertexAttrib3fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
  };
  VertexAttrib3fvImmediate& cmd = *GetBufferAs<VertexAttrib3fvImmediate>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      data);
  EXPECT_EQ(static_cast<uint32>(VertexAttrib3fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, VertexAttrib4f) {
  VertexAttrib4f& cmd = *GetBufferAs<VertexAttrib4f>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLfloat>(12),
      static_cast<GLfloat>(13),
      static_cast<GLfloat>(14),
      static_cast<GLfloat>(15));
  EXPECT_EQ(static_cast<uint32>(VertexAttrib4f::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.y);
  EXPECT_EQ(static_cast<GLfloat>(14), cmd.z);
  EXPECT_EQ(static_cast<GLfloat>(15), cmd.w);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, VertexAttrib4fv) {
  VertexAttrib4fv& cmd = *GetBufferAs<VertexAttrib4fv>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(VertexAttrib4fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<uint32>(12), cmd.values_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.values_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, VertexAttrib4fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
  };
  VertexAttrib4fvImmediate& cmd = *GetBufferAs<VertexAttrib4fvImmediate>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      data);
  EXPECT_EQ(static_cast<uint32>(VertexAttrib4fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) +
      RoundSizeToMultipleOfEntries(sizeof(data)));
  // TODO(gman): Check that data was inserted;
}

TEST_F(GLES2FormatTest, VertexAttribPointer) {
  VertexAttribPointer& cmd = *GetBufferAs<VertexAttribPointer>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLint>(12),
      static_cast<GLenum>(13),
      static_cast<GLboolean>(14),
      static_cast<GLsizei>(15),
      static_cast<GLuint>(16));
  EXPECT_EQ(static_cast<uint32>(VertexAttribPointer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<GLint>(12), cmd.size);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.type);
  EXPECT_EQ(static_cast<GLboolean>(14), cmd.normalized);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.stride);
  EXPECT_EQ(static_cast<GLuint>(16), cmd.offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Viewport) {
  Viewport& cmd = *GetBufferAs<Viewport>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLint>(12),
      static_cast<GLsizei>(13),
      static_cast<GLsizei>(14));
  EXPECT_EQ(static_cast<uint32>(Viewport::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.x);
  EXPECT_EQ(static_cast<GLint>(12), cmd.y);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, BlitFramebufferEXT) {
  BlitFramebufferEXT& cmd = *GetBufferAs<BlitFramebufferEXT>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLint>(12),
      static_cast<GLint>(13),
      static_cast<GLint>(14),
      static_cast<GLint>(15),
      static_cast<GLint>(16),
      static_cast<GLint>(17),
      static_cast<GLint>(18),
      static_cast<GLbitfield>(19),
      static_cast<GLenum>(20));
  EXPECT_EQ(static_cast<uint32>(BlitFramebufferEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.srcX0);
  EXPECT_EQ(static_cast<GLint>(12), cmd.srcY0);
  EXPECT_EQ(static_cast<GLint>(13), cmd.srcX1);
  EXPECT_EQ(static_cast<GLint>(14), cmd.srcY1);
  EXPECT_EQ(static_cast<GLint>(15), cmd.dstX0);
  EXPECT_EQ(static_cast<GLint>(16), cmd.dstY0);
  EXPECT_EQ(static_cast<GLint>(17), cmd.dstX1);
  EXPECT_EQ(static_cast<GLint>(18), cmd.dstY1);
  EXPECT_EQ(static_cast<GLbitfield>(19), cmd.mask);
  EXPECT_EQ(static_cast<GLenum>(20), cmd.filter);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, RenderbufferStorageMultisampleEXT) {
  RenderbufferStorageMultisampleEXT& cmd =
      *GetBufferAs<RenderbufferStorageMultisampleEXT>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLsizei>(12),
      static_cast<GLenum>(13),
      static_cast<GLsizei>(14),
      static_cast<GLsizei>(15));
  EXPECT_EQ(static_cast<uint32>(RenderbufferStorageMultisampleEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.samples);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.internalformat);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, SwapBuffers) {
  SwapBuffers& cmd = *GetBufferAs<SwapBuffers>();
  void* next_cmd = cmd.Set(
      &cmd);
  EXPECT_EQ(static_cast<uint32>(SwapBuffers::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetMaxValueInBufferCHROMIUM) {
  GetMaxValueInBufferCHROMIUM& cmd =
      *GetBufferAs<GetMaxValueInBufferCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLsizei>(12),
      static_cast<GLenum>(13),
      static_cast<GLuint>(14),
      static_cast<uint32>(15),
      static_cast<uint32>(16));
  EXPECT_EQ(static_cast<uint32>(GetMaxValueInBufferCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.buffer_id);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.type);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.offset);
  EXPECT_EQ(static_cast<uint32>(15), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(16), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GenSharedIdsCHROMIUM) {
  GenSharedIdsCHROMIUM& cmd = *GetBufferAs<GenSharedIdsCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLuint>(12),
      static_cast<GLsizei>(13),
      static_cast<uint32>(14),
      static_cast<uint32>(15));
  EXPECT_EQ(static_cast<uint32>(GenSharedIdsCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.namespace_id);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.id_offset);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.n);
  EXPECT_EQ(static_cast<uint32>(14), cmd.ids_shm_id);
  EXPECT_EQ(static_cast<uint32>(15), cmd.ids_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DeleteSharedIdsCHROMIUM) {
  DeleteSharedIdsCHROMIUM& cmd = *GetBufferAs<DeleteSharedIdsCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(DeleteSharedIdsCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.namespace_id);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.n);
  EXPECT_EQ(static_cast<uint32>(13), cmd.ids_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.ids_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, RegisterSharedIdsCHROMIUM) {
  RegisterSharedIdsCHROMIUM& cmd = *GetBufferAs<RegisterSharedIdsCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(RegisterSharedIdsCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.namespace_id);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.n);
  EXPECT_EQ(static_cast<uint32>(13), cmd.ids_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.ids_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, EnableFeatureCHROMIUM) {
  EnableFeatureCHROMIUM& cmd = *GetBufferAs<EnableFeatureCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(EnableFeatureCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.bucket_id);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, ResizeCHROMIUM) {
  ResizeCHROMIUM& cmd = *GetBufferAs<ResizeCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32>(ResizeCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.width);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetRequestableExtensionsCHROMIUM) {
  GetRequestableExtensionsCHROMIUM& cmd =
      *GetBufferAs<GetRequestableExtensionsCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<uint32>(11));
  EXPECT_EQ(static_cast<uint32>(GetRequestableExtensionsCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<uint32>(11), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, RequestExtensionCHROMIUM) {
  RequestExtensionCHROMIUM& cmd = *GetBufferAs<RequestExtensionCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<uint32>(11));
  EXPECT_EQ(static_cast<uint32>(RequestExtensionCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<uint32>(11), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetMultipleIntegervCHROMIUM) {
  GetMultipleIntegervCHROMIUM& cmd =
      *GetBufferAs<GetMultipleIntegervCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<uint32>(11),
      static_cast<uint32>(12),
      static_cast<GLuint>(13),
      static_cast<uint32>(14),
      static_cast<uint32>(15),
      static_cast<GLsizeiptr>(16));
  EXPECT_EQ(static_cast<uint32>(GetMultipleIntegervCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<uint32>(11), cmd.pnames_shm_id);
  EXPECT_EQ(static_cast<uint32>(12), cmd.pnames_shm_offset);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.count);
  EXPECT_EQ(static_cast<uint32>(14), cmd.results_shm_id);
  EXPECT_EQ(static_cast<uint32>(15), cmd.results_shm_offset);
  EXPECT_EQ(static_cast<GLsizeiptr>(16), cmd.size);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetProgramInfoCHROMIUM) {
  GetProgramInfoCHROMIUM& cmd = *GetBufferAs<GetProgramInfoCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12));
  EXPECT_EQ(static_cast<uint32>(GetProgramInfoCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<uint32>(12), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Placeholder447CHROMIUM) {
  Placeholder447CHROMIUM& cmd = *GetBufferAs<Placeholder447CHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd);
  EXPECT_EQ(static_cast<uint32>(Placeholder447CHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, CreateStreamTextureCHROMIUM) {
  CreateStreamTextureCHROMIUM& cmd =
      *GetBufferAs<CreateStreamTextureCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(CreateStreamTextureCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.client_id);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, DestroyStreamTextureCHROMIUM) {
  DestroyStreamTextureCHROMIUM& cmd =
      *GetBufferAs<DestroyStreamTextureCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(DestroyStreamTextureCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.texture);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, Placeholder453CHROMIUM) {
  Placeholder453CHROMIUM& cmd = *GetBufferAs<Placeholder453CHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd);
  EXPECT_EQ(static_cast<uint32>(Placeholder453CHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, GetTranslatedShaderSourceANGLE) {
  GetTranslatedShaderSourceANGLE& cmd =
      *GetBufferAs<GetTranslatedShaderSourceANGLE>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12));
  EXPECT_EQ(static_cast<uint32>(GetTranslatedShaderSourceANGLE::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  EXPECT_EQ(static_cast<uint32>(12), cmd.bucket_id);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, PostSubBufferCHROMIUM) {
  PostSubBufferCHROMIUM& cmd = *GetBufferAs<PostSubBufferCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLint>(12),
      static_cast<GLint>(13),
      static_cast<GLint>(14));
  EXPECT_EQ(static_cast<uint32>(PostSubBufferCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.x);
  EXPECT_EQ(static_cast<GLint>(12), cmd.y);
  EXPECT_EQ(static_cast<GLint>(13), cmd.width);
  EXPECT_EQ(static_cast<GLint>(14), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

TEST_F(GLES2FormatTest, TexImageIOSurface2DCHROMIUM) {
  TexImageIOSurface2DCHROMIUM& cmd =
      *GetBufferAs<TexImageIOSurface2DCHROMIUM>();
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLsizei>(12),
      static_cast<GLsizei>(13),
      static_cast<GLuint>(14),
      static_cast<GLuint>(15));
  EXPECT_EQ(static_cast<uint32>(TexImageIOSurface2DCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.height);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.ioSurfaceId);
  EXPECT_EQ(static_cast<GLuint>(15), cmd.plane);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd));
}

#endif  // GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_FORMAT_TEST_AUTOGEN_H_

