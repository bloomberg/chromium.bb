// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// DO NOT EDIT!

// This file is included by gles2_implementation.h to declare the
// GL api functions.
#ifndef GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_UNITTEST_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_UNITTEST_AUTOGEN_H_


TEST_F(GLES2ImplementationTest, AttachShader) {
  struct Cmds {
    AttachShader cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2);

  gl_->AttachShader(1, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}
// TODO: Implement unit test for BindAttribLocation

TEST_F(GLES2ImplementationTest, BindBuffer) {
  struct Cmds {
    BindBuffer cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_ARRAY_BUFFER, 2);

  gl_->BindBuffer(GL_ARRAY_BUFFER, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  ClearCommands();
  gl_->BindBuffer(GL_ARRAY_BUFFER, 2);
  EXPECT_TRUE(NoCommandsWritten());
}

TEST_F(GLES2ImplementationTest, BindFramebuffer) {
  struct Cmds {
    BindFramebuffer cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_FRAMEBUFFER, 2);

  gl_->BindFramebuffer(GL_FRAMEBUFFER, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  ClearCommands();
  gl_->BindFramebuffer(GL_FRAMEBUFFER, 2);
  EXPECT_TRUE(NoCommandsWritten());
}

TEST_F(GLES2ImplementationTest, BindRenderbuffer) {
  struct Cmds {
    BindRenderbuffer cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_RENDERBUFFER, 2);

  gl_->BindRenderbuffer(GL_RENDERBUFFER, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  ClearCommands();
  gl_->BindRenderbuffer(GL_RENDERBUFFER, 2);
  EXPECT_TRUE(NoCommandsWritten());
}

TEST_F(GLES2ImplementationTest, BlendColor) {
  struct Cmds {
    BlendColor cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4);

  gl_->BlendColor(1, 2, 3, 4);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, BlendEquation) {
  struct Cmds {
    BlendEquation cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_FUNC_SUBTRACT);

  gl_->BlendEquation(GL_FUNC_SUBTRACT);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, BlendEquationSeparate) {
  struct Cmds {
    BlendEquationSeparate cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_FUNC_SUBTRACT, GL_FUNC_ADD);

  gl_->BlendEquationSeparate(GL_FUNC_SUBTRACT, GL_FUNC_ADD);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, BlendFunc) {
  struct Cmds {
    BlendFunc cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_ZERO, GL_ZERO);

  gl_->BlendFunc(GL_ZERO, GL_ZERO);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, BlendFuncSeparate) {
  struct Cmds {
    BlendFuncSeparate cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_ZERO, GL_ZERO, GL_ZERO, GL_ZERO);

  gl_->BlendFuncSeparate(GL_ZERO, GL_ZERO, GL_ZERO, GL_ZERO);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, CheckFramebufferStatus) {
  struct Cmds {
    CheckFramebufferStatus cmd;
  };

  typedef CheckFramebufferStatus::Result Result;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(CheckFramebufferStatus::Result));
  expected.cmd.Init(1, result1.id, result1.offset);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, uint32(1)))
      .RetiresOnSaturation();

  GLboolean result = gl_->CheckFramebufferStatus(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(result);
}

TEST_F(GLES2ImplementationTest, Clear) {
  struct Cmds {
    Clear cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->Clear(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ClearColor) {
  struct Cmds {
    ClearColor cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4);

  gl_->ClearColor(1, 2, 3, 4);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ClearDepthf) {
  struct Cmds {
    ClearDepthf cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->ClearDepthf(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ClearStencil) {
  struct Cmds {
    ClearStencil cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->ClearStencil(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ColorMask) {
  struct Cmds {
    ColorMask cmd;
  };
  Cmds expected;
  expected.cmd.Init(true, true, true, true);

  gl_->ColorMask(true, true, true, true);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, CompileShader) {
  struct Cmds {
    CompileShader cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->CompileShader(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}
// TODO: Implement unit test for CompressedTexImage2D
// TODO: Implement unit test for CompressedTexSubImage2D

TEST_F(GLES2ImplementationTest, CopyTexImage2D) {
  struct Cmds {
    CopyTexImage2D cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_TEXTURE_2D, 2, GL_ALPHA, 4, 5, 6, 7, 0);

  gl_->CopyTexImage2D(GL_TEXTURE_2D, 2, GL_ALPHA, 4, 5, 6, 7, 0);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, CopyTexSubImage2D) {
  struct Cmds {
    CopyTexSubImage2D cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_TEXTURE_2D, 2, 3, 4, 5, 6, 7, 8);

  gl_->CopyTexSubImage2D(GL_TEXTURE_2D, 2, 3, 4, 5, 6, 7, 8);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, CullFace) {
  struct Cmds {
    CullFace cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_FRONT);

  gl_->CullFace(GL_FRONT);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DeleteBuffers) {
  GLuint ids[2] = { kBuffersStartId, kBuffersStartId + 1 };
  struct Cmds {
    DeleteBuffersImmediate del;
    GLuint data[2];
  };
  Cmds expected;
  expected.del.Init(arraysize(ids), &ids[0]);
  expected.data[0] = kBuffersStartId;
  expected.data[1] = kBuffersStartId + 1;
  gl_->DeleteBuffers(arraysize(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DeleteFramebuffers) {
  GLuint ids[2] = { kFramebuffersStartId, kFramebuffersStartId + 1 };
  struct Cmds {
    DeleteFramebuffersImmediate del;
    GLuint data[2];
  };
  Cmds expected;
  expected.del.Init(arraysize(ids), &ids[0]);
  expected.data[0] = kFramebuffersStartId;
  expected.data[1] = kFramebuffersStartId + 1;
  gl_->DeleteFramebuffers(arraysize(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DeleteProgram) {
  struct Cmds {
    DeleteProgram cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->DeleteProgram(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DeleteRenderbuffers) {
  GLuint ids[2] = { kRenderbuffersStartId, kRenderbuffersStartId + 1 };
  struct Cmds {
    DeleteRenderbuffersImmediate del;
    GLuint data[2];
  };
  Cmds expected;
  expected.del.Init(arraysize(ids), &ids[0]);
  expected.data[0] = kRenderbuffersStartId;
  expected.data[1] = kRenderbuffersStartId + 1;
  gl_->DeleteRenderbuffers(arraysize(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DeleteShader) {
  struct Cmds {
    DeleteShader cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->DeleteShader(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DeleteTextures) {
  GLuint ids[2] = { kTexturesStartId, kTexturesStartId + 1 };
  struct Cmds {
    DeleteTexturesImmediate del;
    GLuint data[2];
  };
  Cmds expected;
  expected.del.Init(arraysize(ids), &ids[0]);
  expected.data[0] = kTexturesStartId;
  expected.data[1] = kTexturesStartId + 1;
  gl_->DeleteTextures(arraysize(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DepthFunc) {
  struct Cmds {
    DepthFunc cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_NEVER);

  gl_->DepthFunc(GL_NEVER);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DepthMask) {
  struct Cmds {
    DepthMask cmd;
  };
  Cmds expected;
  expected.cmd.Init(true);

  gl_->DepthMask(true);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DepthRangef) {
  struct Cmds {
    DepthRangef cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2);

  gl_->DepthRangef(1, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DetachShader) {
  struct Cmds {
    DetachShader cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2);

  gl_->DetachShader(1, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DisableVertexAttribArray) {
  struct Cmds {
    DisableVertexAttribArray cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->DisableVertexAttribArray(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DrawArrays) {
  struct Cmds {
    DrawArrays cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_POINTS, 2, 3);

  gl_->DrawArrays(GL_POINTS, 2, 3);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, EnableVertexAttribArray) {
  struct Cmds {
    EnableVertexAttribArray cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->EnableVertexAttribArray(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Flush) {
  struct Cmds {
    Flush cmd;
  };
  Cmds expected;
  expected.cmd.Init();

  gl_->Flush();
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, FramebufferRenderbuffer) {
  struct Cmds {
    FramebufferRenderbuffer cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 4);

  gl_->FramebufferRenderbuffer(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 4);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, FramebufferTexture2D) {
  struct Cmds {
    FramebufferTexture2D cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 4, 0);

  gl_->FramebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 4, 0);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, FrontFace) {
  struct Cmds {
    FrontFace cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_CW);

  gl_->FrontFace(GL_CW);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, GenBuffers) {
  GLuint ids[2] = { 0, };
  struct Cmds {
    GenBuffersImmediate gen;
    GLuint data[2];
  };
  Cmds expected;
  expected.gen.Init(arraysize(ids), &ids[0]);
  expected.data[0] = kBuffersStartId;
  expected.data[1] = kBuffersStartId + 1;
  gl_->GenBuffers(arraysize(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(kBuffersStartId, ids[0]);
  EXPECT_EQ(kBuffersStartId + 1, ids[1]);
}

TEST_F(GLES2ImplementationTest, GenerateMipmap) {
  struct Cmds {
    GenerateMipmap cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_TEXTURE_2D);

  gl_->GenerateMipmap(GL_TEXTURE_2D);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, GenFramebuffers) {
  GLuint ids[2] = { 0, };
  struct Cmds {
    GenFramebuffersImmediate gen;
    GLuint data[2];
  };
  Cmds expected;
  expected.gen.Init(arraysize(ids), &ids[0]);
  expected.data[0] = kFramebuffersStartId;
  expected.data[1] = kFramebuffersStartId + 1;
  gl_->GenFramebuffers(arraysize(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(kFramebuffersStartId, ids[0]);
  EXPECT_EQ(kFramebuffersStartId + 1, ids[1]);
}

TEST_F(GLES2ImplementationTest, GenRenderbuffers) {
  GLuint ids[2] = { 0, };
  struct Cmds {
    GenRenderbuffersImmediate gen;
    GLuint data[2];
  };
  Cmds expected;
  expected.gen.Init(arraysize(ids), &ids[0]);
  expected.data[0] = kRenderbuffersStartId;
  expected.data[1] = kRenderbuffersStartId + 1;
  gl_->GenRenderbuffers(arraysize(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(kRenderbuffersStartId, ids[0]);
  EXPECT_EQ(kRenderbuffersStartId + 1, ids[1]);
}

TEST_F(GLES2ImplementationTest, GenTextures) {
  GLuint ids[2] = { 0, };
  struct Cmds {
    GenTexturesImmediate gen;
    GLuint data[2];
  };
  Cmds expected;
  expected.gen.Init(arraysize(ids), &ids[0]);
  expected.data[0] = kTexturesStartId;
  expected.data[1] = kTexturesStartId + 1;
  gl_->GenTextures(arraysize(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(kTexturesStartId, ids[0]);
  EXPECT_EQ(kTexturesStartId + 1, ids[1]);
}
// TODO: Implement unit test for GetActiveAttrib
// TODO: Implement unit test for GetActiveUniform
// TODO: Implement unit test for GetAttachedShaders
// TODO: Implement unit test for GetAttribLocation

TEST_F(GLES2ImplementationTest, GetBooleanv) {
  struct Cmds {
    GetBooleanv cmd;
  };
  typedef GetBooleanv::Result Result;
  Result::Type result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 = GetExpectedResultMemory(4);
  expected.cmd.Init(123, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<Result::Type>(1)))
      .RetiresOnSaturation();
  gl_->GetBooleanv(123, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<Result::Type>(1), result);
}

TEST_F(GLES2ImplementationTest, GetBufferParameteriv) {
  struct Cmds {
    GetBufferParameteriv cmd;
  };
  typedef GetBufferParameteriv::Result Result;
  Result::Type result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 = GetExpectedResultMemory(4);
  expected.cmd.Init(123, GL_BUFFER_SIZE, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<Result::Type>(1)))
      .RetiresOnSaturation();
  gl_->GetBufferParameteriv(123, GL_BUFFER_SIZE, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<Result::Type>(1), result);
}

TEST_F(GLES2ImplementationTest, GetFloatv) {
  struct Cmds {
    GetFloatv cmd;
  };
  typedef GetFloatv::Result Result;
  Result::Type result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 = GetExpectedResultMemory(4);
  expected.cmd.Init(123, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<Result::Type>(1)))
      .RetiresOnSaturation();
  gl_->GetFloatv(123, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<Result::Type>(1), result);
}

TEST_F(GLES2ImplementationTest, GetFramebufferAttachmentParameteriv) {
  struct Cmds {
    GetFramebufferAttachmentParameteriv cmd;
  };
  typedef GetFramebufferAttachmentParameteriv::Result Result;
  Result::Type result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 = GetExpectedResultMemory(4);
  expected.cmd.Init(
      123, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
      result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<Result::Type>(1)))
      .RetiresOnSaturation();
  gl_->GetFramebufferAttachmentParameteriv(
      123, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
      &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<Result::Type>(1), result);
}

TEST_F(GLES2ImplementationTest, GetIntegerv) {
  struct Cmds {
    GetIntegerv cmd;
  };
  typedef GetIntegerv::Result Result;
  Result::Type result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 = GetExpectedResultMemory(4);
  expected.cmd.Init(123, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<Result::Type>(1)))
      .RetiresOnSaturation();
  gl_->GetIntegerv(123, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<Result::Type>(1), result);
}

TEST_F(GLES2ImplementationTest, GetProgramiv) {
  struct Cmds {
    GetProgramiv cmd;
  };
  typedef GetProgramiv::Result Result;
  Result::Type result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 = GetExpectedResultMemory(4);
  expected.cmd.Init(123, GL_DELETE_STATUS, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<Result::Type>(1)))
      .RetiresOnSaturation();
  gl_->GetProgramiv(123, GL_DELETE_STATUS, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<Result::Type>(1), result);
}
// TODO: Implement unit test for GetProgramInfoLog

TEST_F(GLES2ImplementationTest, GetRenderbufferParameteriv) {
  struct Cmds {
    GetRenderbufferParameteriv cmd;
  };
  typedef GetRenderbufferParameteriv::Result Result;
  Result::Type result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 = GetExpectedResultMemory(4);
  expected.cmd.Init(123, GL_RENDERBUFFER_RED_SIZE, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<Result::Type>(1)))
      .RetiresOnSaturation();
  gl_->GetRenderbufferParameteriv(123, GL_RENDERBUFFER_RED_SIZE, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<Result::Type>(1), result);
}

TEST_F(GLES2ImplementationTest, GetShaderiv) {
  struct Cmds {
    GetShaderiv cmd;
  };
  typedef GetShaderiv::Result Result;
  Result::Type result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 = GetExpectedResultMemory(4);
  expected.cmd.Init(123, GL_SHADER_TYPE, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<Result::Type>(1)))
      .RetiresOnSaturation();
  gl_->GetShaderiv(123, GL_SHADER_TYPE, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<Result::Type>(1), result);
}
// TODO: Implement unit test for GetShaderInfoLog
// TODO: Implement unit test for GetShaderPrecisionFormat

TEST_F(GLES2ImplementationTest, GetTexParameterfv) {
  struct Cmds {
    GetTexParameterfv cmd;
  };
  typedef GetTexParameterfv::Result Result;
  Result::Type result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 = GetExpectedResultMemory(4);
  expected.cmd.Init(123, GL_TEXTURE_MAG_FILTER, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<Result::Type>(1)))
      .RetiresOnSaturation();
  gl_->GetTexParameterfv(123, GL_TEXTURE_MAG_FILTER, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<Result::Type>(1), result);
}

TEST_F(GLES2ImplementationTest, GetTexParameteriv) {
  struct Cmds {
    GetTexParameteriv cmd;
  };
  typedef GetTexParameteriv::Result Result;
  Result::Type result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 = GetExpectedResultMemory(4);
  expected.cmd.Init(123, GL_TEXTURE_MAG_FILTER, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<Result::Type>(1)))
      .RetiresOnSaturation();
  gl_->GetTexParameteriv(123, GL_TEXTURE_MAG_FILTER, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<Result::Type>(1), result);
}
// TODO: Implement unit test for GetUniformfv
// TODO: Implement unit test for GetUniformiv
// TODO: Implement unit test for GetUniformLocation

TEST_F(GLES2ImplementationTest, GetVertexAttribfv) {
  struct Cmds {
    GetVertexAttribfv cmd;
  };
  typedef GetVertexAttribfv::Result Result;
  Result::Type result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 = GetExpectedResultMemory(4);
  expected.cmd.Init(
      123, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<Result::Type>(1)))
      .RetiresOnSaturation();
  gl_->GetVertexAttribfv(123, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<Result::Type>(1), result);
}

TEST_F(GLES2ImplementationTest, GetVertexAttribiv) {
  struct Cmds {
    GetVertexAttribiv cmd;
  };
  typedef GetVertexAttribiv::Result Result;
  Result::Type result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 = GetExpectedResultMemory(4);
  expected.cmd.Init(
      123, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<Result::Type>(1)))
      .RetiresOnSaturation();
  gl_->GetVertexAttribiv(123, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<Result::Type>(1), result);
}

TEST_F(GLES2ImplementationTest, Hint) {
  struct Cmds {
    Hint cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_GENERATE_MIPMAP_HINT, GL_FASTEST);

  gl_->Hint(GL_GENERATE_MIPMAP_HINT, GL_FASTEST);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, IsBuffer) {
  struct Cmds {
    IsBuffer cmd;
  };

  typedef IsBuffer::Result Result;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(IsBuffer::Result));
  expected.cmd.Init(1, result1.id, result1.offset);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, uint32(1)))
      .RetiresOnSaturation();

  GLboolean result = gl_->IsBuffer(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(result);
}

TEST_F(GLES2ImplementationTest, IsEnabled) {
  struct Cmds {
    IsEnabled cmd;
  };

  typedef IsEnabled::Result Result;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(IsEnabled::Result));
  expected.cmd.Init(1, result1.id, result1.offset);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, uint32(1)))
      .RetiresOnSaturation();

  GLboolean result = gl_->IsEnabled(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(result);
}

TEST_F(GLES2ImplementationTest, IsFramebuffer) {
  struct Cmds {
    IsFramebuffer cmd;
  };

  typedef IsFramebuffer::Result Result;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(IsFramebuffer::Result));
  expected.cmd.Init(1, result1.id, result1.offset);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, uint32(1)))
      .RetiresOnSaturation();

  GLboolean result = gl_->IsFramebuffer(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(result);
}

TEST_F(GLES2ImplementationTest, IsProgram) {
  struct Cmds {
    IsProgram cmd;
  };

  typedef IsProgram::Result Result;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(IsProgram::Result));
  expected.cmd.Init(1, result1.id, result1.offset);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, uint32(1)))
      .RetiresOnSaturation();

  GLboolean result = gl_->IsProgram(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(result);
}

TEST_F(GLES2ImplementationTest, IsRenderbuffer) {
  struct Cmds {
    IsRenderbuffer cmd;
  };

  typedef IsRenderbuffer::Result Result;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(IsRenderbuffer::Result));
  expected.cmd.Init(1, result1.id, result1.offset);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, uint32(1)))
      .RetiresOnSaturation();

  GLboolean result = gl_->IsRenderbuffer(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(result);
}

TEST_F(GLES2ImplementationTest, IsShader) {
  struct Cmds {
    IsShader cmd;
  };

  typedef IsShader::Result Result;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(IsShader::Result));
  expected.cmd.Init(1, result1.id, result1.offset);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, uint32(1)))
      .RetiresOnSaturation();

  GLboolean result = gl_->IsShader(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(result);
}

TEST_F(GLES2ImplementationTest, IsTexture) {
  struct Cmds {
    IsTexture cmd;
  };

  typedef IsTexture::Result Result;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(IsTexture::Result));
  expected.cmd.Init(1, result1.id, result1.offset);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, uint32(1)))
      .RetiresOnSaturation();

  GLboolean result = gl_->IsTexture(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(result);
}

TEST_F(GLES2ImplementationTest, LineWidth) {
  struct Cmds {
    LineWidth cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->LineWidth(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, LinkProgram) {
  struct Cmds {
    LinkProgram cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->LinkProgram(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, PixelStorei) {
  struct Cmds {
    PixelStorei cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_PACK_ALIGNMENT, 1);

  gl_->PixelStorei(GL_PACK_ALIGNMENT, 1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, PolygonOffset) {
  struct Cmds {
    PolygonOffset cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2);

  gl_->PolygonOffset(1, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ReleaseShaderCompiler) {
  struct Cmds {
    ReleaseShaderCompiler cmd;
  };
  Cmds expected;
  expected.cmd.Init();

  gl_->ReleaseShaderCompiler();
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, RenderbufferStorage) {
  struct Cmds {
    RenderbufferStorage cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_RENDERBUFFER, GL_RGBA4, 3, 4);

  gl_->RenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, 3, 4);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, SampleCoverage) {
  struct Cmds {
    SampleCoverage cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, true);

  gl_->SampleCoverage(1, true);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Scissor) {
  struct Cmds {
    Scissor cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4);

  gl_->Scissor(1, 2, 3, 4);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, StencilFunc) {
  struct Cmds {
    StencilFunc cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_NEVER, 2, 3);

  gl_->StencilFunc(GL_NEVER, 2, 3);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, StencilFuncSeparate) {
  struct Cmds {
    StencilFuncSeparate cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_FRONT, GL_NEVER, 3, 4);

  gl_->StencilFuncSeparate(GL_FRONT, GL_NEVER, 3, 4);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, StencilMask) {
  struct Cmds {
    StencilMask cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->StencilMask(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, StencilMaskSeparate) {
  struct Cmds {
    StencilMaskSeparate cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_FRONT, 2);

  gl_->StencilMaskSeparate(GL_FRONT, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, StencilOp) {
  struct Cmds {
    StencilOp cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_KEEP, GL_INCR, GL_KEEP);

  gl_->StencilOp(GL_KEEP, GL_INCR, GL_KEEP);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, StencilOpSeparate) {
  struct Cmds {
    StencilOpSeparate cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_FRONT, GL_INCR, GL_KEEP, GL_KEEP);

  gl_->StencilOpSeparate(GL_FRONT, GL_INCR, GL_KEEP, GL_KEEP);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, TexParameterf) {
  struct Cmds {
    TexParameterf cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 3);

  gl_->TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 3);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, TexParameterfv) {
  struct Cmds {
    TexParameterfvImmediate cmd;
    GLfloat data[1];
  };

  Cmds expected;
  for (int jj = 0; jj < 1; ++jj) {
    expected.data[jj] = static_cast<GLfloat>(jj);
  }
  expected.cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &expected.data[0]);
  gl_->TexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &expected.data[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, TexParameteri) {
  struct Cmds {
    TexParameteri cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 3);

  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 3);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, TexParameteriv) {
  struct Cmds {
    TexParameterivImmediate cmd;
    GLint data[1];
  };

  Cmds expected;
  for (int jj = 0; jj < 1; ++jj) {
    expected.data[jj] = static_cast<GLint>(jj);
  }
  expected.cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &expected.data[0]);
  gl_->TexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &expected.data[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform1f) {
  struct Cmds {
    Uniform1f cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2);

  gl_->Uniform1f(1, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform1fv) {
  struct Cmds {
    Uniform1fvImmediate cmd;
    GLfloat data[2][1];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 1; ++jj) {
      expected.data[ii][jj] = static_cast<GLfloat>(ii * 1 + jj);
    }
  }
  expected.cmd.Init(1, 2, &expected.data[0][0]);
  gl_->Uniform1fv(1, 2, &expected.data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform1i) {
  struct Cmds {
    Uniform1i cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2);

  gl_->Uniform1i(1, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform1iv) {
  struct Cmds {
    Uniform1ivImmediate cmd;
    GLint data[2][1];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 1; ++jj) {
      expected.data[ii][jj] = static_cast<GLint>(ii * 1 + jj);
    }
  }
  expected.cmd.Init(1, 2, &expected.data[0][0]);
  gl_->Uniform1iv(1, 2, &expected.data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform2f) {
  struct Cmds {
    Uniform2f cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3);

  gl_->Uniform2f(1, 2, 3);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform2fv) {
  struct Cmds {
    Uniform2fvImmediate cmd;
    GLfloat data[2][2];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 2; ++jj) {
      expected.data[ii][jj] = static_cast<GLfloat>(ii * 2 + jj);
    }
  }
  expected.cmd.Init(1, 2, &expected.data[0][0]);
  gl_->Uniform2fv(1, 2, &expected.data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform2i) {
  struct Cmds {
    Uniform2i cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3);

  gl_->Uniform2i(1, 2, 3);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform2iv) {
  struct Cmds {
    Uniform2ivImmediate cmd;
    GLint data[2][2];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 2; ++jj) {
      expected.data[ii][jj] = static_cast<GLint>(ii * 2 + jj);
    }
  }
  expected.cmd.Init(1, 2, &expected.data[0][0]);
  gl_->Uniform2iv(1, 2, &expected.data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform3f) {
  struct Cmds {
    Uniform3f cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4);

  gl_->Uniform3f(1, 2, 3, 4);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform3fv) {
  struct Cmds {
    Uniform3fvImmediate cmd;
    GLfloat data[2][3];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 3; ++jj) {
      expected.data[ii][jj] = static_cast<GLfloat>(ii * 3 + jj);
    }
  }
  expected.cmd.Init(1, 2, &expected.data[0][0]);
  gl_->Uniform3fv(1, 2, &expected.data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform3i) {
  struct Cmds {
    Uniform3i cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4);

  gl_->Uniform3i(1, 2, 3, 4);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform3iv) {
  struct Cmds {
    Uniform3ivImmediate cmd;
    GLint data[2][3];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 3; ++jj) {
      expected.data[ii][jj] = static_cast<GLint>(ii * 3 + jj);
    }
  }
  expected.cmd.Init(1, 2, &expected.data[0][0]);
  gl_->Uniform3iv(1, 2, &expected.data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform4f) {
  struct Cmds {
    Uniform4f cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4, 5);

  gl_->Uniform4f(1, 2, 3, 4, 5);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform4fv) {
  struct Cmds {
    Uniform4fvImmediate cmd;
    GLfloat data[2][4];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 4; ++jj) {
      expected.data[ii][jj] = static_cast<GLfloat>(ii * 4 + jj);
    }
  }
  expected.cmd.Init(1, 2, &expected.data[0][0]);
  gl_->Uniform4fv(1, 2, &expected.data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform4i) {
  struct Cmds {
    Uniform4i cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4, 5);

  gl_->Uniform4i(1, 2, 3, 4, 5);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Uniform4iv) {
  struct Cmds {
    Uniform4ivImmediate cmd;
    GLint data[2][4];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 4; ++jj) {
      expected.data[ii][jj] = static_cast<GLint>(ii * 4 + jj);
    }
  }
  expected.cmd.Init(1, 2, &expected.data[0][0]);
  gl_->Uniform4iv(1, 2, &expected.data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, UniformMatrix2fv) {
  struct Cmds {
    UniformMatrix2fvImmediate cmd;
    GLfloat data[2][4];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 4; ++jj) {
      expected.data[ii][jj] = static_cast<GLfloat>(ii * 4 + jj);
    }
  }
  expected.cmd.Init(1, 2, false, &expected.data[0][0]);
  gl_->UniformMatrix2fv(1, 2, false, &expected.data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, UniformMatrix3fv) {
  struct Cmds {
    UniformMatrix3fvImmediate cmd;
    GLfloat data[2][9];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 9; ++jj) {
      expected.data[ii][jj] = static_cast<GLfloat>(ii * 9 + jj);
    }
  }
  expected.cmd.Init(1, 2, false, &expected.data[0][0]);
  gl_->UniformMatrix3fv(1, 2, false, &expected.data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, UniformMatrix4fv) {
  struct Cmds {
    UniformMatrix4fvImmediate cmd;
    GLfloat data[2][16];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 16; ++jj) {
      expected.data[ii][jj] = static_cast<GLfloat>(ii * 16 + jj);
    }
  }
  expected.cmd.Init(1, 2, false, &expected.data[0][0]);
  gl_->UniformMatrix4fv(1, 2, false, &expected.data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, UseProgram) {
  struct Cmds {
    UseProgram cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->UseProgram(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ValidateProgram) {
  struct Cmds {
    ValidateProgram cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->ValidateProgram(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, VertexAttrib1f) {
  struct Cmds {
    VertexAttrib1f cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2);

  gl_->VertexAttrib1f(1, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, VertexAttrib1fv) {
  struct Cmds {
    VertexAttrib1fvImmediate cmd;
    GLfloat data[1];
  };

  Cmds expected;
  for (int jj = 0; jj < 1; ++jj) {
    expected.data[jj] = static_cast<GLfloat>(jj);
  }
  expected.cmd.Init(1, &expected.data[0]);
  gl_->VertexAttrib1fv(1, &expected.data[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, VertexAttrib2f) {
  struct Cmds {
    VertexAttrib2f cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3);

  gl_->VertexAttrib2f(1, 2, 3);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, VertexAttrib2fv) {
  struct Cmds {
    VertexAttrib2fvImmediate cmd;
    GLfloat data[2];
  };

  Cmds expected;
  for (int jj = 0; jj < 2; ++jj) {
    expected.data[jj] = static_cast<GLfloat>(jj);
  }
  expected.cmd.Init(1, &expected.data[0]);
  gl_->VertexAttrib2fv(1, &expected.data[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, VertexAttrib3f) {
  struct Cmds {
    VertexAttrib3f cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4);

  gl_->VertexAttrib3f(1, 2, 3, 4);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, VertexAttrib3fv) {
  struct Cmds {
    VertexAttrib3fvImmediate cmd;
    GLfloat data[3];
  };

  Cmds expected;
  for (int jj = 0; jj < 3; ++jj) {
    expected.data[jj] = static_cast<GLfloat>(jj);
  }
  expected.cmd.Init(1, &expected.data[0]);
  gl_->VertexAttrib3fv(1, &expected.data[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, VertexAttrib4f) {
  struct Cmds {
    VertexAttrib4f cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4, 5);

  gl_->VertexAttrib4f(1, 2, 3, 4, 5);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, VertexAttrib4fv) {
  struct Cmds {
    VertexAttrib4fvImmediate cmd;
    GLfloat data[4];
  };

  Cmds expected;
  for (int jj = 0; jj < 4; ++jj) {
    expected.data[jj] = static_cast<GLfloat>(jj);
  }
  expected.cmd.Init(1, &expected.data[0]);
  gl_->VertexAttrib4fv(1, &expected.data[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, Viewport) {
  struct Cmds {
    Viewport cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4);

  gl_->Viewport(1, 2, 3, 4);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, BlitFramebufferEXT) {
  struct Cmds {
    BlitFramebufferEXT cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4, 5, 6, 7, 8, 9, GL_NEAREST);

  gl_->BlitFramebufferEXT(1, 2, 3, 4, 5, 6, 7, 8, 9, GL_NEAREST);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, RenderbufferStorageMultisampleEXT) {
  struct Cmds {
    RenderbufferStorageMultisampleEXT cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_RENDERBUFFER, 2, GL_RGBA4, 4, 5);

  gl_->RenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, 2, GL_RGBA4, 4, 5);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, TexStorage2DEXT) {
  struct Cmds {
    TexStorage2DEXT cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_TEXTURE_2D, 2, GL_RGB565, 4, 5);

  gl_->TexStorage2DEXT(GL_TEXTURE_2D, 2, GL_RGB565, 4, 5);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, GenQueriesEXT) {
  GLuint ids[2] = { 0, };
  struct Cmds {
    GenQueriesEXTImmediate gen;
    GLuint data[2];
  };
  Cmds expected;
  expected.gen.Init(arraysize(ids), &ids[0]);
  expected.data[0] = kQueriesStartId;
  expected.data[1] = kQueriesStartId + 1;
  gl_->GenQueriesEXT(arraysize(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(kQueriesStartId, ids[0]);
  EXPECT_EQ(kQueriesStartId + 1, ids[1]);
}

TEST_F(GLES2ImplementationTest, DeleteQueriesEXT) {
  GLuint ids[2] = { kQueriesStartId, kQueriesStartId + 1 };
  struct Cmds {
    DeleteQueriesEXTImmediate del;
    GLuint data[2];
  };
  Cmds expected;
  expected.del.Init(arraysize(ids), &ids[0]);
  expected.data[0] = kQueriesStartId;
  expected.data[1] = kQueriesStartId + 1;
  gl_->DeleteQueriesEXT(arraysize(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}
// TODO: Implement unit test for BeginQueryEXT
// TODO: Implement unit test for InsertEventMarkerEXT
// TODO: Implement unit test for PushGroupMarkerEXT

TEST_F(GLES2ImplementationTest, PopGroupMarkerEXT) {
  struct Cmds {
    PopGroupMarkerEXT cmd;
  };
  Cmds expected;
  expected.cmd.Init();

  gl_->PopGroupMarkerEXT();
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, GenVertexArraysOES) {
  GLuint ids[2] = { 0, };
  struct Cmds {
    GenVertexArraysOESImmediate gen;
    GLuint data[2];
  };
  Cmds expected;
  expected.gen.Init(arraysize(ids), &ids[0]);
  expected.data[0] = kVertexArraysStartId;
  expected.data[1] = kVertexArraysStartId + 1;
  gl_->GenVertexArraysOES(arraysize(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(kVertexArraysStartId, ids[0]);
  EXPECT_EQ(kVertexArraysStartId + 1, ids[1]);
}

TEST_F(GLES2ImplementationTest, DeleteVertexArraysOES) {
  GLuint ids[2] = { kVertexArraysStartId, kVertexArraysStartId + 1 };
  struct Cmds {
    DeleteVertexArraysOESImmediate del;
    GLuint data[2];
  };
  Cmds expected;
  expected.del.Init(arraysize(ids), &ids[0]);
  expected.data[0] = kVertexArraysStartId;
  expected.data[1] = kVertexArraysStartId + 1;
  gl_->DeleteVertexArraysOES(arraysize(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, IsVertexArrayOES) {
  struct Cmds {
    IsVertexArrayOES cmd;
  };

  typedef IsVertexArrayOES::Result Result;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(IsVertexArrayOES::Result));
  expected.cmd.Init(1, result1.id, result1.offset);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, uint32(1)))
      .RetiresOnSaturation();

  GLboolean result = gl_->IsVertexArrayOES(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(result);
}
// TODO: Implement unit test for GenSharedIdsCHROMIUM
// TODO: Implement unit test for DeleteSharedIdsCHROMIUM
// TODO: Implement unit test for RegisterSharedIdsCHROMIUM
// TODO: Implement unit test for EnableFeatureCHROMIUM

TEST_F(GLES2ImplementationTest, ResizeCHROMIUM) {
  struct Cmds {
    ResizeCHROMIUM cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2);

  gl_->ResizeCHROMIUM(1, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}
// TODO: Implement unit test for GetRequestableExtensionsCHROMIUM

TEST_F(GLES2ImplementationTest, DestroyStreamTextureCHROMIUM) {
  struct Cmds {
    DestroyStreamTextureCHROMIUM cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->DestroyStreamTextureCHROMIUM(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}
// TODO: Implement unit test for GetTranslatedShaderSourceANGLE

TEST_F(GLES2ImplementationTest, TexImageIOSurface2DCHROMIUM) {
  struct Cmds {
    TexImageIOSurface2DCHROMIUM cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_TEXTURE_2D, 2, 3, 4, 5);

  gl_->TexImageIOSurface2DCHROMIUM(GL_TEXTURE_2D, 2, 3, 4, 5);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, CopyTextureCHROMIUM) {
  struct Cmds {
    CopyTextureCHROMIUM cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4, GL_ALPHA);

  gl_->CopyTextureCHROMIUM(1, 2, 3, 4, GL_ALPHA);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DrawArraysInstancedANGLE) {
  struct Cmds {
    DrawArraysInstancedANGLE cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_POINTS, 2, 3, 4);

  gl_->DrawArraysInstancedANGLE(GL_POINTS, 2, 3, 4);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, VertexAttribDivisorANGLE) {
  struct Cmds {
    VertexAttribDivisorANGLE cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2);

  gl_->VertexAttribDivisorANGLE(1, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ProduceTextureCHROMIUM) {
  struct Cmds {
    ProduceTextureCHROMIUMImmediate cmd;
    GLbyte data[64];
  };

  Cmds expected;
  for (int jj = 0; jj < 64; ++jj) {
    expected.data[jj] = static_cast<GLbyte>(jj);
  }
  expected.cmd.Init(GL_TEXTURE_2D, &expected.data[0]);
  gl_->ProduceTextureCHROMIUM(GL_TEXTURE_2D, &expected.data[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ConsumeTextureCHROMIUM) {
  struct Cmds {
    ConsumeTextureCHROMIUMImmediate cmd;
    GLbyte data[64];
  };

  Cmds expected;
  for (int jj = 0; jj < 64; ++jj) {
    expected.data[jj] = static_cast<GLbyte>(jj);
  }
  expected.cmd.Init(GL_TEXTURE_2D, &expected.data[0]);
  gl_->ConsumeTextureCHROMIUM(GL_TEXTURE_2D, &expected.data[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}
// TODO: Implement unit test for BindUniformLocationCHROMIUM

TEST_F(GLES2ImplementationTest, BindTexImage2DCHROMIUM) {
  struct Cmds {
    BindTexImage2DCHROMIUM cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_TEXTURE_2D, 2);

  gl_->BindTexImage2DCHROMIUM(GL_TEXTURE_2D, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ReleaseTexImage2DCHROMIUM) {
  struct Cmds {
    ReleaseTexImage2DCHROMIUM cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_TEXTURE_2D, 2);

  gl_->ReleaseTexImage2DCHROMIUM(GL_TEXTURE_2D, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DiscardFramebufferEXT) {
  struct Cmds {
    DiscardFramebufferEXTImmediate cmd;
    GLenum data[2][1];
  };

  Cmds expected;
  for (int ii = 0; ii < 2; ++ii) {
    for (int jj = 0; jj < 1; ++jj) {
      expected.data[ii][jj] = static_cast<GLenum>(ii * 1 + jj);
    }
  }
  expected.cmd.Init(1, 2, &expected.data[0][0]);
  gl_->DiscardFramebufferEXT(1, 2, &expected.data[0][0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, LoseContextCHROMIUM) {
  struct Cmds {
    LoseContextCHROMIUM cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2);

  gl_->LoseContextCHROMIUM(1, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, WaitSyncPointCHROMIUM) {
  struct Cmds {
    WaitSyncPointCHROMIUM cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->WaitSyncPointCHROMIUM(1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}
#endif  // GPU_COMMAND_BUFFER_CLIENT_GLES2_IMPLEMENTATION_UNITTEST_AUTOGEN_H_

