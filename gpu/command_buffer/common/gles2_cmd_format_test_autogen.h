// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated. DO NOT EDIT!

// This file contains unit tests for gles2 commmands
// It is included by gles2_cmd_format_test.cc

#ifndef GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_FORMAT_TEST_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_FORMAT_TEST_AUTOGEN_H_

TEST(GLES2FormatTest, ActiveTexture) {
  ActiveTexture cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32>(ActiveTexture::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.texture);
}

TEST(GLES2FormatTest, AttachShader) {
  AttachShader cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32>(AttachShader::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.shader);
}

TEST(GLES2FormatTest, BindAttribLocation) {
  BindAttribLocation cmd = { { 0 } };
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
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.index);
  EXPECT_EQ(static_cast<uint32>(13), cmd.name_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.name_shm_offset);
  EXPECT_EQ(static_cast<uint32>(15), cmd.data_size);
}


TEST(GLES2FormatTest, BindAttribLocationImmediate) {
  int8 buf[256] = { 0, };
  BindAttribLocationImmediate& cmd =
      *static_cast<BindAttribLocationImmediate*>(static_cast<void*>(&buf));
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
}

TEST(GLES2FormatTest, BindAttribLocationBucket) {
  BindAttribLocationBucket cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLuint>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(BindAttribLocationBucket::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.index);
  EXPECT_EQ(static_cast<uint32>(13), cmd.name_bucket_id);
}

TEST(GLES2FormatTest, BindBuffer) {
  BindBuffer cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32>(BindBuffer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.buffer);
}

TEST(GLES2FormatTest, BindFramebuffer) {
  BindFramebuffer cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32>(BindFramebuffer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.framebuffer);
}

TEST(GLES2FormatTest, BindRenderbuffer) {
  BindRenderbuffer cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32>(BindRenderbuffer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.renderbuffer);
}

TEST(GLES2FormatTest, BindTexture) {
  BindTexture cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32>(BindTexture::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.texture);
}

TEST(GLES2FormatTest, BlendColor) {
  BlendColor cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLclampf>(11),
      static_cast<GLclampf>(12),
      static_cast<GLclampf>(13),
      static_cast<GLclampf>(14));
  EXPECT_EQ(static_cast<uint32>(BlendColor::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLclampf>(11), cmd.red);
  EXPECT_EQ(static_cast<GLclampf>(12), cmd.green);
  EXPECT_EQ(static_cast<GLclampf>(13), cmd.blue);
  EXPECT_EQ(static_cast<GLclampf>(14), cmd.alpha);
}

TEST(GLES2FormatTest, BlendEquation) {
  BlendEquation cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32>(BlendEquation::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
}

TEST(GLES2FormatTest, BlendEquationSeparate) {
  BlendEquationSeparate cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12));
  EXPECT_EQ(static_cast<uint32>(BlendEquationSeparate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.modeRGB);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.modeAlpha);
}

TEST(GLES2FormatTest, BlendFunc) {
  BlendFunc cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12));
  EXPECT_EQ(static_cast<uint32>(BlendFunc::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.sfactor);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.dfactor);
}

TEST(GLES2FormatTest, BlendFuncSeparate) {
  BlendFuncSeparate cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLenum>(13),
      static_cast<GLenum>(14));
  EXPECT_EQ(static_cast<uint32>(BlendFuncSeparate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.srcRGB);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.dstRGB);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.srcAlpha);
  EXPECT_EQ(static_cast<GLenum>(14), cmd.dstAlpha);
}

TEST(GLES2FormatTest, BufferData) {
  BufferData cmd = { { 0 } };
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
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLsizeiptr>(12), cmd.size);
  EXPECT_EQ(static_cast<uint32>(13), cmd.data_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.data_shm_offset);
  EXPECT_EQ(static_cast<GLenum>(15), cmd.usage);
}

// TODO(gman): Implement test for BufferDataImmediate
TEST(GLES2FormatTest, BufferSubData) {
  BufferSubData cmd = { { 0 } };
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
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLintptr>(12), cmd.offset);
  EXPECT_EQ(static_cast<GLsizeiptr>(13), cmd.size);
  EXPECT_EQ(static_cast<uint32>(14), cmd.data_shm_id);
  EXPECT_EQ(static_cast<uint32>(15), cmd.data_shm_offset);
}

// TODO(gman): Implement test for BufferSubDataImmediate
TEST(GLES2FormatTest, CheckFramebufferStatus) {
  CheckFramebufferStatus cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(CheckFramebufferStatus::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
}

TEST(GLES2FormatTest, Clear) {
  Clear cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLbitfield>(11));
  EXPECT_EQ(static_cast<uint32>(Clear::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLbitfield>(11), cmd.mask);
}

TEST(GLES2FormatTest, ClearColor) {
  ClearColor cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLclampf>(11),
      static_cast<GLclampf>(12),
      static_cast<GLclampf>(13),
      static_cast<GLclampf>(14));
  EXPECT_EQ(static_cast<uint32>(ClearColor::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLclampf>(11), cmd.red);
  EXPECT_EQ(static_cast<GLclampf>(12), cmd.green);
  EXPECT_EQ(static_cast<GLclampf>(13), cmd.blue);
  EXPECT_EQ(static_cast<GLclampf>(14), cmd.alpha);
}

TEST(GLES2FormatTest, ClearDepthf) {
  ClearDepthf cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLclampf>(11));
  EXPECT_EQ(static_cast<uint32>(ClearDepthf::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLclampf>(11), cmd.depth);
}

TEST(GLES2FormatTest, ClearStencil) {
  ClearStencil cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11));
  EXPECT_EQ(static_cast<uint32>(ClearStencil::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLint>(11), cmd.s);
}

TEST(GLES2FormatTest, ColorMask) {
  ColorMask cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLboolean>(11),
      static_cast<GLboolean>(12),
      static_cast<GLboolean>(13),
      static_cast<GLboolean>(14));
  EXPECT_EQ(static_cast<uint32>(ColorMask::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLboolean>(11), cmd.red);
  EXPECT_EQ(static_cast<GLboolean>(12), cmd.green);
  EXPECT_EQ(static_cast<GLboolean>(13), cmd.blue);
  EXPECT_EQ(static_cast<GLboolean>(14), cmd.alpha);
}

