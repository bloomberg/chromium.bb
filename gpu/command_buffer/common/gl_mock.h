// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements mock GL Interface for unit testing. It has to mock
// Desktop GL, not GLES2 as it is used to test the service side code.

#ifndef GPU_COMMAND_BUFFER_COMMON_GL_MOCK_H_
#define GPU_COMMAND_BUFFER_COMMON_GL_MOCK_H_
#pragma once

#include "app/gfx/gl/gl_interface.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace gfx {

class MockGLInterface : public GLInterface {
 public:
  MockGLInterface();
  virtual ~MockGLInterface();

  MOCK_METHOD1(ActiveTexture, void(GLenum texture));

  MOCK_METHOD2(AttachShader, void(GLuint program, GLuint shader));

  MOCK_METHOD3(BindAttribLocation, void(
      GLuint program, GLuint index, const char* name));

  MOCK_METHOD2(BindBuffer, void(GLenum target, GLuint buffer));

  MOCK_METHOD2(BindFramebufferEXT, void(GLenum target, GLuint framebuffer));

  MOCK_METHOD2(BindRenderbufferEXT, void(GLenum target, GLuint renderbuffer));

  MOCK_METHOD2(BindTexture, void(GLenum target, GLuint texture));

  MOCK_METHOD4(BlendColor, void(
      GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha));

  MOCK_METHOD1(BlendEquation, void(GLenum mode));

  MOCK_METHOD2(BlendEquationSeparate, void(GLenum modeRGB, GLenum modeAlpha));

  MOCK_METHOD2(BlendFunc, void(GLenum sfactor, GLenum dfactor));

  MOCK_METHOD4(BlendFuncSeparate, void(
      GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha));

  MOCK_METHOD10(BlitFramebufferANGLE, void(
      GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
      GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
      GLbitfield mask, GLenum filter));

  MOCK_METHOD10(BlitFramebufferEXT, void(
      GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
      GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
      GLbitfield mask, GLenum filter));

  MOCK_METHOD4(BufferData, void(
      GLenum target, GLsizeiptr size, const void* data, GLenum usage));

  MOCK_METHOD4(BufferSubData, void(
      GLenum target, GLintptr offset, GLsizeiptr size, const void* data));

  MOCK_METHOD1(CheckFramebufferStatusEXT, GLenum(GLenum target));

  MOCK_METHOD1(Clear, void(GLbitfield mask));

  MOCK_METHOD4(ClearColor, void(
      GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha));

  MOCK_METHOD1(ClearDepth, void(GLclampd depth));

  MOCK_METHOD1(ClearDepthf, void(GLclampf depth));

  MOCK_METHOD1(ClearStencil, void(GLint s));

  MOCK_METHOD4(ColorMask, void(
      GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha));

  MOCK_METHOD1(CompileShader, void(GLuint shader));

  MOCK_METHOD8(CompressedTexImage2D, void(
      GLenum target, GLint level, GLenum internalformat, GLsizei width,
      GLsizei height, GLint border, GLsizei imageSize, const void* data));

  MOCK_METHOD9(CompressedTexSubImage2D, void(
      GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
      GLsizei height, GLenum format, GLsizei imageSize, const void* data));

  MOCK_METHOD8(CopyTexImage2D, void(
      GLenum target, GLint level, GLenum internalformat, GLint x, GLint y,
      GLsizei width, GLsizei height, GLint border));

  MOCK_METHOD8(CopyTexSubImage2D, void(
      GLenum target, GLint level, GLint xoffset, GLint yoffset,
      GLint x, GLint y, GLsizei width, GLsizei height));

  MOCK_METHOD0(CreateProgram, GLuint());

  MOCK_METHOD1(CreateShader, GLuint(GLenum type));

  MOCK_METHOD1(CullFace, void(GLenum mode));

  MOCK_METHOD2(DeleteBuffersARB, void(GLsizei n, const GLuint* buffers));

  MOCK_METHOD2(DeleteFramebuffersEXT,
               void(GLsizei n, const GLuint* framebuffers));

  MOCK_METHOD1(DeleteProgram, void(GLuint program));

  MOCK_METHOD2(DeleteRenderbuffersEXT, void(
      GLsizei n, const GLuint* renderbuffers));

  MOCK_METHOD1(DeleteShader, void(GLuint shader));

  MOCK_METHOD2(DeleteTextures, void(GLsizei n, const GLuint* textures));

