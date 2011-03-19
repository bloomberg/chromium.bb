// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "app/gfx/gl/gl_bindings_skia.h"

#include "app/gfx/gl/gl_bindings.h"
#include "app/gfx/gl/gl_implementation.h"
#include "base/logging.h"

// Skia is built against the headers in gpu\GLES.  These functions
// are exported without any call-type modifiers.
#define GR_GL_FUNCTION_TYPE

// The GrGLInterface header pulls in the default GL headers.  Disable pulling
// in the default platform headers to prevent conflicts with the GL api points
// included via gl_bindings.h
#define GR_GL_NO_PLATFORM_INCLUDES 1
#include "third_party/skia/gpu/include/GrGLInterface.h"
#undef GR_GL_NO_PLATFORM_INCLUDES

namespace {

extern "C" {

// The following stub functions are required because the glXXX routines exported
// via gl_bindings.h use call-type GL_BINDING_CALL, which on Windows is stdcall.
// Skia has been built against the GLES headers, so the interfaces in
// GrGLInterface are __cdecl.

GLvoid StubGLActiveTexture(GLenum texture) {
  glActiveTexture(texture);
}

GLvoid StubGLAttachShader(GLuint program, GLuint shader) {
  glAttachShader(program, shader);
}

GLvoid StubGLBindAttribLocation(GLuint program, GLuint index,
                                const char* name) {
  glBindAttribLocation(program, index, name);
}

GLvoid StubGLBindBuffer(GLenum target, GLuint buffer) {
  glBindBuffer(target, buffer);
}

GLvoid StubGLBindFramebuffer(GLenum target, GLuint framebuffer) {
  glBindFramebufferEXT(target, framebuffer);
}

GLvoid StubGLBindRenderbuffer(GLenum target, GLuint renderbuffer) {
  glBindRenderbufferEXT(target, renderbuffer);
}

GLvoid StubGLBindTexture(GLenum target, GLuint texture) {
  glBindTexture(target, texture);
}

GLvoid StubGLBlendColor(GLclampf red, GLclampf green, GLclampf blue,
                        GLclampf alpha) {
  glBlendColor(red, green, blue, alpha);
}

GLvoid StubGLBlendFunc(GLenum sfactor, GLenum dfactor) {
  glBlendFunc(sfactor, dfactor);
}

GLvoid StubGLBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                             GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                             GLbitfield mask, GLenum filter) {
  glBlitFramebufferEXT(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1,
                       mask, filter);
}

GLvoid StubGLBufferData(GLenum target, GLsizei size, const void* data,
                        GLenum usage) {
  glBufferData(target, size, data, usage);
}

GLvoid StubGLBufferSubData(GLenum target, GLint offset, GLsizei size,
                           const void* data) {
  glBufferSubData(target, offset, size, data);
}

GLenum StubGLCheckFramebufferStatus(GLenum target) {
  return glCheckFramebufferStatusEXT(target);
}

GLvoid StubGLClear(GLbitfield mask) {
  glClear(mask);
}

GLvoid StubGLClearColor(GLclampf red, GLclampf green, GLclampf blue,
                        GLclampf alpha) {
  glClearColor(red, green, blue, alpha);
}

GLvoid StubGLClearStencil(GLint s) {
  glClearStencil(s);
}

GLvoid StubGLColorMask(GLboolean red, GLboolean green, GLboolean blue,
                       GLboolean alpha) {
  glColorMask(red, green, blue, alpha);
}

GLvoid StubGLCompileShader(GLuint shader) {
  glCompileShader(shader);
}

GLvoid StubGLCompressedTexImage2D(GLenum target, GLint level,
                                  GLenum internalformat, GLsizei width,
                                  GLsizei height, GLint border,
                                  GLsizei imageSize, const void* data) {
  glCompressedTexImage2D(target, level, internalformat, width, height, border,
                         imageSize, data);
}

GLuint StubGLCreateProgram(void) {
  return glCreateProgram();
}

GLuint StubGLCreateShader(GLenum type) {
  return glCreateShader(type);
}

GLvoid StubGLCullFace(GLenum mode) {
  glCullFace(mode);
}

GLvoid StubGLDeleteBuffers(GLsizei n, const GLuint* buffers) {
  glDeleteBuffersARB(n, buffers);
}

GLvoid StubGLDeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
  glDeleteFramebuffersEXT(n, framebuffers);
}