TEST(GLES2FormatTest, CompileShader) {
  CompileShader cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(CompileShader::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
}

TEST(GLES2FormatTest, CompressedTexImage2D) {
  CompressedTexImage2D cmd = { { 0 } };
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
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.internalformat);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.height);
  EXPECT_EQ(static_cast<GLint>(16), cmd.border);
  EXPECT_EQ(static_cast<GLsizei>(17), cmd.imageSize);
  EXPECT_EQ(static_cast<uint32>(18), cmd.data_shm_id);
  EXPECT_EQ(static_cast<uint32>(19), cmd.data_shm_offset);
}

// TODO(gman): Implement test for CompressedTexImage2DImmediate
TEST(GLES2FormatTest, CompressedTexImage2DBucket) {
  CompressedTexImage2DBucket cmd = { { 0 } };
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
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.internalformat);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.height);
  EXPECT_EQ(static_cast<GLint>(16), cmd.border);
  EXPECT_EQ(static_cast<GLuint>(17), cmd.bucket_id);
}

TEST(GLES2FormatTest, CompressedTexSubImage2D) {
  CompressedTexSubImage2D cmd = { { 0 } };
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
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
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
}

// TODO(gman): Implement test for CompressedTexSubImage2DImmediate
TEST(GLES2FormatTest, CompressedTexSubImage2DBucket) {
  CompressedTexSubImage2DBucket cmd = { { 0 } };
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
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLint>(13), cmd.xoffset);
  EXPECT_EQ(static_cast<GLint>(14), cmd.yoffset);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.height);
  EXPECT_EQ(static_cast<GLenum>(17), cmd.format);
  EXPECT_EQ(static_cast<GLuint>(18), cmd.bucket_id);
}

TEST(GLES2FormatTest, CopyTexImage2D) {
  CopyTexImage2D cmd = { { 0 } };
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
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.internalformat);
  EXPECT_EQ(static_cast<GLint>(14), cmd.x);
  EXPECT_EQ(static_cast<GLint>(15), cmd.y);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(17), cmd.height);
  EXPECT_EQ(static_cast<GLint>(18), cmd.border);
}

TEST(GLES2FormatTest, CopyTexSubImage2D) {
  CopyTexSubImage2D cmd = { { 0 } };
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
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLint>(12), cmd.level);
  EXPECT_EQ(static_cast<GLint>(13), cmd.xoffset);
  EXPECT_EQ(static_cast<GLint>(14), cmd.yoffset);
  EXPECT_EQ(static_cast<GLint>(15), cmd.x);
  EXPECT_EQ(static_cast<GLint>(16), cmd.y);
  EXPECT_EQ(static_cast<GLsizei>(17), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(18), cmd.height);
}

TEST(GLES2FormatTest, CreateProgram) {
  CreateProgram cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<uint32>(11));
  EXPECT_EQ(static_cast<uint32>(CreateProgram::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<uint32>(11), cmd.client_id);
}

TEST(GLES2FormatTest, CreateShader) {
  CreateShader cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<uint32>(12));
  EXPECT_EQ(static_cast<uint32>(CreateShader::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.type);
  EXPECT_EQ(static_cast<uint32>(12), cmd.client_id);
}

TEST(GLES2FormatTest, CullFace) {
  CullFace cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32>(CullFace::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
}

TEST(GLES2FormatTest, DeleteBuffers) {
  DeleteBuffers cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(DeleteBuffers::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.buffers_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.buffers_shm_offset);
}

TEST(GLES2FormatTest, DeleteBuffersImmediate) {
  static GLuint ids[] = { 12, 23, 34, };
  int8 buf[256] = { 0, };
  DeleteBuffersImmediate& cmd =
      *static_cast<DeleteBuffersImmediate*>(static_cast<void*>(&buf));
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      ids);
  EXPECT_EQ(static_cast<uint32>(DeleteBuffersImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +
                RoundSizeToMultipleOfEntries(cmd.n * 4u));
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  // TODO(gman): Check that ids were inserted;
}

TEST(GLES2FormatTest, DeleteFramebuffers) {
  DeleteFramebuffers cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(DeleteFramebuffers::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.framebuffers_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.framebuffers_shm_offset);
}

TEST(GLES2FormatTest, DeleteFramebuffersImmediate) {
  static GLuint ids[] = { 12, 23, 34, };
  int8 buf[256] = { 0, };
  DeleteFramebuffersImmediate& cmd =
      *static_cast<DeleteFramebuffersImmediate*>(static_cast<void*>(&buf));
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      ids);
  EXPECT_EQ(static_cast<uint32>(DeleteFramebuffersImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +
                RoundSizeToMultipleOfEntries(cmd.n * 4u));
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  // TODO(gman): Check that ids were inserted;
}

TEST(GLES2FormatTest, DeleteProgram) {
  DeleteProgram cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(DeleteProgram::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
}

TEST(GLES2FormatTest, DeleteRenderbuffers) {
  DeleteRenderbuffers cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(DeleteRenderbuffers::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.renderbuffers_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.renderbuffers_shm_offset);
}

TEST(GLES2FormatTest, DeleteRenderbuffersImmediate) {
  static GLuint ids[] = { 12, 23, 34, };
  int8 buf[256] = { 0, };
  DeleteRenderbuffersImmediate& cmd =
      *static_cast<DeleteRenderbuffersImmediate*>(static_cast<void*>(&buf));
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      ids);
  EXPECT_EQ(static_cast<uint32>(DeleteRenderbuffersImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +
                RoundSizeToMultipleOfEntries(cmd.n * 4u));
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  // TODO(gman): Check that ids were inserted;
}

TEST(GLES2FormatTest, DeleteShader) {
  DeleteShader cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(DeleteShader::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
}

TEST(GLES2FormatTest, DeleteTextures) {
  DeleteTextures cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(DeleteTextures::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.textures_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.textures_shm_offset);
}

TEST(GLES2FormatTest, DeleteTexturesImmediate) {
  static GLuint ids[] = { 12, 23, 34, };
  int8 buf[256] = { 0, };
  DeleteTexturesImmediate& cmd =
      *static_cast<DeleteTexturesImmediate*>(static_cast<void*>(&buf));
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      ids);
  EXPECT_EQ(static_cast<uint32>(DeleteTexturesImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +
                RoundSizeToMultipleOfEntries(cmd.n * 4u));
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  // TODO(gman): Check that ids were inserted;
}

TEST(GLES2FormatTest, DepthFunc) {
  DepthFunc cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32>(DepthFunc::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.func);
}

TEST(GLES2FormatTest, DepthMask) {
  DepthMask cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLboolean>(11));
  EXPECT_EQ(static_cast<uint32>(DepthMask::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLboolean>(11), cmd.flag);
}

TEST(GLES2FormatTest, DepthRangef) {
  DepthRangef cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLclampf>(11),
      static_cast<GLclampf>(12));
  EXPECT_EQ(static_cast<uint32>(DepthRangef::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLclampf>(11), cmd.zNear);
  EXPECT_EQ(static_cast<GLclampf>(12), cmd.zFar);
}

TEST(GLES2FormatTest, DetachShader) {
  DetachShader cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32>(DetachShader::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.shader);
}

TEST(GLES2FormatTest, Disable) {
  Disable cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32>(Disable::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.cap);
}

TEST(GLES2FormatTest, DisableVertexAttribArray) {
  DisableVertexAttribArray cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(DisableVertexAttribArray::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.index);
}

TEST(GLES2FormatTest, DrawArrays) {
  DrawArrays cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12),
      static_cast<GLsizei>(13));
  EXPECT_EQ(static_cast<uint32>(DrawArrays::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  EXPECT_EQ(static_cast<GLint>(12), cmd.first);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.count);
}

TEST(GLES2FormatTest, DrawElements) {
  DrawElements cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLsizei>(12),
      static_cast<GLenum>(13),
      static_cast<GLuint>(14));
  EXPECT_EQ(static_cast<uint32>(DrawElements::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.type);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.index_offset);
}

TEST(GLES2FormatTest, Enable) {
  Enable cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32>(Enable::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.cap);
}

TEST(GLES2FormatTest, EnableVertexAttribArray) {
  EnableVertexAttribArray cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(EnableVertexAttribArray::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.index);
}

TEST(GLES2FormatTest, Finish) {
  Finish cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd);
  EXPECT_EQ(static_cast<uint32>(Finish::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
}

TEST(GLES2FormatTest, Flush) {
  Flush cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd);
  EXPECT_EQ(static_cast<uint32>(Flush::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
}

TEST(GLES2FormatTest, FramebufferRenderbuffer) {
  FramebufferRenderbuffer cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLenum>(13),
      static_cast<GLuint>(14));
  EXPECT_EQ(static_cast<uint32>(FramebufferRenderbuffer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.attachment);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.renderbuffertarget);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.renderbuffer);
}

TEST(GLES2FormatTest, FramebufferTexture2D) {
  FramebufferTexture2D cmd = { { 0 } };
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
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.attachment);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.textarget);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.texture);
  EXPECT_EQ(static_cast<GLint>(15), cmd.level);
}