  MOCK_METHOD1(DepthFunc, void(GLenum func));

  MOCK_METHOD1(DepthMask, void(GLboolean flag));

  MOCK_METHOD2(DepthRange, void(GLclampd zNear, GLclampd zFar));

  MOCK_METHOD2(DepthRangef, void(GLclampf zNear, GLclampf zFar));

  MOCK_METHOD2(DetachShader, void(GLuint program, GLuint shader));

  MOCK_METHOD1(Disable, void(GLenum cap));

  MOCK_METHOD1(DisableVertexAttribArray, void(GLuint index));

  MOCK_METHOD3(DrawArrays, void(GLenum mode, GLint first, GLsizei count));

  MOCK_METHOD4(DrawElements, void(
      GLenum mode, GLsizei count, GLenum type, const void* indices));

  MOCK_METHOD1(Enable, void(GLenum cap));

  MOCK_METHOD1(EnableVertexAttribArray, void(GLuint index));

  MOCK_METHOD0(Finish, void());

  MOCK_METHOD0(Flush, void());

  MOCK_METHOD4(FramebufferRenderbufferEXT, void(
      GLenum target, GLenum attachment, GLenum renderbuffertarget,
      GLuint renderbuffer));

  MOCK_METHOD5(FramebufferTexture2DEXT, void(
      GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
      GLint level));

  MOCK_METHOD1(FrontFace, void(GLenum mode));

  MOCK_METHOD2(GenBuffersARB, void(GLsizei n, GLuint* buffers));

  MOCK_METHOD1(GenerateMipmapEXT, void(GLenum target));

  MOCK_METHOD2(GenFramebuffersEXT, void(GLsizei n, GLuint* framebuffers));

  MOCK_METHOD2(GenRenderbuffersEXT, void(GLsizei n, GLuint* renderbuffers));

  MOCK_METHOD2(GenTextures, void(GLsizei n, GLuint* textures));

  MOCK_METHOD7(GetActiveAttrib, void(
      GLuint program, GLuint index, GLsizei bufsize, GLsizei* length,
      GLint* size, GLenum* type, char* name));

  MOCK_METHOD7(GetActiveUniform, void(
      GLuint program, GLuint index, GLsizei bufsize, GLsizei* length,
      GLint* size, GLenum* type, char* name));

  MOCK_METHOD4(GetAttachedShaders, void(
      GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders));

  MOCK_METHOD2(GetAttribLocation, GLint(GLuint program, const char* name));

  MOCK_METHOD2(GetBooleanv, void(GLenum pname, GLboolean* params));

  MOCK_METHOD3(GetBufferParameteriv, void(
      GLenum target, GLenum pname, GLint* params));

  MOCK_METHOD0(GetError, GLenum());

  MOCK_METHOD2(GetFloatv, void(GLenum pname, GLfloat* params));

  MOCK_METHOD4(GetFramebufferAttachmentParameterivEXT, void(
      GLenum target, GLenum attachment, GLenum pname, GLint* params));

  MOCK_METHOD2(GetIntegerv, void(GLenum pname, GLint* params));

  MOCK_METHOD3(GetProgramiv, void(GLuint program, GLenum pname, GLint* params));

  MOCK_METHOD4(GetProgramInfoLog, void(
      GLuint program, GLsizei bufsize, GLsizei* length, char* infolog));

  MOCK_METHOD3(GetRenderbufferParameterivEXT, void(
      GLenum target, GLenum pname, GLint* params));

  MOCK_METHOD3(GetShaderiv, void(GLuint shader, GLenum pname, GLint* params));

  MOCK_METHOD4(GetShaderInfoLog, void(
      GLuint shader, GLsizei bufsize, GLsizei* length, char* infolog));

  MOCK_METHOD4(GetShaderPrecisionFormat, void(
      GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision));

  MOCK_METHOD4(GetShaderSource, void(
      GLuint shader, GLsizei bufsize, GLsizei* length, char* source));

  MOCK_METHOD1(GetString, const GLubyte*(GLenum name));

  MOCK_METHOD4(GetTexLevelParameterfv, void(
      GLenum target, GLint level, GLenum pname, GLfloat* params));

  MOCK_METHOD4(GetTexLevelParameteriv, void(
      GLenum target, GLint level, GLenum pname, GLint* params));

  MOCK_METHOD3(GetTexParameterfv, void(
      GLenum target, GLenum pname, GLfloat* params));