GLvoid StubGLDeleteProgram(GLuint program) {
  glDeleteProgram(program);
}

GLvoid StubGLDeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) {
  glDeleteRenderbuffersEXT(n, renderbuffers);
}

GLvoid StubGLDeleteShader(GLuint shader) {
  glDeleteShader(shader);
}

GLvoid StubGLDeleteTextures(GLsizei n, const GLuint* textures) {
  glDeleteTextures(n, textures);
}

GLvoid StubGLDepthMask(GLboolean flag) {
  glDepthMask(flag);
}

GLvoid StubGLDisable(GLenum cap) {
  glDisable(cap);
}

GLvoid StubGLDisableVertexAttribArray(GLuint index) {
  glDisableVertexAttribArray(index);
}

GLvoid StubGLDrawArrays(GLenum mode, GLint first, GLsizei count) {
  glDrawArrays(mode, first, count);
}

GLvoid StubGLDrawElements(GLenum mode, GLsizei count, GLenum type,
                          const void* indices) {
  glDrawElements(mode, count, type, indices);
}

GLvoid StubGLEnable(GLenum cap) {
  glEnable(cap);
}

GLvoid StubGLEnableVertexAttribArray(GLuint index) {
  glEnableVertexAttribArray(index);
}

GLvoid StubGLFramebufferRenderbuffer(GLenum target, GLenum attachment,
                                     GLenum renderbuffertarget,
                                     GLuint renderbuffer) {
  glFramebufferRenderbufferEXT(target, attachment, renderbuffertarget,
                               renderbuffer);
}

GLvoid StubGLFramebufferTexture2D(GLenum target, GLenum attachment,
                                  GLenum textarget, GLuint texture,
                                  GLint level) {
  glFramebufferTexture2DEXT(target, attachment, textarget, texture, level);
}

GLvoid StubGLFrontFace(GLenum mode) {
  glFrontFace(mode);
}

GLvoid StubGLGenBuffers(GLsizei n, GLuint* buffers) {
  glGenBuffersARB(n, buffers);
}

GLvoid StubGLGenFramebuffers(GLsizei n, GLuint* framebuffers) {
  glGenFramebuffersEXT(n, framebuffers);
}

GLvoid StubGLGenRenderbuffers(GLsizei n, GLuint* renderbuffers) {
  glGenRenderbuffersEXT(n, renderbuffers);
}

GLvoid StubGLGenTextures(GLsizei n, GLuint* textures) {
  glGenTextures(n, textures);
}

GLvoid StubGLGetBufferParameteriv(GLenum target, GLenum pname, GLint* params) {
  glGetBufferParameteriv(target, pname, params);
}

GLenum StubGLGetError() {
  return glGetError();
}

GLvoid StubGLGetIntegerv(GLenum pname, GLint* params) {
  glGetIntegerv(pname, params);
}

GLvoid StubGLGetProgramInfoLog(GLuint program, GLsizei bufsize, GLsizei* length,
                               char* infolog) {
  glGetProgramInfoLog(program, bufsize, length, infolog);
}

GLvoid StubGLGetProgramiv(GLuint program, GLenum pname, GLint* params) {
  glGetProgramiv(program, pname, params);
}

GLvoid StubGLGetShaderInfoLog(GLuint shader, GLsizei bufsize, GLsizei* length,
                              char* infolog) {
  glGetShaderInfoLog(shader, bufsize, length, infolog);
}

GLvoid StubGLGetShaderiv(GLuint shader, GLenum pname, GLint* params) {
  glGetShaderiv(shader, pname, params);
}

