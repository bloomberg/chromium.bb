
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


namespace command_buffer {
namespace gles2 {

namespace {

parse_error::ParseError ValidateActiveTexture(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum texture) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateAttachShader(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint program,
    GLuint shader) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateBindAttribLocation(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint program, GLuint index,
    const char* name) {
  if (name == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateBindAttribLocationImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint program, GLuint index,
    const char* name) {
  if (name == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateBindBuffer(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target,
    GLuint buffer) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateBindFramebuffer(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target,
    GLuint framebuffer) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateBindRenderbuffer(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target,
    GLuint renderbuffer) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateBindTexture(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target,
    GLuint texture) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateBlendColor(
    GLES2Decoder* decoder, unsigned int arg_count, GLclampf red, GLclampf green,
    GLclampf blue, GLclampf alpha) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateBlendEquation(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum mode) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateBlendEquationSeparate(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum modeRGB,
    GLenum modeAlpha) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateBlendFunc(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum sfactor,
    GLenum dfactor) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateBlendFuncSeparate(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum srcRGB, GLenum dstRGB,
    GLenum srcAlpha, GLenum dstAlpha) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateBufferData(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target,
    GLsizeiptr size, const void* data, GLenum usage) {
  if (data == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateBufferDataImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target,
    GLsizeiptr size, const void* data, GLenum usage) {
  if (data == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateBufferSubData(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target,
    GLintptr offset, GLsizeiptr size, const void* data) {
  if (data == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateBufferSubDataImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target,
    GLintptr offset, GLsizeiptr size, const void* data) {
  if (data == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateCheckFramebufferStatus(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateClear(
    GLES2Decoder* decoder, unsigned int arg_count, GLbitfield mask) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateClearColor(
    GLES2Decoder* decoder, unsigned int arg_count, GLclampf red, GLclampf green,
    GLclampf blue, GLclampf alpha) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateClearDepthf(
    GLES2Decoder* decoder, unsigned int arg_count, GLclampf depth) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateClearStencil(
    GLES2Decoder* decoder, unsigned int arg_count, GLint s) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateColorMask(
    GLES2Decoder* decoder, unsigned int arg_count, GLboolean red,
    GLboolean green, GLboolean blue, GLboolean alpha) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateCompileShader(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint shader) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateCompressedTexImage2D(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target, GLint level,
    GLenum internalformat, GLsizei width, GLsizei height, GLint border,
    GLsizei imageSize, const void* data) {
  if (data == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateCompressedTexImage2DImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target, GLint level,
    GLenum internalformat, GLsizei width, GLsizei height, GLint border,
    GLsizei imageSize, const void* data) {
  if (data == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateCompressedTexSubImage2D(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target, GLint level,
    GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format,
    GLsizei imageSize, const void* data) {
  if (data == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateCompressedTexSubImage2DImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target, GLint level,
    GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format,
    GLsizei imageSize, const void* data) {
  if (data == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateCopyTexImage2D(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target, GLint level,
    GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height,
    GLint border) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateCopyTexSubImage2D(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target, GLint level,
    GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width,
    GLsizei height) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateCreateProgram(
    GLES2Decoder* decoder, unsigned int arg_count) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateCreateShader(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum type) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateCullFace(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum mode) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateDeleteBuffers(
    GLES2Decoder* decoder, unsigned int arg_count, GLsizei n,
    const GLuint* buffers) {
  if (buffers == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateDeleteBuffersImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLsizei n,
    const GLuint* buffers) {
  if (buffers == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateDeleteFramebuffers(
    GLES2Decoder* decoder, unsigned int arg_count, GLsizei n,
    const GLuint* framebuffers) {
  if (framebuffers == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateDeleteFramebuffersImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLsizei n,
    const GLuint* framebuffers) {
  if (framebuffers == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateDeleteProgram(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint program) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateDeleteRenderbuffers(
    GLES2Decoder* decoder, unsigned int arg_count, GLsizei n,
    const GLuint* renderbuffers) {
  if (renderbuffers == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateDeleteRenderbuffersImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLsizei n,
    const GLuint* renderbuffers) {
  if (renderbuffers == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateDeleteShader(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint shader) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateDeleteTextures(
    GLES2Decoder* decoder, unsigned int arg_count, GLsizei n,
    const GLuint* textures) {
  if (textures == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateDeleteTexturesImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLsizei n,
    const GLuint* textures) {
  if (textures == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateDepthFunc(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum func) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateDepthMask(
    GLES2Decoder* decoder, unsigned int arg_count, GLboolean flag) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateDepthRangef(
    GLES2Decoder* decoder, unsigned int arg_count, GLclampf zNear,
    GLclampf zFar) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateDetachShader(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint program,
    GLuint shader) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateDisable(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum cap) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateDisableVertexAttribArray(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint index) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateDrawArrays(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum mode, GLint first,
    GLsizei count) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateDrawElements(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum mode, GLsizei count,
    GLenum type, const void* indices) {
  if (indices == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateEnable(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum cap) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateEnableVertexAttribArray(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint index) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateFinish(
    GLES2Decoder* decoder, unsigned int arg_count) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateFlush(
    GLES2Decoder* decoder, unsigned int arg_count) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateFramebufferRenderbuffer(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target,
    GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateFramebufferTexture2D(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target,
    GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateFrontFace(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum mode) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGenBuffers(
    GLES2Decoder* decoder, unsigned int arg_count, GLsizei n,
    GLuint* buffers) {
  if (buffers == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGenBuffersImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLsizei n,
    GLuint* buffers) {
  if (buffers == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGenerateMipmap(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGenFramebuffers(
    GLES2Decoder* decoder, unsigned int arg_count, GLsizei n,
    GLuint* framebuffers) {
  if (framebuffers == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGenFramebuffersImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLsizei n,
    GLuint* framebuffers) {
  if (framebuffers == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGenRenderbuffers(
    GLES2Decoder* decoder, unsigned int arg_count, GLsizei n,
    GLuint* renderbuffers) {
  if (renderbuffers == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGenRenderbuffersImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLsizei n,
    GLuint* renderbuffers) {
  if (renderbuffers == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGenTextures(
    GLES2Decoder* decoder, unsigned int arg_count, GLsizei n,
    GLuint* textures) {
  if (textures == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGenTexturesImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLsizei n,
    GLuint* textures) {
  if (textures == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGetActiveAttrib(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint program, GLuint index,
    GLsizei bufsize, GLsizei* length, GLint* size, GLenum* type, char* name) {
  if (length == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (size == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (type == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (name == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGetActiveUniform(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint program, GLuint index,
    GLsizei bufsize, GLsizei* length, GLint* size, GLenum* type, char* name) {
  if (length == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (size == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (type == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (name == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGetAttachedShaders(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint program,
    GLsizei maxcount, GLsizei* count, GLuint* shaders) {
  if (count == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (shaders == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGetAttribLocation(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint program,
    const char* name) {
  if (name == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGetAttribLocationImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint program,
    const char* name) {
  if (name == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGetBooleanv(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum pname,
    GLboolean* params) {
  if (params == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGetBufferParameteriv(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target, GLenum pname,
    GLint* params) {
  if (params == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGetError(
    GLES2Decoder* decoder, unsigned int arg_count) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGetFloatv(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum pname,
    GLfloat* params) {
  if (params == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGetFramebufferAttachmentParameteriv(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target,
    GLenum attachment, GLenum pname, GLint* params) {
  if (params == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGetIntegerv(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum pname,
    GLint* params) {
  if (params == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGetProgramiv(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint program, GLenum pname,
    GLint* params) {
  if (params == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGetProgramInfoLog(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint program,
    GLsizei bufsize, GLsizei* length, char* infolog) {
  if (length == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (infolog == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGetRenderbufferParameteriv(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target, GLenum pname,
    GLint* params) {
  if (params == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGetShaderiv(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint shader, GLenum pname,
    GLint* params) {
  if (params == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGetShaderInfoLog(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint shader,
    GLsizei bufsize, GLsizei* length, char* infolog) {
  if (length == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (infolog == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGetShaderPrecisionFormat(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum shadertype,
    GLenum precisiontype, GLint* range, GLint* precision) {
  if (range == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (precision == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGetShaderSource(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint shader,
    GLsizei bufsize, GLsizei* length, char* source) {
  if (length == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (source == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGetString(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum name) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGetTexParameterfv(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target, GLenum pname,
    GLfloat* params) {
  if (params == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGetTexParameteriv(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target, GLenum pname,
    GLint* params) {
  if (params == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGetUniformfv(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint program,
    GLint location, GLfloat* params) {
  if (params == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGetUniformiv(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint program,
    GLint location, GLint* params) {
  if (params == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGetUniformLocation(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint program,
    const char* name) {
  if (name == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGetUniformLocationImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint program,
    const char* name) {
  if (name == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGetVertexAttribfv(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint index, GLenum pname,
    GLfloat* params) {
  if (params == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGetVertexAttribiv(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint index, GLenum pname,
    GLint* params) {
  if (params == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateGetVertexAttribPointerv(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint index, GLenum pname,
    void** pointer) {
  if (pointer == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateHint(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target,
    GLenum mode) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateIsBuffer(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint buffer) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateIsEnabled(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum cap) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateIsFramebuffer(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint framebuffer) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateIsProgram(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint program) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateIsRenderbuffer(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint renderbuffer) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateIsShader(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint shader) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateIsTexture(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint texture) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateLineWidth(
    GLES2Decoder* decoder, unsigned int arg_count, GLfloat width) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateLinkProgram(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint program) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidatePixelStorei(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum pname, GLint param) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidatePolygonOffset(
    GLES2Decoder* decoder, unsigned int arg_count, GLfloat factor,
    GLfloat units) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateReadPixels(
    GLES2Decoder* decoder, unsigned int arg_count, GLint x, GLint y,
    GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels) {
  if (pixels == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateRenderbufferStorage(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target,
    GLenum internalformat, GLsizei width, GLsizei height) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateSampleCoverage(
    GLES2Decoder* decoder, unsigned int arg_count, GLclampf value,
    GLboolean invert) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateScissor(
    GLES2Decoder* decoder, unsigned int arg_count, GLint x, GLint y,
    GLsizei width, GLsizei height) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateShaderSource(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint shader, GLsizei count,
    const char** string, const GLint* length) {
  if (string == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (length == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateShaderSourceImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint shader, GLsizei count,
    const char** string, const GLint* length) {
  if (string == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (length == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateStencilFunc(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum func, GLint ref,
    GLuint mask) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateStencilFuncSeparate(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum face, GLenum func,
    GLint ref, GLuint mask) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateStencilMask(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint mask) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateStencilMaskSeparate(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum face, GLuint mask) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateStencilOp(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum fail, GLenum zfail,
    GLenum zpass) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateStencilOpSeparate(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum face, GLenum fail,
    GLenum zfail, GLenum zpass) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateTexImage2D(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target, GLint level,
    GLint internalformat, GLsizei width, GLsizei height, GLint border,
    GLenum format, GLenum type, const void* pixels) {
  if (pixels == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateTexImage2DImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target, GLint level,
    GLint internalformat, GLsizei width, GLsizei height, GLint border,
    GLenum format, GLenum type, const void* pixels) {
  if (pixels == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateTexParameterf(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target, GLenum pname,
    GLfloat param) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateTexParameterfv(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target, GLenum pname,
    const GLfloat* params) {
  if (params == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateTexParameterfvImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target, GLenum pname,
    const GLfloat* params) {
  if (params == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (!CheckImmediateDataSize<TexParameterfvImmediate>(
      arg_count, 1, sizeof(GLfloat), 1)) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateTexParameteri(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target, GLenum pname,
    GLint param) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateTexParameteriv(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target, GLenum pname,
    const GLint* params) {
  if (params == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateTexParameterivImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target, GLenum pname,
    const GLint* params) {
  if (params == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (!CheckImmediateDataSize<TexParameterivImmediate>(
      arg_count, 1, sizeof(GLint), 1)) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateTexSubImage2D(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target, GLint level,
    GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format,
    GLenum type, const void* pixels) {
  if (pixels == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateTexSubImage2DImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLenum target, GLint level,
    GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format,
    GLenum type, const void* pixels) {
  if (pixels == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniform1f(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location, GLfloat x) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniform1fv(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location,
    GLsizei count, const GLfloat* v) {
  if (v == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniform1fvImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location,
    GLsizei count, const GLfloat* v) {
  if (v == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (!CheckImmediateDataSize<Uniform1fvImmediate>(
      arg_count, count, sizeof(GLfloat), 1)) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniform1i(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location, GLint x) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniform1iv(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location,
    GLsizei count, const GLint* v) {
  if (v == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniform1ivImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location,
    GLsizei count, const GLint* v) {
  if (v == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (!CheckImmediateDataSize<Uniform1ivImmediate>(
      arg_count, count, sizeof(GLint), 1)) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniform2f(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location, GLfloat x,
    GLfloat y) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniform2fv(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location,
    GLsizei count, const GLfloat* v) {
  if (v == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniform2fvImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location,
    GLsizei count, const GLfloat* v) {
  if (v == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (!CheckImmediateDataSize<Uniform2fvImmediate>(
      arg_count, count, sizeof(GLfloat), 2)) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniform2i(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location, GLint x,
    GLint y) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniform2iv(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location,
    GLsizei count, const GLint* v) {
  if (v == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniform2ivImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location,
    GLsizei count, const GLint* v) {
  if (v == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (!CheckImmediateDataSize<Uniform2ivImmediate>(
      arg_count, count, sizeof(GLint), 2)) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniform3f(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location, GLfloat x,
    GLfloat y, GLfloat z) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniform3fv(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location,
    GLsizei count, const GLfloat* v) {
  if (v == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniform3fvImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location,
    GLsizei count, const GLfloat* v) {
  if (v == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (!CheckImmediateDataSize<Uniform3fvImmediate>(
      arg_count, count, sizeof(GLfloat), 3)) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniform3i(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location, GLint x,
    GLint y, GLint z) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniform3iv(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location,
    GLsizei count, const GLint* v) {
  if (v == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniform3ivImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location,
    GLsizei count, const GLint* v) {
  if (v == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (!CheckImmediateDataSize<Uniform3ivImmediate>(
      arg_count, count, sizeof(GLint), 3)) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniform4f(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location, GLfloat x,
    GLfloat y, GLfloat z, GLfloat w) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniform4fv(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location,
    GLsizei count, const GLfloat* v) {
  if (v == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniform4fvImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location,
    GLsizei count, const GLfloat* v) {
  if (v == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (!CheckImmediateDataSize<Uniform4fvImmediate>(
      arg_count, count, sizeof(GLfloat), 4)) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniform4i(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location, GLint x,
    GLint y, GLint z, GLint w) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniform4iv(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location,
    GLsizei count, const GLint* v) {
  if (v == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniform4ivImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location,
    GLsizei count, const GLint* v) {
  if (v == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (!CheckImmediateDataSize<Uniform4ivImmediate>(
      arg_count, count, sizeof(GLint), 4)) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniformMatrix2fv(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location,
    GLsizei count, GLboolean transpose, const GLfloat* value) {
  if (value == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniformMatrix2fvImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location,
    GLsizei count, GLboolean transpose, const GLfloat* value) {
  if (value == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (!CheckImmediateDataSize<UniformMatrix2fvImmediate>(
      arg_count, count, sizeof(GLfloat), 4)) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniformMatrix3fv(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location,
    GLsizei count, GLboolean transpose, const GLfloat* value) {
  if (value == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniformMatrix3fvImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location,
    GLsizei count, GLboolean transpose, const GLfloat* value) {
  if (value == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (!CheckImmediateDataSize<UniformMatrix3fvImmediate>(
      arg_count, count, sizeof(GLfloat), 9)) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniformMatrix4fv(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location,
    GLsizei count, GLboolean transpose, const GLfloat* value) {
  if (value == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUniformMatrix4fvImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLint location,
    GLsizei count, GLboolean transpose, const GLfloat* value) {
  if (value == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (!CheckImmediateDataSize<UniformMatrix4fvImmediate>(
      arg_count, count, sizeof(GLfloat), 16)) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateUseProgram(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint program) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateValidateProgram(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint program) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateVertexAttrib1f(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint indx, GLfloat x) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateVertexAttrib1fv(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint indx,
    const GLfloat* values) {
  if (values == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateVertexAttrib1fvImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint indx,
    const GLfloat* values) {
  if (values == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (!CheckImmediateDataSize<VertexAttrib1fvImmediate>(
      arg_count, 1, sizeof(GLfloat), 1)) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateVertexAttrib2f(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint indx, GLfloat x,
    GLfloat y) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateVertexAttrib2fv(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint indx,
    const GLfloat* values) {
  if (values == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateVertexAttrib2fvImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint indx,
    const GLfloat* values) {
  if (values == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (!CheckImmediateDataSize<VertexAttrib2fvImmediate>(
      arg_count, 1, sizeof(GLfloat), 2)) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateVertexAttrib3f(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint indx, GLfloat x,
    GLfloat y, GLfloat z) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateVertexAttrib3fv(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint indx,
    const GLfloat* values) {
  if (values == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateVertexAttrib3fvImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint indx,
    const GLfloat* values) {
  if (values == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (!CheckImmediateDataSize<VertexAttrib3fvImmediate>(
      arg_count, 1, sizeof(GLfloat), 3)) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateVertexAttrib4f(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint indx, GLfloat x,
    GLfloat y, GLfloat z, GLfloat w) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateVertexAttrib4fv(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint indx,
    const GLfloat* values) {
  if (values == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateVertexAttrib4fvImmediate(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint indx,
    const GLfloat* values) {
  if (values == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  if (!CheckImmediateDataSize<VertexAttrib4fvImmediate>(
      arg_count, 1, sizeof(GLfloat), 4)) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateVertexAttribPointer(
    GLES2Decoder* decoder, unsigned int arg_count, GLuint indx, GLint size,
    GLenum type, GLboolean normalized, GLsizei stride, const void* ptr) {
  if (ptr == NULL) {
    return parse_error::kParseOutOfBounds;
  }
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateViewport(
    GLES2Decoder* decoder, unsigned int arg_count, GLint x, GLint y,
    GLsizei width, GLsizei height) {
  return parse_error::kParseNoError;
}
parse_error::ParseError ValidateSwapBuffers(
    GLES2Decoder* decoder, unsigned int arg_count) {
  return parse_error::kParseNoError;
}
}  // anonymous namespace
}  // namespace gles2
}  // namespace command_buffer