TEST(GLES2FormatTest, FrontFace) {
  FrontFace cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32>(FrontFace::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.mode);
}

TEST(GLES2FormatTest, GenBuffers) {
  GenBuffers cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(GenBuffers::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.buffers_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.buffers_shm_offset);
}

TEST(GLES2FormatTest, GenBuffersImmediate) {
  static GLuint ids[] = { 12, 23, 34, };
  int8 buf[256] = { 0, };
  GenBuffersImmediate& cmd =
      *static_cast<GenBuffersImmediate*>(static_cast<void*>(&buf));
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      ids);
  EXPECT_EQ(static_cast<uint32>(GenBuffersImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +
                RoundSizeToMultipleOfEntries(cmd.n * 4u));
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  // TODO(gman): Check that ids were inserted;
}

TEST(GLES2FormatTest, GenerateMipmap) {
  GenerateMipmap cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11));
  EXPECT_EQ(static_cast<uint32>(GenerateMipmap::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
}

TEST(GLES2FormatTest, GenFramebuffers) {
  GenFramebuffers cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(GenFramebuffers::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.framebuffers_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.framebuffers_shm_offset);
}

TEST(GLES2FormatTest, GenFramebuffersImmediate) {
  static GLuint ids[] = { 12, 23, 34, };
  int8 buf[256] = { 0, };
  GenFramebuffersImmediate& cmd =
      *static_cast<GenFramebuffersImmediate*>(static_cast<void*>(&buf));
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      ids);
  EXPECT_EQ(static_cast<uint32>(GenFramebuffersImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +
                RoundSizeToMultipleOfEntries(cmd.n * 4u));
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  // TODO(gman): Check that ids were inserted;
}

TEST(GLES2FormatTest, GenRenderbuffers) {
  GenRenderbuffers cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(GenRenderbuffers::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.renderbuffers_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.renderbuffers_shm_offset);
}

TEST(GLES2FormatTest, GenRenderbuffersImmediate) {
  static GLuint ids[] = { 12, 23, 34, };
  int8 buf[256] = { 0, };
  GenRenderbuffersImmediate& cmd =
      *static_cast<GenRenderbuffersImmediate*>(static_cast<void*>(&buf));
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      ids);
  EXPECT_EQ(static_cast<uint32>(GenRenderbuffersImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +
                RoundSizeToMultipleOfEntries(cmd.n * 4u));
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  // TODO(gman): Check that ids were inserted;
}

TEST(GLES2FormatTest, GenTextures) {
  GenTextures cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(GenTextures::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.textures_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.textures_shm_offset);
}

TEST(GLES2FormatTest, GenTexturesImmediate) {
  static GLuint ids[] = { 12, 23, 34, };
  int8 buf[256] = { 0, };
  GenTexturesImmediate& cmd =
      *static_cast<GenTexturesImmediate*>(static_cast<void*>(&buf));
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLsizei>(11),
      ids);
  EXPECT_EQ(static_cast<uint32>(GenTexturesImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +
                RoundSizeToMultipleOfEntries(cmd.n * 4u));
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  // TODO(gman): Check that ids were inserted;
}

TEST(GLES2FormatTest, GetActiveAttrib) {
  GetActiveAttrib cmd = { { 0 } };
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
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.index);
  EXPECT_EQ(static_cast<uint32>(13), cmd.name_bucket_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(15), cmd.result_shm_offset);
}

TEST(GLES2FormatTest, GetActiveUniform) {
  GetActiveUniform cmd = { { 0 } };
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
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.index);
  EXPECT_EQ(static_cast<uint32>(13), cmd.name_bucket_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(15), cmd.result_shm_offset);
}