const GLubyte* StubGLGetString(GLenum name) {
  return glGetString(name);
}

GLint StubGLGetUniformLocation(GLuint program, const char* name) {
  return glGetUniformLocation(program, name);
}

GLvoid StubGLLineWidth(GLfloat width) {
  glLineWidth(width);
}

GLvoid StubGLLinkProgram(GLuint program) {
  glLinkProgram(program);
}

void* StubGLMapBuffer(GLenum target, GLenum access) {
  return glMapBuffer(target, access);
}

GLvoid StubGLPixelStorei(GLenum pname, GLint param) {
  glPixelStorei(pname, param);
}

GLvoid StubGLReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                        GLenum format, GLenum type, void* pixels) {
  glReadPixels(x, y, width, height, format, type, pixels);
}

GLvoid StubGLRenderBufferStorage(GLenum target, GLenum internalformat,
                                 GLsizei width, GLsizei height) {
  glRenderbufferStorageEXT(target, internalformat, width, height);
}

GLvoid StubGLRenderbufferStorageMultisample(GLenum target, GLsizei samples,
                                            GLenum internalformat,
                                            GLsizei width, GLsizei height) {
  glRenderbufferStorageMultisampleEXT(target, samples, internalformat, width,
                                      height);
}

GLvoid StubGLScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
  glScissor(x, y, width, height);
}

GLvoid StubGLShaderSource(GLuint shader, GLsizei count, const char** str,
                          const GLint* length) {
  glShaderSource(shader, count, str, length);
}

GLvoid StubGLStencilFunc(GLenum func, GLint ref, GLuint mask) {
  glStencilFunc(func, ref, mask);
}

GLvoid StubGLStencilFuncSeparate(GLenum face, GLenum func, GLint ref,
                                 GLuint mask) {
  glStencilFuncSeparate(face, func, ref, mask);
}

GLvoid StubGLStencilMask(GLuint mask) {
  glStencilMask(mask);
}

GLvoid StubGLStencilMaskSeparate(GLenum face, GLuint mask) {
  glStencilMaskSeparate(face, mask);
}

GLvoid StubGLStencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
  glStencilOp(fail, zfail, zpass);
}

GLvoid StubGLStencilOpSeparate(GLenum face, GLenum fail, GLenum zfail,
                               GLenum zpass) {
  glStencilOpSeparate(face, fail, zfail, zpass);
}

GLvoid StubGLTexImage2D(GLenum target, GLint level, GLint internalformat,
                        GLsizei width, GLsizei height, GLint border,
                        GLenum format, GLenum type, const void* pixels) {
  glTexImage2D(target, level, internalformat, width, height, border, format,
               type, pixels);
}

GLvoid StubGLTexParameteri(GLenum target, GLenum pname, GLint param) {
  glTexParameteri(target, pname, param);
}

GLvoid StubGLTexSubImage2D(GLenum target, GLint level, GLint xoffset,
                           GLint yoffset, GLsizei width, GLsizei height,
                           GLenum format, GLenum type, const void* pixels) {
  glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type,
                  pixels);
}

GLvoid StubGLUniform1fv(GLint location, GLsizei count, const GLfloat* v) {
  glUniform1fv(location, count, v);
}

GLvoid StubGLUniform1i(GLint location, GLint x) {
  glUniform1i(location, x);
}

GLvoid StubGLUniform4fv(GLint location, GLsizei count, const GLfloat* v) {
  glUniform4fv(location, count, v);
}

GLvoid StubGLUniformMatrix3fv(GLint location, GLsizei count,
                              GLboolean transpose, const GLfloat* value) {
  glUniformMatrix3fv(location, count, transpose, value);
}

GLboolean StubGLUnmapBuffer(GLenum target) {
  return glUnmapBuffer(target);
}

GLvoid StubGLUseProgram(GLuint program) {
  glUseProgram(program);
}

GLvoid StubGLVertexAttrib4fv(GLuint indx, const GLfloat* values) {
  glVertexAttrib4fv(indx, values);
}

