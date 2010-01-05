// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements glue to a GL interface so we can mock it for unit
// testing. It has to be Desktop GL, not GLES2 as it is used to test the service
// side code.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GL_INTERFACE_H_
#define GPU_COMMAND_BUFFER_SERVICE_GL_INTERFACE_H_

#include <GLES2/gl2types.h>

// These are Desktop GL constants that we want to test that our GLES2
// implemenation does not let through.
#define GL_PROXY_TEXTURE_CUBE_MAP 0x851B
#define GL_BGR 0x80E0
#define GL_BGRA 0x80E1
#define GL_UNPACK_SWAP_BYTES 0x0CF0
#define GL_PACK_SWAP_BYTES 0x0D00
#define GL_PERSPECTIVE_CORRECTION_HINT 0x0C50
#define GL_QUADS 0x0007
#define GL_POLYGON 0x0009
#define GL_FOG 0x0B60
#define GL_CLIP_PLANE0 0x3000
#define GL_GENERATE_MIPMAP 0x8191
#define GL_PIXEL_PACK_BUFFER 0x88EB
#define GL_POINT_SPRITE 0x8861
#define GL_MIN 0x8007
#define GL_MAX 0x8008
#define GL_TEXTURE_1D 0x0DE0
#define GL_TEXTURE_3D 0x806F
#define GL_DOUBLE 0x140A

namespace gles2 {

class GLInterface {
 public:
  virtual ~GLInterface() {
  }

  static void SetGLInterface(GLInterface* gl_interface);

  static GLInterface* GetGLInterface();

  virtual void ActiveTexture(GLenum texture) = 0;

  virtual void AttachShader(GLuint program, GLuint shader) = 0;

  virtual void BindAttribLocation(
      GLuint program, GLuint index, const char* name) = 0;

  virtual void BindBuffer(GLenum target, GLuint buffer) = 0;

  virtual void BindFramebufferEXT(GLenum target, GLuint framebuffer) = 0;

  virtual void BindRenderbufferEXT(GLenum target, GLuint renderbuffer) = 0;

  virtual void BindTexture(GLenum target, GLuint texture) = 0;

  virtual void BlendColor(
      GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) = 0;

  virtual void BlendEquation(GLenum mode) = 0;

  virtual void BlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) = 0;

  virtual void BlendFunc(GLenum sfactor, GLenum dfactor) = 0;

  virtual void BlendFuncSeparate(
      GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) = 0;

  virtual void BufferData(
      GLenum target, GLsizeiptr size, const void* data, GLenum usage) = 0;

  virtual void BufferSubData(
      GLenum target, GLintptr offset, GLsizeiptr size, const void* data) = 0;

  virtual GLenum CheckFramebufferStatusEXT(GLenum target) = 0;

  virtual void Clear(GLbitfield mask) = 0;

  virtual void ClearColor(
      GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) = 0;

  virtual void ClearDepth(GLclampf depth) = 0;

  virtual void ClearStencil(GLint s) = 0;

  virtual void ColorMask(
      GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) = 0;

  virtual void CompileShader(GLuint shader) = 0;

  virtual void CompressedTexImage2D(
      GLenum target, GLint level, GLenum internalformat, GLsizei width,
      GLsizei height, GLint border, GLsizei imageSize, const void* data) = 0;

  virtual void CompressedTexSubImage2D(
      GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
      GLsizei height, GLenum format, GLsizei imageSize, const void* data) = 0;

  virtual void CopyTexImage2D(
      GLenum target, GLint level, GLenum internalformat, GLint x, GLint y,
      GLsizei width, GLsizei height, GLint border) = 0;

  virtual void CopyTexSubImage2D(
      GLenum target, GLint level, GLint xoffset, GLint yoffset,
      GLint x, GLint y, GLsizei width, GLsizei height) = 0;

  virtual GLuint CreateProgram() = 0;

  virtual GLuint CreateShader(GLenum type) = 0;