TEST(GLES2FormatTest, GetAttachedShaders) {
  GetAttachedShaders cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(GetAttachedShaders::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
  EXPECT_EQ(static_cast<uint32>(14), cmd.result_size);
}

// TODO(gman): Write test for GetAttribLocation
// TODO(gman): Write test for GetAttribLocationImmediate
// TODO(gman): Write test for GetAttribLocationBucket
TEST(GLES2FormatTest, GetBooleanv) {
  GetBooleanv cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(GetBooleanv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(12), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_offset);
}

TEST(GLES2FormatTest, GetBufferParameteriv) {
  GetBufferParameteriv cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(GetBufferParameteriv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
}

TEST(GLES2FormatTest, GetError) {
  GetError cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<uint32>(11),
      static_cast<uint32>(12));
  EXPECT_EQ(static_cast<uint32>(GetError::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<uint32>(11), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_offset);
}

TEST(GLES2FormatTest, GetFloatv) {
  GetFloatv cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(GetFloatv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(12), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_offset);
}

TEST(GLES2FormatTest, GetFramebufferAttachmentParameteriv) {
  GetFramebufferAttachmentParameteriv cmd = { { 0 } };
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
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.attachment);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(15), cmd.params_shm_offset);
}

TEST(GLES2FormatTest, GetIntegerv) {
  GetIntegerv cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(GetIntegerv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(12), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_offset);
}

TEST(GLES2FormatTest, GetProgramiv) {
  GetProgramiv cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(GetProgramiv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
}

TEST(GLES2FormatTest, GetProgramInfoLog) {
  GetProgramInfoLog cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12));
  EXPECT_EQ(static_cast<uint32>(GetProgramInfoLog::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<uint32>(12), cmd.bucket_id);
}

TEST(GLES2FormatTest, GetRenderbufferParameteriv) {
  GetRenderbufferParameteriv cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(GetRenderbufferParameteriv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
}

TEST(GLES2FormatTest, GetShaderiv) {
  GetShaderiv cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(GetShaderiv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
}

TEST(GLES2FormatTest, GetShaderInfoLog) {
  GetShaderInfoLog cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12));
  EXPECT_EQ(static_cast<uint32>(GetShaderInfoLog::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  EXPECT_EQ(static_cast<uint32>(12), cmd.bucket_id);
}

TEST(GLES2FormatTest, GetShaderPrecisionFormat) {
  GetShaderPrecisionFormat cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(GetShaderPrecisionFormat::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.shadertype);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.precisiontype);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.result_shm_offset);
}

TEST(GLES2FormatTest, GetShaderSource) {
  GetShaderSource cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12));
  EXPECT_EQ(static_cast<uint32>(GetShaderSource::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  EXPECT_EQ(static_cast<uint32>(12), cmd.bucket_id);
}

TEST(GLES2FormatTest, GetString) {
  GetString cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<uint32>(12));
  EXPECT_EQ(static_cast<uint32>(GetString::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.name);
  EXPECT_EQ(static_cast<uint32>(12), cmd.bucket_id);
}

TEST(GLES2FormatTest, GetTexParameterfv) {
  GetTexParameterfv cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(GetTexParameterfv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
}

TEST(GLES2FormatTest, GetTexParameteriv) {
  GetTexParameteriv cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(GetTexParameteriv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
}

TEST(GLES2FormatTest, GetUniformfv) {
  GetUniformfv cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLint>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(GetUniformfv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLint>(12), cmd.location);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
}

TEST(GLES2FormatTest, GetUniformiv) {
  GetUniformiv cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLint>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(GetUniformiv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<GLint>(12), cmd.location);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
}

// TODO(gman): Write test for GetUniformLocation
// TODO(gman): Write test for GetUniformLocationImmediate
// TODO(gman): Write test for GetUniformLocationBucket
TEST(GLES2FormatTest, GetVertexAttribfv) {
  GetVertexAttribfv cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(GetVertexAttribfv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.index);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
}

TEST(GLES2FormatTest, GetVertexAttribiv) {
  GetVertexAttribiv cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(GetVertexAttribiv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.index);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
}

TEST(GLES2FormatTest, GetVertexAttribPointerv) {
  GetVertexAttribPointerv cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(GetVertexAttribPointerv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.index);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.pointer_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.pointer_shm_offset);
}

TEST(GLES2FormatTest, Hint) {
  Hint cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12));
  EXPECT_EQ(static_cast<uint32>(Hint::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.mode);
}

TEST(GLES2FormatTest, IsBuffer) {
  IsBuffer cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(IsBuffer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.buffer);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
}

TEST(GLES2FormatTest, IsEnabled) {
  IsEnabled cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(IsEnabled::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.cap);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
}

TEST(GLES2FormatTest, IsFramebuffer) {
  IsFramebuffer cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(IsFramebuffer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.framebuffer);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
}

TEST(GLES2FormatTest, IsProgram) {
  IsProgram cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(IsProgram::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
}

TEST(GLES2FormatTest, IsRenderbuffer) {
  IsRenderbuffer cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(IsRenderbuffer::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.renderbuffer);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
}

TEST(GLES2FormatTest, IsShader) {
  IsShader cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(IsShader::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
}

TEST(GLES2FormatTest, IsTexture) {
  IsTexture cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(IsTexture::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.texture);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
}

TEST(GLES2FormatTest, LineWidth) {
  LineWidth cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLfloat>(11));
  EXPECT_EQ(static_cast<uint32>(LineWidth::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLfloat>(11), cmd.width);
}

TEST(GLES2FormatTest, LinkProgram) {
  LinkProgram cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(LinkProgram::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
}

TEST(GLES2FormatTest, PixelStorei) {
  PixelStorei cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12));
  EXPECT_EQ(static_cast<uint32>(PixelStorei::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.pname);
  EXPECT_EQ(static_cast<GLint>(12), cmd.param);
}

TEST(GLES2FormatTest, PolygonOffset) {
  PolygonOffset cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLfloat>(11),
      static_cast<GLfloat>(12));
  EXPECT_EQ(static_cast<uint32>(PolygonOffset::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLfloat>(11), cmd.factor);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.units);
}

TEST(GLES2FormatTest, ReadPixels) {
  ReadPixels cmd = { { 0 } };
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
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
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
}

TEST(GLES2FormatTest, ReleaseShaderCompiler) {
  ReleaseShaderCompiler cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd);
  EXPECT_EQ(static_cast<uint32>(ReleaseShaderCompiler::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
}

TEST(GLES2FormatTest, RenderbufferStorage) {
  RenderbufferStorage cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLsizei>(13),
      static_cast<GLsizei>(14));
  EXPECT_EQ(static_cast<uint32>(RenderbufferStorage::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.internalformat);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.height);
}

TEST(GLES2FormatTest, SampleCoverage) {
  SampleCoverage cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLclampf>(11),
      static_cast<GLboolean>(12));
  EXPECT_EQ(static_cast<uint32>(SampleCoverage::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLclampf>(11), cmd.value);
  EXPECT_EQ(static_cast<GLboolean>(12), cmd.invert);
}

TEST(GLES2FormatTest, Scissor) {
  Scissor cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLint>(12),
      static_cast<GLsizei>(13),
      static_cast<GLsizei>(14));
  EXPECT_EQ(static_cast<uint32>(Scissor::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLint>(11), cmd.x);
  EXPECT_EQ(static_cast<GLint>(12), cmd.y);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.height);
}

TEST(GLES2FormatTest, ShaderBinary) {
  ShaderBinary cmd = { { 0 } };
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
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32>(12), cmd.shaders_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.shaders_shm_offset);
  EXPECT_EQ(static_cast<GLenum>(14), cmd.binaryformat);
  EXPECT_EQ(static_cast<uint32>(15), cmd.binary_shm_id);
  EXPECT_EQ(static_cast<uint32>(16), cmd.binary_shm_offset);
  EXPECT_EQ(static_cast<GLsizei>(17), cmd.length);
}