GLvoid StubGLVertexAttribPointer(GLuint indx, GLint size, GLenum type,
                                 GLboolean normalized, GLsizei stride,
                                 const void* ptr) {
  glVertexAttribPointer(indx, size, type, normalized, stride, ptr);
}

GLvoid StubGLViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
  glViewport(x, y, width, height);
}

}  // extern "C"

// Populate |gl_interface| with pointers to the GL implementation used by
// Chrome.
void InitializeGrGLInterface(GrGLInterface* gl_interface) {

  // Propagate the type of GL bindings exported back to skia.
  switch (gfx::GetGLImplementation()) {
    case gfx::kGLImplementationNone:
      NOTREACHED();
      break;
    case gfx::kGLImplementationDesktopGL:
      gl_interface->fBindingsExported = kDesktop_GrGLBinding;
      break;
    case gfx::kGLImplementationOSMesaGL:
      gl_interface->fBindingsExported = kDesktop_GrGLBinding;
      break;
    case gfx::kGLImplementationEGLGLES2:
      gl_interface->fBindingsExported = kES2_GrGLBinding;
      break;
    case gfx::kGLImplementationMockGL:
      NOTREACHED();
      break;
  }

  gl_interface->fClientActiveTexture = NULL;
  gl_interface->fColor4ub = NULL;
  gl_interface->fColorPointer = NULL;
  gl_interface->fDisableClientState = NULL;
  gl_interface->fEnableClientState = NULL;
  gl_interface->fLoadMatrixf = NULL;
  gl_interface->fMatrixMode = NULL;
  gl_interface->fPointSize = NULL;
  gl_interface->fShadeModel = NULL;
  gl_interface->fTexCoordPointer = NULL;
  gl_interface->fTexEnvi = NULL;
  gl_interface->fVertexPointer = NULL;

  gl_interface->fFramebufferTexture2DMultisample = NULL;
  gl_interface->fResolveMultisampleFramebuffer = NULL;
  gl_interface->fActiveTexture = StubGLActiveTexture;
  gl_interface->fAttachShader = StubGLAttachShader;
  gl_interface->fBindAttribLocation = StubGLBindAttribLocation;
  gl_interface->fBindBuffer = StubGLBindBuffer;
  gl_interface->fBindTexture = StubGLBindTexture;
  gl_interface->fBlendColor = StubGLBlendColor;
  gl_interface->fBlendFunc = StubGLBlendFunc;
  gl_interface->fBufferData = StubGLBufferData;
  gl_interface->fBufferSubData = StubGLBufferSubData;
  gl_interface->fClear = StubGLClear;
  gl_interface->fClearColor = StubGLClearColor;
  gl_interface->fClearStencil = StubGLClearStencil;
  gl_interface->fColorMask = StubGLColorMask;
  gl_interface->fCompileShader = StubGLCompileShader;
  gl_interface->fCompressedTexImage2D = StubGLCompressedTexImage2D;
  gl_interface->fCreateProgram = StubGLCreateProgram;
  gl_interface->fCreateShader = StubGLCreateShader;
  gl_interface->fCullFace = StubGLCullFace;
  gl_interface->fDeleteBuffers = StubGLDeleteBuffers;
  gl_interface->fDeleteProgram = StubGLDeleteProgram;
  gl_interface->fDeleteShader = StubGLDeleteShader;
  gl_interface->fDeleteTextures = StubGLDeleteTextures;
  gl_interface->fDepthMask = StubGLDepthMask;
  gl_interface->fDisable = StubGLDisable;
  gl_interface->fDisableVertexAttribArray = StubGLDisableVertexAttribArray;
  gl_interface->fDrawArrays = StubGLDrawArrays;
  gl_interface->fDrawElements = StubGLDrawElements;
  gl_interface->fEnable = StubGLEnable;
  gl_interface->fEnableVertexAttribArray = StubGLEnableVertexAttribArray;
  gl_interface->fFrontFace = StubGLFrontFace;
  gl_interface->fGenBuffers = StubGLGenBuffers;
  gl_interface->fGenTextures = StubGLGenTextures;
  gl_interface->fGetBufferParameteriv = StubGLGetBufferParameteriv;
  gl_interface->fGetError = StubGLGetError;
  gl_interface->fGetIntegerv = StubGLGetIntegerv;
  gl_interface->fGetProgramInfoLog = StubGLGetProgramInfoLog;
  gl_interface->fGetProgramiv = StubGLGetProgramiv;
  gl_interface->fGetShaderInfoLog = StubGLGetShaderInfoLog;
  gl_interface->fGetShaderiv = StubGLGetShaderiv;
  gl_interface->fGetString = StubGLGetString;
  gl_interface->fGetUniformLocation = StubGLGetUniformLocation;
  gl_interface->fLineWidth = StubGLLineWidth;
  gl_interface->fLinkProgram = StubGLLinkProgram;
  gl_interface->fPixelStorei = StubGLPixelStorei;
  gl_interface->fReadPixels = StubGLReadPixels;
  gl_interface->fScissor = StubGLScissor;
  gl_interface->fShaderSource = StubGLShaderSource;
  gl_interface->fStencilFunc = StubGLStencilFunc;
  gl_interface->fStencilFuncSeparate = StubGLStencilFuncSeparate;
  gl_interface->fStencilMask = StubGLStencilMask;
  gl_interface->fStencilMaskSeparate = StubGLStencilMaskSeparate;
  gl_interface->fStencilOp = StubGLStencilOp;
  gl_interface->fStencilOpSeparate = StubGLStencilOpSeparate;
  gl_interface->fTexImage2D = StubGLTexImage2D;
  gl_interface->fTexParameteri = StubGLTexParameteri;
  gl_interface->fTexSubImage2D = StubGLTexSubImage2D;
  gl_interface->fUniform1fv = StubGLUniform1fv;
  gl_interface->fUniform1i = StubGLUniform1i;
  gl_interface->fUniform4fv = StubGLUniform4fv;
  gl_interface->fUniformMatrix3fv = StubGLUniformMatrix3fv;
  gl_interface->fUseProgram = StubGLUseProgram;
  gl_interface->fVertexAttrib4fv = StubGLVertexAttrib4fv;
  gl_interface->fVertexAttribPointer = StubGLVertexAttribPointer;
  gl_interface->fViewport = StubGLViewport;
  gl_interface->fBindFramebuffer = StubGLBindFramebuffer;
  gl_interface->fBindRenderbuffer = StubGLBindRenderbuffer;
  gl_interface->fCheckFramebufferStatus = StubGLCheckFramebufferStatus;
  gl_interface->fDeleteFramebuffers = StubGLDeleteFramebuffers;
  gl_interface->fDeleteRenderbuffers = StubGLDeleteRenderbuffers;
  gl_interface->fFramebufferRenderbuffer = StubGLFramebufferRenderbuffer;
  gl_interface->fFramebufferTexture2D = StubGLFramebufferTexture2D;
  gl_interface->fGenFramebuffers = StubGLGenFramebuffers;
  gl_interface->fGenRenderbuffers = StubGLGenRenderbuffers;
  gl_interface->fRenderbufferStorage = StubGLRenderBufferStorage;
  gl_interface->fRenderbufferStorageMultisample =
     StubGLRenderbufferStorageMultisample;
  gl_interface->fBlitFramebuffer = StubGLBlitFramebuffer;
  gl_interface->fMapBuffer = StubGLMapBuffer;
  gl_interface->fUnmapBuffer = StubGLUnmapBuffer;
}

}  // namespace

namespace gfx {

void BindSkiaToHostGL() {
  static GrGLInterface host_gl_interface;
  static bool host_StubGL_initialized = false;
  if (!host_StubGL_initialized) {
    InitializeGrGLInterface(&host_gl_interface);
    GrGLSetGLInterface(&host_gl_interface);
    host_StubGL_initialized = true;
  }
}

}  // namespace gfx