  virtual void CullFace(GLenum mode) = 0;

  virtual void DeleteBuffersARB(GLsizei n, const GLuint* buffers) = 0;

  virtual void DeleteFramebuffersEXT(GLsizei n, const GLuint* framebuffers) = 0;

  virtual void DeleteProgram(GLuint program) = 0;

  virtual void DeleteRenderbuffersEXT(
      GLsizei n, const GLuint* renderbuffers) = 0;

  virtual void DeleteShader(GLuint shader) = 0;

  virtual void DeleteTextures(GLsizei n, const GLuint* textures) = 0;

  virtual void DepthFunc(GLenum func) = 0;

  virtual void DepthMask(GLboolean flag) = 0;

  virtual void DepthRange(GLclampf zNear, GLclampf zFar) = 0;

  virtual void DetachShader(GLuint program, GLuint shader) = 0;

  virtual void Disable(GLenum cap) = 0;

  virtual void DisableVertexAttribArray(GLuint index) = 0;

  virtual void DrawArrays(GLenum mode, GLint first, GLsizei count) = 0;

  virtual void DrawElements(
      GLenum mode, GLsizei count, GLenum type, const void* indices) = 0;

  virtual void Enable(GLenum cap) = 0;

  virtual void EnableVertexAttribArray(GLuint index) = 0;

  virtual void Finish() = 0;

  virtual void Flush() = 0;

  virtual void FramebufferRenderbufferEXT(
      GLenum target, GLenum attachment, GLenum renderbuffertarget,
      GLuint renderbuffer) = 0;

  virtual void FramebufferTexture2DEXT(
      GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
      GLint level) = 0;

  virtual void FrontFace(GLenum mode) = 0;

  virtual void GenBuffersARB(GLsizei n, GLuint* buffers) = 0;

  virtual void GenerateMipmapEXT(GLenum target) = 0;

  virtual void GenFramebuffersEXT(GLsizei n, GLuint* framebuffers) = 0;

  virtual void GenRenderbuffersEXT(GLsizei n, GLuint* renderbuffers) = 0;

  virtual void GenTextures(GLsizei n, GLuint* textures) = 0;

  virtual void GetActiveAttrib(
      GLuint program, GLuint index, GLsizei bufsize, GLsizei* length,
      GLint* size, GLenum* type, char* name) = 0;

  virtual void GetActiveUniform(
      GLuint program, GLuint index, GLsizei bufsize, GLsizei* length,
      GLint* size, GLenum* type, char* name) = 0;

  virtual void GetAttachedShaders(
      GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders) = 0;

  virtual GLint GetAttribLocation(GLuint program, const char* name) = 0;

  virtual void GetBooleanv(GLenum pname, GLboolean* params) = 0;

  virtual void GetBufferParameteriv(
      GLenum target, GLenum pname, GLint* params) = 0;

  virtual GLenum GetError() = 0;

  virtual void GetFloatv(GLenum pname, GLfloat* params) = 0;

  virtual void GetFramebufferAttachmentParameterivEXT(
      GLenum target, GLenum attachment, GLenum pname, GLint* params) = 0;

  virtual void GetIntegerv(GLenum pname, GLint* params) = 0;

  virtual void GetProgramiv(GLuint program, GLenum pname, GLint* params) = 0;

  // TODO(gman): Implement this
  virtual void GetProgramInfoLog(
      GLuint program, GLsizei bufsize, GLsizei* length, char* infolog) = 0;

  virtual void GetRenderbufferParameterivEXT(
      GLenum target, GLenum pname, GLint* params) = 0;

  virtual void GetShaderiv(GLuint shader, GLenum pname, GLint* params) = 0;

  // TODO(gman): Implement this
  virtual void GetShaderInfoLog(
      GLuint shader, GLsizei bufsize, GLsizei* length, char* infolog) = 0;

  virtual void GetShaderPrecisionFormat(
      GLenum shadertype, GLenum precisiontype, GLint* range,
      GLint* precision) = 0;