  MOCK_METHOD3(GetTexParameteriv, void(
      GLenum target, GLenum pname, GLint* params));

  MOCK_METHOD3(GetUniformfv,
               void(GLuint program, GLint location, GLfloat* params));

  MOCK_METHOD3(GetUniformiv,
               void(GLuint program, GLint location, GLint* params));

  MOCK_METHOD2(GetUniformLocation, GLint(GLuint program, const char* name));

  MOCK_METHOD3(GetVertexAttribfv, void(
      GLuint index, GLenum pname, GLfloat* params));

  MOCK_METHOD3(GetVertexAttribiv,
               void(GLuint index, GLenum pname, GLint* params));

  MOCK_METHOD3(GetVertexAttribPointerv, void(
      GLuint index, GLenum pname, void** pointer));

  MOCK_METHOD2(Hint, void(GLenum target, GLenum mode));

  MOCK_METHOD1(IsBuffer, GLboolean(GLuint buffer));

  MOCK_METHOD1(IsEnabled, GLboolean(GLenum cap));

  MOCK_METHOD1(IsFramebufferEXT, GLboolean(GLuint framebuffer));

  MOCK_METHOD1(IsProgram, GLboolean(GLuint program));

  MOCK_METHOD1(IsRenderbufferEXT, GLboolean(GLuint renderbuffer));

  MOCK_METHOD1(IsShader, GLboolean(GLuint shader));

  MOCK_METHOD1(IsTexture, GLboolean(GLuint texture));

  MOCK_METHOD1(LineWidth, void(GLfloat width));

  MOCK_METHOD1(LinkProgram, void(GLuint program));

  MOCK_METHOD2(PixelStorei, void(GLenum pname, GLint param));

  MOCK_METHOD2(PolygonOffset, void(GLfloat factor, GLfloat units));

  MOCK_METHOD7(ReadPixels, void(
      GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
      GLenum type, void* pixels));

  MOCK_METHOD0(ReleaseShaderCompiler, void());

  MOCK_METHOD4(RenderbufferStorageEXT, void(
      GLenum target, GLenum internalformat, GLsizei width, GLsizei height));

  MOCK_METHOD5(RenderbufferStorageMultisampleANGLE, void(
      GLenum target, GLsizei samples, GLenum internalformat,
      GLsizei width, GLsizei height));

  MOCK_METHOD5(RenderbufferStorageMultisampleEXT, void(
      GLenum target, GLsizei samples, GLenum internalformat,
      GLsizei width, GLsizei height));

  MOCK_METHOD2(SampleCoverage, void(GLclampf value, GLboolean invert));

  MOCK_METHOD4(Scissor, void(GLint x, GLint y, GLsizei width, GLsizei height));

  MOCK_METHOD5(ShaderBinary, void(
      GLsizei n, const GLuint* shaders, GLenum binaryformat,
      const void* binary, GLsizei length));

  MOCK_METHOD4(ShaderSource, void(
      GLuint shader, GLsizei count, const char** str, const GLint* length));

  MOCK_METHOD3(StencilFunc, void(GLenum func, GLint ref, GLuint mask));

  MOCK_METHOD4(StencilFuncSeparate, void(
      GLenum face, GLenum func, GLint ref, GLuint mask));

  MOCK_METHOD1(StencilMask, void(GLuint mask));

  MOCK_METHOD2(StencilMaskSeparate, void(GLenum face, GLuint mask));

  MOCK_METHOD3(StencilOp, void(GLenum fail, GLenum zfail, GLenum zpass));

  MOCK_METHOD4(StencilOpSeparate, void(
      GLenum face, GLenum fail, GLenum zfail, GLenum zpass));

  MOCK_METHOD9(TexImage2D, void(
      GLenum target, GLint level, GLint internalformat, GLsizei width,
      GLsizei height, GLint border, GLenum format, GLenum type,
      const void* pixels));

  MOCK_METHOD3(TexParameterf, void(GLenum target, GLenum pname, GLfloat param));

  MOCK_METHOD3(TexParameterfv, void(
      GLenum target, GLenum pname, const GLfloat* params));

  MOCK_METHOD3(TexParameteri, void(GLenum target, GLenum pname, GLint param));

  MOCK_METHOD3(TexParameteriv, void(
      GLenum target, GLenum pname, const GLint* params));

  MOCK_METHOD9(TexSubImage2D, void(
      GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
      GLsizei height, GLenum format, GLenum type, const void* pixels));