TEST(GLES2FormatTest, ShaderSource) {
  ShaderSource cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(ShaderSource::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  EXPECT_EQ(static_cast<uint32>(12), cmd.data_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.data_shm_offset);
  EXPECT_EQ(static_cast<uint32>(14), cmd.data_size);
}

// TODO(gman): Implement test for ShaderSourceImmediate
TEST(GLES2FormatTest, ShaderSourceBucket) {
  ShaderSourceBucket cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12));
  EXPECT_EQ(static_cast<uint32>(ShaderSourceBucket::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.shader);
  EXPECT_EQ(static_cast<uint32>(12), cmd.data_bucket_id);
}

TEST(GLES2FormatTest, StencilFunc) {
  StencilFunc cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLint>(12),
      static_cast<GLuint>(13));
  EXPECT_EQ(static_cast<uint32>(StencilFunc::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.func);
  EXPECT_EQ(static_cast<GLint>(12), cmd.ref);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.mask);
}

TEST(GLES2FormatTest, StencilFuncSeparate) {
  StencilFuncSeparate cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLint>(13),
      static_cast<GLuint>(14));
  EXPECT_EQ(static_cast<uint32>(StencilFuncSeparate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.face);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.func);
  EXPECT_EQ(static_cast<GLint>(13), cmd.ref);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.mask);
}

TEST(GLES2FormatTest, StencilMask) {
  StencilMask cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(StencilMask::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.mask);
}

TEST(GLES2FormatTest, StencilMaskSeparate) {
  StencilMaskSeparate cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32>(StencilMaskSeparate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.face);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.mask);
}

TEST(GLES2FormatTest, StencilOp) {
  StencilOp cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLenum>(13));
  EXPECT_EQ(static_cast<uint32>(StencilOp::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.fail);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.zfail);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.zpass);
}

TEST(GLES2FormatTest, StencilOpSeparate) {
  StencilOpSeparate cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLenum>(13),
      static_cast<GLenum>(14));
  EXPECT_EQ(static_cast<uint32>(StencilOpSeparate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.face);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.fail);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.zfail);
  EXPECT_EQ(static_cast<GLenum>(14), cmd.zpass);
}

TEST(GLES2FormatTest, TexImage2D) {
  TexImage2D cmd = { { 0 } };
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
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
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
}

// TODO(gman): Implement test for TexImage2DImmediate
TEST(GLES2FormatTest, TexParameterf) {
  TexParameterf cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLfloat>(13));
  EXPECT_EQ(static_cast<uint32>(TexParameterf::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.param);
}

TEST(GLES2FormatTest, TexParameterfv) {
  TexParameterfv cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(TexParameterfv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
}

TEST(GLES2FormatTest, TexParameterfvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
  };
  int8 buf[256] = { 0, };
  TexParameterfvImmediate& cmd =
      *static_cast<TexParameterfvImmediate*>(static_cast<void*>(&buf));
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
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +
                RoundSizeToMultipleOfEntries(sizeof(data)));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  // TODO(gman): Check that data was inserted;
}

TEST(GLES2FormatTest, TexParameteri) {
  TexParameteri cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<GLint>(13));
  EXPECT_EQ(static_cast<uint32>(TexParameteri::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<GLint>(13), cmd.param);
}

TEST(GLES2FormatTest, TexParameteriv) {
  TexParameteriv cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLenum>(11),
      static_cast<GLenum>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(TexParameteriv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<uint32>(13), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.params_shm_offset);
}

TEST(GLES2FormatTest, TexParameterivImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLint data[] = {
    static_cast<GLint>(kSomeBaseValueToTestWith + 0),
  };
  int8 buf[256] = { 0, };
  TexParameterivImmediate& cmd =
      *static_cast<TexParameterivImmediate*>(static_cast<void*>(&buf));
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
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +
                RoundSizeToMultipleOfEntries(sizeof(data)));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  // TODO(gman): Check that data was inserted;
}