  // TODO(gman): Implement this
  virtual void GetShaderSource(
      GLuint shader, GLsizei bufsize, GLsizei* length, char* source) = 0;

  virtual const GLubyte* GetString(GLenum name) = 0;

  virtual void GetTexParameterfv(
      GLenum target, GLenum pname, GLfloat* params) = 0;

  virtual void GetTexParameteriv(
      GLenum target, GLenum pname, GLint* params) = 0;

  virtual void GetUniformfv(
      GLuint program, GLint location, GLfloat* params) = 0;

  virtual void GetUniformiv(GLuint program, GLint location, GLint* params) = 0;

  virtual GLint GetUniformLocation(GLuint program, const char* name) = 0;

  virtual void GetVertexAttribfv(
      GLuint index, GLenum pname, GLfloat* params) = 0;

  virtual void GetVertexAttribiv(GLuint index, GLenum pname, GLint* params) = 0;

  virtual void GetVertexAttribPointerv(
      GLuint index, GLenum pname, void** pointer) = 0;

  virtual void Hint(GLenum target, GLenum mode) = 0;

  virtual GLboolean IsBuffer(GLuint buffer) = 0;

  virtual GLboolean IsEnabled(GLenum cap) = 0;

  virtual GLboolean IsFramebufferEXT(GLuint framebuffer) = 0;

  virtual GLboolean IsProgram(GLuint program) = 0;

  virtual GLboolean IsRenderbufferEXT(GLuint renderbuffer) = 0;

  virtual GLboolean IsShader(GLuint shader) = 0;

  virtual GLboolean IsTexture(GLuint texture) = 0;

  virtual void LineWidth(GLfloat width) = 0;

  virtual void LinkProgram(GLuint program) = 0;

  virtual void PixelStorei(GLenum pname, GLint param) = 0;

  virtual void PolygonOffset(GLfloat factor, GLfloat units) = 0;

  virtual void ReadPixels(
      GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
      GLenum type, void* pixels) = 0;

  virtual void RenderbufferStorageEXT(
      GLenum target, GLenum internalformat, GLsizei width, GLsizei height) = 0;

  virtual void SampleCoverage(GLclampf value, GLboolean invert) = 0;

  virtual void Scissor(GLint x, GLint y, GLsizei width, GLsizei height) = 0;

  virtual void ShaderSource(
      GLuint shader, GLsizei count, const char** str, const
      GLint* length) = 0;

  virtual void StencilFunc(GLenum func, GLint ref, GLuint mask) = 0;

  virtual void StencilFuncSeparate(
      GLenum face, GLenum func, GLint ref, GLuint mask) = 0;

  virtual void StencilMask(GLuint mask) = 0;

  virtual void StencilMaskSeparate(GLenum face, GLuint mask) = 0;

  virtual void StencilOp(GLenum fail, GLenum zfail, GLenum zpass) = 0;

  virtual void StencilOpSeparate(
      GLenum face, GLenum fail, GLenum zfail, GLenum zpass) = 0;

  virtual void TexImage2D(
      GLenum target, GLint level, GLint internalformat, GLsizei width,
      GLsizei height, GLint border, GLenum format, GLenum type,
      const void* pixels) = 0;

  virtual void TexParameterf(GLenum target, GLenum pname, GLfloat param) = 0;

  virtual void TexParameterfv(
      GLenum target, GLenum pname, const GLfloat* params) = 0;

  virtual void TexParameteri(GLenum target, GLenum pname, GLint param) = 0;

  virtual void TexParameteriv(
      GLenum target, GLenum pname, const GLint* params) = 0;

  virtual void TexSubImage2D(
      GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
      GLsizei height, GLenum format, GLenum type, const void* pixels) = 0;

  virtual void Uniform1f(GLint location, GLfloat x) = 0;

  virtual void Uniform1fv(GLint location, GLsizei count, const GLfloat* v) = 0;