  MOCK_METHOD2(Uniform1f, void(GLint location, GLfloat x));

  MOCK_METHOD3(Uniform1fv,
               void(GLint location, GLsizei count, const GLfloat* v));

  MOCK_METHOD2(Uniform1i, void(GLint location, GLint x));

  MOCK_METHOD3(Uniform1iv, void(GLint location, GLsizei count, const GLint* v));

  MOCK_METHOD3(Uniform2f, void(GLint location, GLfloat x, GLfloat y));

  MOCK_METHOD3(Uniform2fv,
               void(GLint location, GLsizei count, const GLfloat* v));

  MOCK_METHOD3(Uniform2i, void(GLint location, GLint x, GLint y));

  MOCK_METHOD3(Uniform2iv, void(GLint location, GLsizei count, const GLint* v));

  MOCK_METHOD4(Uniform3f,
               void(GLint location, GLfloat x, GLfloat y, GLfloat z));

  MOCK_METHOD3(Uniform3fv,
               void(GLint location, GLsizei count, const GLfloat* v));

  MOCK_METHOD4(Uniform3i, void(GLint location, GLint x, GLint y, GLint z));

  MOCK_METHOD3(Uniform3iv, void(GLint location, GLsizei count, const GLint* v));

  MOCK_METHOD5(Uniform4f, void(
      GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w));

  MOCK_METHOD3(Uniform4fv,
               void(GLint location, GLsizei count, const GLfloat* v));

  MOCK_METHOD5(Uniform4i,
               void(GLint location, GLint x, GLint y, GLint z, GLint w));

  MOCK_METHOD3(Uniform4iv, void(GLint location, GLsizei count, const GLint* v));

  MOCK_METHOD4(UniformMatrix2fv, void(
      GLint location, GLsizei count, GLboolean transpose,
      const GLfloat* value));

  MOCK_METHOD4(UniformMatrix3fv, void(
      GLint location, GLsizei count, GLboolean transpose,
      const GLfloat* value));

  MOCK_METHOD4(UniformMatrix4fv, void(
      GLint location, GLsizei count, GLboolean transpose,
      const GLfloat* value));

  MOCK_METHOD1(UseProgram, void(GLuint program));

  MOCK_METHOD1(ValidateProgram, void(GLuint program));

  MOCK_METHOD2(VertexAttrib1f, void(GLuint indx, GLfloat x));

  MOCK_METHOD2(VertexAttrib1fv, void(GLuint indx, const GLfloat* values));

  MOCK_METHOD3(VertexAttrib2f, void(GLuint indx, GLfloat x, GLfloat y));

  MOCK_METHOD2(VertexAttrib2fv, void(GLuint indx, const GLfloat* values));

  MOCK_METHOD4(VertexAttrib3f,
              void(GLuint indx, GLfloat x, GLfloat y, GLfloat z));

  MOCK_METHOD2(VertexAttrib3fv, void(GLuint indx, const GLfloat* values));

  MOCK_METHOD5(VertexAttrib4f, void(
      GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w));

  MOCK_METHOD2(VertexAttrib4fv, void(GLuint indx, const GLfloat* values));

  MOCK_METHOD6(VertexAttribPointer, void(
      GLuint indx, GLint size, GLenum type, GLboolean normalized,
      GLsizei stride, const void* ptr));

  MOCK_METHOD4(Viewport, void(GLint x, GLint y, GLsizei width, GLsizei height));

  MOCK_METHOD0(SwapBuffers, void());

  MOCK_METHOD4(GetMaxValueInBufferCHROMIUM, GLuint(
      GLuint buffer_id, GLsizei count, GLenum type, GLuint offset));

  MOCK_METHOD2(GenFencesNV, void(GLsizei n, GLuint *fences));

  MOCK_METHOD2(DeleteFencesNV, void(GLsizei n, const GLuint *fences));

  MOCK_METHOD2(SetFenceNV, void(GLuint fence, GLenum condition));

  MOCK_METHOD1(TestFenceNV, GLboolean(GLuint fence));

  MOCK_METHOD1(FinishFenceNV, void(GLuint fence));

  MOCK_METHOD1(IsFenceNV, GLboolean(GLuint fence));

  MOCK_METHOD3(GetFenceivNV, void(GLuint fence, GLenum pname, GLint *params));
};

}  // namespace gfx

#endif  // GPU_COMMAND_BUFFER_COMMON_GL_MOCK_H_