TEST(GLES2FormatTest, TexSubImage2D) {
  TexSubImage2D cmd = { { 0 } };
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
      static_cast<uint32>(20));
  EXPECT_EQ(static_cast<uint32>(TexSubImage2D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
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
}

// TODO(gman): Implement test for TexSubImage2DImmediate
TEST(GLES2FormatTest, Uniform1f) {
  Uniform1f cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLfloat>(12));
  EXPECT_EQ(static_cast<uint32>(Uniform1f::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
}

TEST(GLES2FormatTest, Uniform1fv) {
  Uniform1fv cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(Uniform1fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<uint32>(13), cmd.v_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.v_shm_offset);
}

TEST(GLES2FormatTest, Uniform1fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
  };
  int8 buf[256] = { 0, };
  Uniform1fvImmediate& cmd =
      *static_cast<Uniform1fvImmediate*>(static_cast<void*>(&buf));
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      data);
  EXPECT_EQ(static_cast<uint32>(Uniform1fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +
                RoundSizeToMultipleOfEntries(sizeof(data)));
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  // TODO(gman): Check that data was inserted;
}

TEST(GLES2FormatTest, Uniform1i) {
  Uniform1i cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLint>(12));
  EXPECT_EQ(static_cast<uint32>(Uniform1i::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLint>(12), cmd.x);
}

TEST(GLES2FormatTest, Uniform1iv) {
  Uniform1iv cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(Uniform1iv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<uint32>(13), cmd.v_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.v_shm_offset);
}

TEST(GLES2FormatTest, Uniform1ivImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLint data[] = {
    static_cast<GLint>(kSomeBaseValueToTestWith + 0),
    static_cast<GLint>(kSomeBaseValueToTestWith + 1),
  };
  int8 buf[256] = { 0, };
  Uniform1ivImmediate& cmd =
      *static_cast<Uniform1ivImmediate*>(static_cast<void*>(&buf));
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      data);
  EXPECT_EQ(static_cast<uint32>(Uniform1ivImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +
                RoundSizeToMultipleOfEntries(sizeof(data)));
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  // TODO(gman): Check that data was inserted;
}

TEST(GLES2FormatTest, Uniform2f) {
  Uniform2f cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLfloat>(12),
      static_cast<GLfloat>(13));
  EXPECT_EQ(static_cast<uint32>(Uniform2f::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.y);
}

TEST(GLES2FormatTest, Uniform2fv) {
  Uniform2fv cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(Uniform2fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<uint32>(13), cmd.v_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.v_shm_offset);
}

TEST(GLES2FormatTest, Uniform2fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
  };
  int8 buf[256] = { 0, };
  Uniform2fvImmediate& cmd =
      *static_cast<Uniform2fvImmediate*>(static_cast<void*>(&buf));
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      data);
  EXPECT_EQ(static_cast<uint32>(Uniform2fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +
                RoundSizeToMultipleOfEntries(sizeof(data)));
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  // TODO(gman): Check that data was inserted;
}

TEST(GLES2FormatTest, Uniform2i) {
  Uniform2i cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLint>(12),
      static_cast<GLint>(13));
  EXPECT_EQ(static_cast<uint32>(Uniform2i::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLint>(12), cmd.x);
  EXPECT_EQ(static_cast<GLint>(13), cmd.y);
}

TEST(GLES2FormatTest, Uniform2iv) {
  Uniform2iv cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(Uniform2iv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<uint32>(13), cmd.v_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.v_shm_offset);
}

TEST(GLES2FormatTest, Uniform2ivImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLint data[] = {
    static_cast<GLint>(kSomeBaseValueToTestWith + 0),
    static_cast<GLint>(kSomeBaseValueToTestWith + 1),
    static_cast<GLint>(kSomeBaseValueToTestWith + 2),
    static_cast<GLint>(kSomeBaseValueToTestWith + 3),
  };
  int8 buf[256] = { 0, };
  Uniform2ivImmediate& cmd =
      *static_cast<Uniform2ivImmediate*>(static_cast<void*>(&buf));
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      data);
  EXPECT_EQ(static_cast<uint32>(Uniform2ivImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +
                RoundSizeToMultipleOfEntries(sizeof(data)));
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  // TODO(gman): Check that data was inserted;
}

TEST(GLES2FormatTest, Uniform3f) {
  Uniform3f cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLfloat>(12),
      static_cast<GLfloat>(13),
      static_cast<GLfloat>(14));
  EXPECT_EQ(static_cast<uint32>(Uniform3f::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.y);
  EXPECT_EQ(static_cast<GLfloat>(14), cmd.z);
}

TEST(GLES2FormatTest, Uniform3fv) {
  Uniform3fv cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(Uniform3fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<uint32>(13), cmd.v_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.v_shm_offset);
}

TEST(GLES2FormatTest, Uniform3fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 4),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 5),
  };
  int8 buf[256] = { 0, };
  Uniform3fvImmediate& cmd =
      *static_cast<Uniform3fvImmediate*>(static_cast<void*>(&buf));
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      data);
  EXPECT_EQ(static_cast<uint32>(Uniform3fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +
                RoundSizeToMultipleOfEntries(sizeof(data)));
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  // TODO(gman): Check that data was inserted;
}

TEST(GLES2FormatTest, Uniform3i) {
  Uniform3i cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLint>(12),
      static_cast<GLint>(13),
      static_cast<GLint>(14));
  EXPECT_EQ(static_cast<uint32>(Uniform3i::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLint>(12), cmd.x);
  EXPECT_EQ(static_cast<GLint>(13), cmd.y);
  EXPECT_EQ(static_cast<GLint>(14), cmd.z);
}

TEST(GLES2FormatTest, Uniform3iv) {
  Uniform3iv cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(Uniform3iv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<uint32>(13), cmd.v_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.v_shm_offset);
}

TEST(GLES2FormatTest, Uniform3ivImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLint data[] = {
    static_cast<GLint>(kSomeBaseValueToTestWith + 0),
    static_cast<GLint>(kSomeBaseValueToTestWith + 1),
    static_cast<GLint>(kSomeBaseValueToTestWith + 2),
    static_cast<GLint>(kSomeBaseValueToTestWith + 3),
    static_cast<GLint>(kSomeBaseValueToTestWith + 4),
    static_cast<GLint>(kSomeBaseValueToTestWith + 5),
  };
  int8 buf[256] = { 0, };
  Uniform3ivImmediate& cmd =
      *static_cast<Uniform3ivImmediate*>(static_cast<void*>(&buf));
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      data);
  EXPECT_EQ(static_cast<uint32>(Uniform3ivImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +
                RoundSizeToMultipleOfEntries(sizeof(data)));
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  // TODO(gman): Check that data was inserted;
}

TEST(GLES2FormatTest, Uniform4f) {
  Uniform4f cmd = { { 0 } };
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
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.y);
  EXPECT_EQ(static_cast<GLfloat>(14), cmd.z);
  EXPECT_EQ(static_cast<GLfloat>(15), cmd.w);
}

TEST(GLES2FormatTest, Uniform4fv) {
  Uniform4fv cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(Uniform4fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<uint32>(13), cmd.v_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.v_shm_offset);
}

TEST(GLES2FormatTest, Uniform4fvImmediate) {
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
  int8 buf[256] = { 0, };
  Uniform4fvImmediate& cmd =
      *static_cast<Uniform4fvImmediate*>(static_cast<void*>(&buf));
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      data);
  EXPECT_EQ(static_cast<uint32>(Uniform4fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +
                RoundSizeToMultipleOfEntries(sizeof(data)));
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  // TODO(gman): Check that data was inserted;
}