  virtual void Uniform1i(GLint location, GLint x) = 0;

  virtual void Uniform1iv(GLint location, GLsizei count, const GLint* v) = 0;

  virtual void Uniform2f(GLint location, GLfloat x, GLfloat y) = 0;

  virtual void Uniform2fv(GLint location, GLsizei count, const GLfloat* v) = 0;

  virtual void Uniform2i(GLint location, GLint x, GLint y) = 0;

  virtual void Uniform2iv(GLint location, GLsizei count, const GLint* v) = 0;

  virtual void Uniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z) = 0;

  virtual void Uniform3fv(GLint location, GLsizei count, const GLfloat* v) = 0;

  virtual void Uniform3i(GLint location, GLint x, GLint y, GLint z) = 0;

  virtual void Uniform3iv(GLint location, GLsizei count, const GLint* v) = 0;

  virtual void Uniform4f(
      GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w) = 0;

  virtual void Uniform4fv(GLint location, GLsizei count, const GLfloat* v) = 0;

  virtual void Uniform4i(
      GLint location, GLint x, GLint y, GLint z, GLint w) = 0;

  virtual void Uniform4iv(GLint location, GLsizei count, const GLint* v) = 0;

  virtual void UniformMatrix2fv(
      GLint location, GLsizei count, GLboolean transpose,
      const GLfloat* value) = 0;

  virtual void UniformMatrix3fv(
      GLint location, GLsizei count, GLboolean transpose,
      const GLfloat* value) = 0;

  virtual void UniformMatrix4fv(
      GLint location, GLsizei count, GLboolean transpose,
      const GLfloat* value) = 0;

  virtual void UseProgram(GLuint program) = 0;

  virtual void ValidateProgram(GLuint program) = 0;

  virtual void VertexAttrib1f(GLuint indx, GLfloat x) = 0;

  virtual void VertexAttrib1fv(GLuint indx, const GLfloat* values) = 0;

  virtual void VertexAttrib2f(GLuint indx, GLfloat x, GLfloat y) = 0;

  virtual void VertexAttrib2fv(GLuint indx, const GLfloat* values) = 0;

  virtual void VertexAttrib3f(GLuint indx, GLfloat x, GLfloat y, GLfloat z) = 0;

  virtual void VertexAttrib3fv(GLuint indx, const GLfloat* values) = 0;

  virtual void VertexAttrib4f(
      GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w) = 0;

  virtual void VertexAttrib4fv(GLuint indx, const GLfloat* values) = 0;

  virtual void VertexAttribPointer(
      GLuint indx, GLint size, GLenum type, GLboolean normalized,
      GLsizei stride, const void* ptr) = 0;

  virtual void Viewport(GLint x, GLint y, GLsizei width, GLsizei height) = 0;

  virtual void SwapBuffers() = 0;

 private:
  static GLInterface* interface_;
};

}  // namespace gles2

#define GL_IFACE_GET_FUN(name) ::gles2::GLInterface::GetGLInterface()->name

#define glActiveTexture GL_IFACE_GET_FUN(ActiveTexture)
#define glAttachShader GL_IFACE_GET_FUN(AttachShader)
#define glBindAttribLocation GL_IFACE_GET_FUN(BindAttribLocation)
#define glBindBuffer GL_IFACE_GET_FUN(BindBuffer)
#define glBindFramebufferEXT GL_IFACE_GET_FUN(BindFramebufferEXT)
#define glBindRenderbufferEXT GL_IFACE_GET_FUN(BindRenderbufferEXT)
#define glBindTexture GL_IFACE_GET_FUN(BindTexture)
#define glBlendColor GL_IFACE_GET_FUN(BlendColor)
#define glBlendEquation GL_IFACE_GET_FUN(BlendEquation)
#define glBlendEquationSeparate GL_IFACE_GET_FUN(BlendEquationSeparate)
#define glBlendFunc GL_IFACE_GET_FUN(BlendFunc)
#define glBlendFuncSeparate GL_IFACE_GET_FUN(BlendFuncSeparate)
#define glBufferData GL_IFACE_GET_FUN(BufferData)
#define glBufferSubData GL_IFACE_GET_FUN(BufferSubData)
#define glCheckFramebufferStatusEXT GL_IFACE_GET_FUN(CheckFramebufferStatusEXT)
#define glClear GL_IFACE_GET_FUN(Clear)
#define glClearColor GL_IFACE_GET_FUN(ClearColor)
#define glClearDepth GL_IFACE_GET_FUN(ClearDepth)
#define glClearStencil GL_IFACE_GET_FUN(ClearStencil)
#define glColorMask GL_IFACE_GET_FUN(ColorMask)
#define glCompileShader GL_IFACE_GET_FUN(CompileShader)
#define glCompressedTexImage2D GL_IFACE_GET_FUN(CompressedTexImage2D)
#define glCompressedTexSubImage2D GL_IFACE_GET_FUN(CompressedTexSubImage2D)
#define glCopyTexImage2D GL_IFACE_GET_FUN(CopyTexImage2D)
#define glCopyTexSubImage2D GL_IFACE_GET_FUN(CopyTexSubImage2D)
#define glCreateProgram GL_IFACE_GET_FUN(CreateProgram)
#define glCreateShader GL_IFACE_GET_FUN(CreateShader)
#define glCullFace GL_IFACE_GET_FUN(CullFace)
#define glDeleteBuffersARB GL_IFACE_GET_FUN(DeleteBuffersARB)
#define glDeleteFramebuffersEXT GL_IFACE_GET_FUN(DeleteFramebuffersEXT)
#define glDeleteProgram GL_IFACE_GET_FUN(DeleteProgram)
#define glDeleteRenderbuffersEXT GL_IFACE_GET_FUN(DeleteRenderbuffersEXT)
#define glDeleteShader GL_IFACE_GET_FUN(DeleteShader)
#define glDeleteTextures GL_IFACE_GET_FUN(DeleteTextures)
#define glDepthFunc GL_IFACE_GET_FUN(DepthFunc)
#define glDepthMask GL_IFACE_GET_FUN(DepthMask)
#define glDepthRange GL_IFACE_GET_FUN(DepthRange)
#define glDetachShader GL_IFACE_GET_FUN(DetachShader)
#define glDisable GL_IFACE_GET_FUN(Disable)
#define glDisableVertexAttribArray GL_IFACE_GET_FUN(DisableVertexAttribArray)
#define glDrawArrays GL_IFACE_GET_FUN(DrawArrays)
#define glDrawElements GL_IFACE_GET_FUN(DrawElements)
#define glEnable GL_IFACE_GET_FUN(Enable)
#define glEnableVertexAttribArray GL_IFACE_GET_FUN(EnableVertexAttribArray)
#define glFinish GL_IFACE_GET_FUN(Finish)
#define glFlush GL_IFACE_GET_FUN(Flush)
#define glFramebufferRenderbufferEXT \
    GL_IFACE_GET_FUN(FramebufferRenderbufferEXT)