TEST(GLES2FormatTest, Uniform4i) {
  Uniform4i cmd = { { 0 } };
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
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLint>(12), cmd.x);
  EXPECT_EQ(static_cast<GLint>(13), cmd.y);
  EXPECT_EQ(static_cast<GLint>(14), cmd.z);
  EXPECT_EQ(static_cast<GLint>(15), cmd.w);
}

TEST(GLES2FormatTest, Uniform4iv) {
  Uniform4iv cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(Uniform4iv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<uint32>(13), cmd.v_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.v_shm_offset);
}

TEST(GLES2FormatTest, Uniform4ivImmediate) {
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
  int8 buf[256] = { 0, };
  Uniform4ivImmediate& cmd =
      *static_cast<Uniform4ivImmediate*>(static_cast<void*>(&buf));
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      data);
  EXPECT_EQ(static_cast<uint32>(Uniform4ivImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +
                RoundSizeToMultipleOfEntries(sizeof(data)));
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  // TODO(gman): Check that data was inserted;
}

TEST(GLES2FormatTest, UniformMatrix2fv) {
  UniformMatrix2fv cmd = { { 0 } };
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
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<GLboolean>(13), cmd.transpose);
  EXPECT_EQ(static_cast<uint32>(14), cmd.value_shm_id);
  EXPECT_EQ(static_cast<uint32>(15), cmd.value_shm_offset);
}

TEST(GLES2FormatTest, UniformMatrix2fvImmediate) {
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
  int8 buf[256] = { 0, };
  UniformMatrix2fvImmediate& cmd =
      *static_cast<UniformMatrix2fvImmediate*>(static_cast<void*>(&buf));
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      static_cast<GLboolean>(3),
      data);
  EXPECT_EQ(static_cast<uint32>(UniformMatrix2fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +
                RoundSizeToMultipleOfEntries(sizeof(data)));
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  EXPECT_EQ(static_cast<GLboolean>(3), cmd.transpose);
  // TODO(gman): Check that data was inserted;
}

TEST(GLES2FormatTest, UniformMatrix3fv) {
  UniformMatrix3fv cmd = { { 0 } };
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
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<GLboolean>(13), cmd.transpose);
  EXPECT_EQ(static_cast<uint32>(14), cmd.value_shm_id);
  EXPECT_EQ(static_cast<uint32>(15), cmd.value_shm_offset);
}

TEST(GLES2FormatTest, UniformMatrix3fvImmediate) {
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
  int8 buf[256] = { 0, };
  UniformMatrix3fvImmediate& cmd =
      *static_cast<UniformMatrix3fvImmediate*>(static_cast<void*>(&buf));
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      static_cast<GLboolean>(3),
      data);
  EXPECT_EQ(static_cast<uint32>(UniformMatrix3fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +
                RoundSizeToMultipleOfEntries(sizeof(data)));
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  EXPECT_EQ(static_cast<GLboolean>(3), cmd.transpose);
  // TODO(gman): Check that data was inserted;
}

TEST(GLES2FormatTest, UniformMatrix4fv) {
  UniformMatrix4fv cmd = { { 0 } };
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
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLint>(11), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<GLboolean>(13), cmd.transpose);
  EXPECT_EQ(static_cast<uint32>(14), cmd.value_shm_id);
  EXPECT_EQ(static_cast<uint32>(15), cmd.value_shm_offset);
}

TEST(GLES2FormatTest, UniformMatrix4fvImmediate) {
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
  int8 buf[256] = { 0, };
  UniformMatrix4fvImmediate& cmd =
      *static_cast<UniformMatrix4fvImmediate*>(static_cast<void*>(&buf));
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(1),
      static_cast<GLsizei>(2),
      static_cast<GLboolean>(3),
      data);
  EXPECT_EQ(static_cast<uint32>(UniformMatrix4fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +
                RoundSizeToMultipleOfEntries(sizeof(data)));
  EXPECT_EQ(static_cast<GLint>(1), cmd.location);
  EXPECT_EQ(static_cast<GLsizei>(2), cmd.count);
  EXPECT_EQ(static_cast<GLboolean>(3), cmd.transpose);
  // TODO(gman): Check that data was inserted;
}

TEST(GLES2FormatTest, UseProgram) {
  UseProgram cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(UseProgram::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
}

TEST(GLES2FormatTest, ValidateProgram) {
  ValidateProgram cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32>(ValidateProgram::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.program);
}

TEST(GLES2FormatTest, VertexAttrib1f) {
  VertexAttrib1f cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLfloat>(12));
  EXPECT_EQ(static_cast<uint32>(VertexAttrib1f::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
}

TEST(GLES2FormatTest, VertexAttrib1fv) {
  VertexAttrib1fv cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(VertexAttrib1fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<uint32>(12), cmd.values_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.values_shm_offset);
}

TEST(GLES2FormatTest, VertexAttrib1fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
  };
  int8 buf[256] = { 0, };
  VertexAttrib1fvImmediate& cmd =
      *static_cast<VertexAttrib1fvImmediate*>(static_cast<void*>(&buf));
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      data);
  EXPECT_EQ(static_cast<uint32>(VertexAttrib1fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +
                RoundSizeToMultipleOfEntries(sizeof(data)));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  // TODO(gman): Check that data was inserted;
}

TEST(GLES2FormatTest, VertexAttrib2f) {
  VertexAttrib2f cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLfloat>(12),
      static_cast<GLfloat>(13));
  EXPECT_EQ(static_cast<uint32>(VertexAttrib2f::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.y);
}

TEST(GLES2FormatTest, VertexAttrib2fv) {
  VertexAttrib2fv cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(VertexAttrib2fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<uint32>(12), cmd.values_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.values_shm_offset);
}

TEST(GLES2FormatTest, VertexAttrib2fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
  };
  int8 buf[256] = { 0, };
  VertexAttrib2fvImmediate& cmd =
      *static_cast<VertexAttrib2fvImmediate*>(static_cast<void*>(&buf));
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      data);
  EXPECT_EQ(static_cast<uint32>(VertexAttrib2fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +
                RoundSizeToMultipleOfEntries(sizeof(data)));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  // TODO(gman): Check that data was inserted;
}

TEST(GLES2FormatTest, VertexAttrib3f) {
  VertexAttrib3f cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLfloat>(12),
      static_cast<GLfloat>(13),
      static_cast<GLfloat>(14));
  EXPECT_EQ(static_cast<uint32>(VertexAttrib3f::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.y);
  EXPECT_EQ(static_cast<GLfloat>(14), cmd.z);
}