#define glFramebufferTexture2DEXT GL_IFACE_GET_FUN(FramebufferTexture2DEXT)
#define glFrontFace GL_IFACE_GET_FUN(FrontFace)
#define glGenBuffersARB GL_IFACE_GET_FUN(GenBuffersARB)
#define glGenerateMipmapEXT GL_IFACE_GET_FUN(GenerateMipmapEXT)
#define glGenFramebuffersEXT GL_IFACE_GET_FUN(GenFramebuffersEXT)
#define glGenRenderbuffersEXT GL_IFACE_GET_FUN(GenRenderbuffersEXT)
#define glGenTextures GL_IFACE_GET_FUN(GenTextures)
#define glGetActiveAttrib GL_IFACE_GET_FUN(GetActiveAttrib)
#define glGetActiveUniform GL_IFACE_GET_FUN(GetActiveUniform)
#define glGetAttachedShaders GL_IFACE_GET_FUN(GetAttachedShaders)
#define glGetAttribLocation GL_IFACE_GET_FUN(GetAttribLocation)
#define glGetBooleanv GL_IFACE_GET_FUN(GetBooleanv)
#define glGetBufferParameteriv GL_IFACE_GET_FUN(GetBufferParameteriv)
#define glGetError GL_IFACE_GET_FUN(GetError)
#define glGetFloatv GL_IFACE_GET_FUN(GetFloatv)
#define glGetFramebufferAttachmentParameterivEXT \
    GL_IFACE_GET_FUN(GetFramebufferAttachmentParameterivEXT)
#define glGetIntegerv GL_IFACE_GET_FUN(GetIntegerv)
#define glGetProgramiv GL_IFACE_GET_FUN(GetProgramiv)
#define glGetProgramInfoLog GL_IFACE_GET_FUN(GetProgramInfoLog)
#define glGetRenderbufferParameterivEXT \
    GL_IFACE_GET_FUN(GetRenderbufferParameterivEXT)
#define glGetShaderiv GL_IFACE_GET_FUN(GetShaderiv)
#define glGetShaderInfoLog GL_IFACE_GET_FUN(GetShaderInfoLog)
#define glGetShaderPrecisionFormat GL_IFACE_GET_FUN(GetShaderPrecisionFormat)
#define glGetShaderSource GL_IFACE_GET_FUN(GetShaderSource)
#define glGetString GL_IFACE_GET_FUN(GetString)
#define glGetTexParameterfv GL_IFACE_GET_FUN(GetTexParameterfv)
#define glGetTexParameteriv GL_IFACE_GET_FUN(GetTexParameteriv)
#define glGetUniformfv GL_IFACE_GET_FUN(GetUniformfv)
#define glGetUniformiv GL_IFACE_GET_FUN(GetUniformiv)
#define glGetUniformLocation GL_IFACE_GET_FUN(GetUniformLocation)
#define glGetVertexAttribfv GL_IFACE_GET_FUN(GetVertexAttribfv)
#define glGetVertexAttribiv GL_IFACE_GET_FUN(GetVertexAttribiv)
#define glGetVertexAttribPointerv GL_IFACE_GET_FUN(GetVertexAttribPointerv)
#define glHint GL_IFACE_GET_FUN(Hint)
#define glIsBuffer GL_IFACE_GET_FUN(IsBuffer)
#define glIsEnabled GL_IFACE_GET_FUN(IsEnabled)
#define glIsFramebufferEXT GL_IFACE_GET_FUN(IsFramebufferEXT)
#define glIsProgram GL_IFACE_GET_FUN(IsProgram)
#define glIsRenderbufferEXT GL_IFACE_GET_FUN(IsRenderbufferEXT)
#define glIsShader GL_IFACE_GET_FUN(IsShader)
#define glIsTexture GL_IFACE_GET_FUN(IsTexture)
#define glLineWidth GL_IFACE_GET_FUN(LineWidth)
#define glLinkProgram GL_IFACE_GET_FUN(LinkProgram)
#define glPixelStorei GL_IFACE_GET_FUN(PixelStorei)
#define glPolygonOffset GL_IFACE_GET_FUN(PolygonOffset)
#define glReadPixels GL_IFACE_GET_FUN(ReadPixels)
#define glReleaseShaderCompiler GL_IFACE_GET_FUN(ReleaseShaderCompiler)
#define glRenderbufferStorageEXT GL_IFACE_GET_FUN(RenderbufferStorageEXT)
#define glSampleCoverage GL_IFACE_GET_FUN(SampleCoverage)
#define glScissor GL_IFACE_GET_FUN(Scissor)
#define glShaderBinary GL_IFACE_GET_FUN(ShaderBinary)
#define glShaderSource GL_IFACE_GET_FUN(ShaderSource)
#define glStencilFunc GL_IFACE_GET_FUN(StencilFunc)
#define glStencilFuncSeparate GL_IFACE_GET_FUN(StencilFuncSeparate)
#define glStencilMask GL_IFACE_GET_FUN(StencilMask)
#define glStencilMaskSeparate GL_IFACE_GET_FUN(StencilMaskSeparate)
#define glStencilOp GL_IFACE_GET_FUN(StencilOp)
#define glStencilOpSeparate GL_IFACE_GET_FUN(StencilOpSeparate)
#define glTexImage2D GL_IFACE_GET_FUN(TexImage2D)
#define glTexParameterf GL_IFACE_GET_FUN(TexParameterf)
#define glTexParameterfv GL_IFACE_GET_FUN(TexParameterfv)
#define glTexParameteri GL_IFACE_GET_FUN(TexParameteri)
#define glTexParameteriv GL_IFACE_GET_FUN(TexParameteriv)
#define glTexSubImage2D GL_IFACE_GET_FUN(TexSubImage2D)
#define glUniform1f GL_IFACE_GET_FUN(Uniform1f)
#define glUniform1fv GL_IFACE_GET_FUN(Uniform1fv)
#define glUniform1i GL_IFACE_GET_FUN(Uniform1i)
#define glUniform1iv GL_IFACE_GET_FUN(Uniform1iv)
#define glUniform2f GL_IFACE_GET_FUN(Uniform2f)
#define glUniform2fv GL_IFACE_GET_FUN(Uniform2fv)
#define glUniform2i GL_IFACE_GET_FUN(Uniform2i)
#define glUniform2iv GL_IFACE_GET_FUN(Uniform2iv)
#define glUniform3f GL_IFACE_GET_FUN(Uniform3f)
#define glUniform3fv GL_IFACE_GET_FUN(Uniform3fv)
#define glUniform3i GL_IFACE_GET_FUN(Uniform3i)
#define glUniform3iv GL_IFACE_GET_FUN(Uniform3iv)
#define glUniform4f GL_IFACE_GET_FUN(Uniform4f)
#define glUniform4fv GL_IFACE_GET_FUN(Uniform4fv)
#define glUniform4i GL_IFACE_GET_FUN(Uniform4i)
#define glUniform4iv GL_IFACE_GET_FUN(Uniform4iv)
#define glUniformMatrix2fv GL_IFACE_GET_FUN(UniformMatrix2fv)
#define glUniformMatrix3fv GL_IFACE_GET_FUN(UniformMatrix3fv)
#define glUniformMatrix4fv GL_IFACE_GET_FUN(UniformMatrix4fv)
#define glUseProgram GL_IFACE_GET_FUN(UseProgram)
#define glValidateProgram GL_IFACE_GET_FUN(ValidateProgram)
#define glVertexAttrib1f GL_IFACE_GET_FUN(VertexAttrib1f)
#define glVertexAttrib1fv GL_IFACE_GET_FUN(VertexAttrib1fv)
#define glVertexAttrib2f GL_IFACE_GET_FUN(VertexAttrib2f)
#define glVertexAttrib2fv GL_IFACE_GET_FUN(VertexAttrib2fv)
#define glVertexAttrib3f GL_IFACE_GET_FUN(VertexAttrib3f)
#define glVertexAttrib3fv GL_IFACE_GET_FUN(VertexAttrib3fv)
#define glVertexAttrib4f GL_IFACE_GET_FUN(VertexAttrib4f)
#define glVertexAttrib4fv GL_IFACE_GET_FUN(VertexAttrib4fv)
#define glVertexAttribPointer GL_IFACE_GET_FUN(VertexAttribPointer)
#define glViewport GL_IFACE_GET_FUN(Viewport)

#endif  // GPU_COMMAND_BUFFER_SERVICE_GL_INTERFACE_H_