TEST(GLES2FormatTest, VertexAttrib3fv) {
  VertexAttrib3fv cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(VertexAttrib3fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<uint32>(12), cmd.values_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.values_shm_offset);
}

TEST(GLES2FormatTest, VertexAttrib3fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
  };
  int8 buf[256] = { 0, };
  VertexAttrib3fvImmediate& cmd =
      *static_cast<VertexAttrib3fvImmediate*>(static_cast<void*>(&buf));
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      data);
  EXPECT_EQ(static_cast<uint32>(VertexAttrib3fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +
                RoundSizeToMultipleOfEntries(sizeof(data)));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  // TODO(gman): Check that data was inserted;
}

TEST(GLES2FormatTest, VertexAttrib4f) {
  VertexAttrib4f cmd = { { 0 } };
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
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.x);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.y);
  EXPECT_EQ(static_cast<GLfloat>(14), cmd.z);
  EXPECT_EQ(static_cast<GLfloat>(15), cmd.w);
}

TEST(GLES2FormatTest, VertexAttrib4fv) {
  VertexAttrib4fv cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(VertexAttrib4fv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<uint32>(12), cmd.values_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.values_shm_offset);
}

TEST(GLES2FormatTest, VertexAttrib4fvImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLfloat data[] = {
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 0),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 1),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 2),
    static_cast<GLfloat>(kSomeBaseValueToTestWith + 3),
  };
  int8 buf[256] = { 0, };
  VertexAttrib4fvImmediate& cmd =
      *static_cast<VertexAttrib4fvImmediate*>(static_cast<void*>(&buf));
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      data);
  EXPECT_EQ(static_cast<uint32>(VertexAttrib4fvImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) +
            RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd) +
                RoundSizeToMultipleOfEntries(sizeof(data)));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  // TODO(gman): Check that data was inserted;
}

TEST(GLES2FormatTest, VertexAttribPointer) {
  VertexAttribPointer cmd = { { 0 } };
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
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.indx);
  EXPECT_EQ(static_cast<GLint>(12), cmd.size);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.type);
  EXPECT_EQ(static_cast<GLboolean>(14), cmd.normalized);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.stride);
  EXPECT_EQ(static_cast<GLuint>(16), cmd.offset);
}

TEST(GLES2FormatTest, Viewport) {
  Viewport cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLint>(11),
      static_cast<GLint>(12),
      static_cast<GLsizei>(13),
      static_cast<GLsizei>(14));
  EXPECT_EQ(static_cast<uint32>(Viewport::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLint>(11), cmd.x);
  EXPECT_EQ(static_cast<GLint>(12), cmd.y);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.height);
}

TEST(GLES2FormatTest, BlitFramebufferEXT) {
  BlitFramebufferEXT cmd = { { 0 } };
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
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
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
}

TEST(GLES2FormatTest, RenderbufferStorageMultisampleEXT) {
  RenderbufferStorageMultisampleEXT cmd = { { 0 } };
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
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.samples);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.internalformat);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.height);
}

TEST(GLES2FormatTest, SwapBuffers) {
  SwapBuffers cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd);
  EXPECT_EQ(static_cast<uint32>(SwapBuffers::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
}

TEST(GLES2FormatTest, GetMaxValueInBufferCHROMIUM) {
  GetMaxValueInBufferCHROMIUM cmd = { { 0 } };
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
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.buffer_id);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.count);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.type);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.offset);
  EXPECT_EQ(static_cast<uint32>(15), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(16), cmd.result_shm_offset);
}

TEST(GLES2FormatTest, GenSharedIdsCHROMIUM) {
  GenSharedIdsCHROMIUM cmd = { { 0 } };
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
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.namespace_id);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.id_offset);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.n);
  EXPECT_EQ(static_cast<uint32>(14), cmd.ids_shm_id);
  EXPECT_EQ(static_cast<uint32>(15), cmd.ids_shm_offset);
}

TEST(GLES2FormatTest, DeleteSharedIdsCHROMIUM) {
  DeleteSharedIdsCHROMIUM cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(DeleteSharedIdsCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.namespace_id);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.n);
  EXPECT_EQ(static_cast<uint32>(13), cmd.ids_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.ids_shm_offset);
}

TEST(GLES2FormatTest, RegisterSharedIdsCHROMIUM) {
  RegisterSharedIdsCHROMIUM cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLsizei>(12),
      static_cast<uint32>(13),
      static_cast<uint32>(14));
  EXPECT_EQ(static_cast<uint32>(RegisterSharedIdsCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.namespace_id);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.n);
  EXPECT_EQ(static_cast<uint32>(13), cmd.ids_shm_id);
  EXPECT_EQ(static_cast<uint32>(14), cmd.ids_shm_offset);
}

TEST(GLES2FormatTest, CommandBufferEnableCHROMIUM) {
  CommandBufferEnableCHROMIUM cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<uint32>(12),
      static_cast<uint32>(13));
  EXPECT_EQ(static_cast<uint32>(CommandBufferEnableCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.bucket_id);
  EXPECT_EQ(static_cast<uint32>(12), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32>(13), cmd.result_shm_offset);
}

TEST(GLES2FormatTest, CopyTextureToParentTextureCHROMIUM) {
  CopyTextureToParentTextureCHROMIUM cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32>(CopyTextureToParentTextureCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.client_child_id);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.client_parent_id);
}

TEST(GLES2FormatTest, ResizeCHROMIUM) {
  ResizeCHROMIUM cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<GLuint>(11),
      static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32>(ResizeCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<GLuint>(11), cmd.width);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.height);
}

TEST(GLES2FormatTest, GetRequestableExtensionsCHROMIUM) {
  GetRequestableExtensionsCHROMIUM cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<uint32>(11));
  EXPECT_EQ(static_cast<uint32>(GetRequestableExtensionsCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<uint32>(11), cmd.bucket_id);
}

TEST(GLES2FormatTest, RequestExtensionCHROMIUM) {
  RequestExtensionCHROMIUM cmd = { { 0 } };
  void* next_cmd = cmd.Set(
      &cmd,
      static_cast<uint32>(11));
  EXPECT_EQ(static_cast<uint32>(RequestExtensionCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<char*>(next_cmd),
            reinterpret_cast<char*>(&cmd) + sizeof(cmd));
  EXPECT_EQ(static_cast<uint32>(11), cmd.bucket_id);
}

#endif  // GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_FORMAT_TEST_AUTOGEN_H_

