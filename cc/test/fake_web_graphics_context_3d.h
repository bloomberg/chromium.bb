// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_WEB_GRAPHICS_CONTEXT_3D_H_
#define CC_TEST_FAKE_WEB_GRAPHICS_CONTEXT_3D_H_

#include <string>

#include "base/macros.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace cc {

// Base class for use in unit tests.
// All operations are no-ops (returning 0 if necessary).
class FakeWebGraphicsContext3D {
 public:
  FakeWebGraphicsContext3D();
  virtual ~FakeWebGraphicsContext3D();

  virtual bool makeContextCurrent();

  virtual bool isGLES2Compliant();

  virtual GLuint getPlatformTextureId();

  virtual void prepareTexture() {}

  virtual void postSubBufferCHROMIUM(int x, int y, int width, int height) {}

  virtual void synthesizeGLError(GLenum) {}

  virtual bool isContextLost();

  virtual void* mapBufferSubDataCHROMIUM(
      GLenum target,
      GLintptr offset,
      GLsizeiptr size,
      GLenum access);

  virtual void unmapBufferSubDataCHROMIUM(const void*) {}
  virtual void* mapTexSubImage2DCHROMIUM(
      GLenum target,
      GLint level,
      GLint xoffset,
      GLint yoffset,
      GLsizei width,
      GLsizei height,
      GLenum format,
      GLenum type,
      GLenum access);
  virtual void unmapTexSubImage2DCHROMIUM(const void*) {}

  virtual void setVisibilityCHROMIUM(bool visible) {}

  virtual void discardFramebufferEXT(
      GLenum target,
      GLsizei num_attachments,
      const GLenum* attachments) {}

  virtual void requestExtensionCHROMIUM(const char*) {}

  virtual void blitFramebufferCHROMIUM(
      GLint src_x0,
      GLint src_y0,
      GLint src_x1,
      GLint src_y1,
      GLint dst_x0,
      GLint dst_y0,
      GLint dst_x1,
      GLint dst_y1,
      GLbitfield mask,
      GLenum filter) {}
  virtual void renderbufferStorageMultisampleCHROMIUM(
      GLenum target,
      GLsizei samples,
      GLenum internalformat,
      GLsizei width,
      GLsizei height) {}

  virtual void activeTexture(GLenum texture) {}
  virtual void attachShader(GLuint program, GLuint shader);
  virtual void bindAttribLocation(
      GLuint program,
      GLuint index,
      const GLchar* name) {}
  virtual void bindBuffer(GLenum target, GLuint buffer);
  virtual void bindFramebuffer(
      GLenum target, GLuint framebuffer);
  virtual void bindRenderbuffer(
      GLenum target, GLuint renderbuffer);
  virtual void bindTexture(
      GLenum target,
      GLuint texture_id);
  virtual void blendColor(
      GLclampf red,
      GLclampf green,
      GLclampf blue,
      GLclampf alpha) {}
  virtual void blendEquation(GLenum mode) {}
  virtual void blendEquationSeparate(
      GLenum mode_rgb,
      GLenum mode_alpha) {}
  virtual void blendFunc(
      GLenum sfactor,
      GLenum dfactor) {}
  virtual void blendFuncSeparate(
      GLenum src_rgb,
      GLenum dst_rgb,
      GLenum src_alpha,
      GLenum dst_alpha) {}

  virtual void bufferData(
      GLenum target,
      GLsizeiptr size,
      const void* data,
      GLenum usage) {}
  virtual void bufferSubData(
      GLenum target,
      GLintptr offset,
      GLsizeiptr size,
      const void* data) {}

  virtual GLenum checkFramebufferStatus(GLenum target);

  virtual void clear(GLbitfield mask) {}
  virtual void clearColor(
      GLclampf red,
      GLclampf green,
      GLclampf blue,
      GLclampf alpha) {}
  virtual void clearDepth(GLclampf depth) {}
  virtual void clearStencil(GLint s) {}
  virtual void colorMask(
      GLboolean red,
      GLboolean green,
      GLboolean blue,
      GLboolean alpha) {}
  virtual void compileShader(GLuint shader) {}

  virtual void compressedTexImage2D(
      GLenum target,
      GLint level,
      GLenum internal_format,
      GLsizei width,
      GLsizei height,
      GLint border,
      GLsizei image_size,
      const void* data) {}
  virtual void compressedTexSubImage2D(
      GLenum target,
      GLint level,
      GLint xoffset,
      GLint yoffset,
      GLsizei width,
      GLsizei height,
      GLenum format,
      GLsizei image_size,
      const void* data) {}
  virtual void copyTexImage2D(
      GLenum target,
      GLint level,
      GLenum internalformat,
      GLint x,
      GLint y,
      GLsizei width,
      GLsizei height,
      GLint border) {}
  virtual void copyTexSubImage2D(
      GLenum target,
      GLint level,
      GLint xoffset,
      GLint yoffset,
      GLint x,
      GLint y,
      GLsizei width,
      GLsizei height) {}
  virtual void cullFace(GLenum mode) {}
  virtual void depthFunc(GLenum func) {}
  virtual void depthMask(GLboolean flag) {}
  virtual void depthRange(
      GLclampf z_near,
      GLclampf z_far) {}
  virtual void detachShader(GLuint program, GLuint shader) {}
  virtual void disable(GLenum cap) {}
  virtual void disableVertexAttribArray(GLuint index) {}
  virtual void drawArrays(
      GLenum mode,
      GLint first,
      GLsizei count) {}
  virtual void drawElements(
      GLenum mode,
      GLsizei count,
      GLenum type,
      GLintptr offset) {}

  virtual void enable(GLenum cap) {}
  virtual void enableVertexAttribArray(GLuint index) {}
  virtual void finish() {}
  virtual void flush() {}
  virtual void shallowFlushCHROMIUM() {}
  virtual void framebufferRenderbuffer(
      GLenum target,
      GLenum attachment,
      GLenum renderbuffertarget,
      GLuint renderbuffer) {}
  virtual void framebufferTexture2D(
      GLenum target,
      GLenum attachment,
      GLenum textarget,
      GLuint texture,
      GLint level) {}
  virtual void frontFace(GLenum mode) {}
  virtual void generateMipmap(GLenum target) {}

  virtual void getAttachedShaders(
      GLuint program,
      GLsizei max_count,
      GLsizei* count,
      GLuint* shaders) {}
  virtual GLint getAttribLocation(
      GLuint program,
      const GLchar* name);
  virtual void getBooleanv(
      GLenum pname,
      GLboolean* value) {}
  virtual void getBufferParameteriv(
      GLenum target,
      GLenum pname,
      GLint* value) {}
  virtual GLenum getError();
  virtual void getFloatv(
      GLenum pname,
      GLfloat* value) {}
  virtual void getFramebufferAttachmentParameteriv(
      GLenum target,
      GLenum attachment,
      GLenum pname,
      GLint* value) {}

  virtual void getIntegerv(
      GLenum pname,
      GLint* value);

  virtual void getProgramiv(
      GLuint program,
      GLenum pname,
      GLint* value);

  virtual void getRenderbufferParameteriv(
      GLenum target,
      GLenum pname,
      GLint* value) {}

  virtual void getShaderiv(
      GLuint shader,
      GLenum pname,
      GLint* value);

  virtual void getShaderPrecisionFormat(
      GLenum shadertype,
      GLenum precisiontype,
      GLint* range,
      GLint* precision);
  virtual void getTexParameterfv(
      GLenum target,
      GLenum pname,
      GLfloat* value) {}
  virtual void getTexParameteriv(
      GLenum target,
      GLenum pname,
      GLint* value) {}
  virtual void getUniformfv(
      GLuint program,
      GLint location,
      GLfloat* value) {}
  virtual void getUniformiv(
      GLuint program,
      GLint location,
      GLint* value) {}
  virtual GLint getUniformLocation(
      GLuint program,
      const GLchar* name);
  virtual void getVertexAttribfv(
      GLuint index,
      GLenum pname,
      GLfloat* value) {}
  virtual void getVertexAttribiv(
      GLuint index,
      GLenum pname,
      GLint* value) {}
  virtual GLsizeiptr getVertexAttribOffset(
      GLuint index,
      GLenum pname);

  virtual void hint(GLenum target, GLenum mode) {}
  virtual GLboolean isBuffer(GLuint buffer);
  virtual GLboolean isEnabled(GLenum cap);
  virtual GLboolean isFramebuffer(GLuint framebuffer);
  virtual GLboolean isProgram(GLuint program);
  virtual GLboolean isRenderbuffer(GLuint renderbuffer);
  virtual GLboolean isShader(GLuint shader);
  virtual GLboolean isTexture(GLuint texture);
  virtual void lineWidth(GLfloat) {}
  virtual void linkProgram(GLuint program) {}
  virtual void pixelStorei(GLenum pname, GLint param) {}
  virtual void polygonOffset(
      GLfloat factor,
      GLfloat units) {}

  virtual void readPixels(
      GLint x,
      GLint y,
      GLsizei width,
      GLsizei height,
      GLenum format,
      GLenum type,
      void* pixels) {}

  virtual void releaseShaderCompiler() {}

  virtual void renderbufferStorage(
      GLenum target,
      GLenum internalformat,
      GLsizei width,
      GLsizei height) {}
  virtual void sampleCoverage(
      GLclampf value,
      GLboolean invert) {}
  virtual void scissor(
      GLint x,
      GLint y,
      GLsizei width,
      GLsizei height) {}
  virtual void shaderSource(
      GLuint shader,
      const GLchar* string) {}
  virtual void stencilFunc(
      GLenum func,
      GLint ref,
      GLuint mask) {}
  virtual void stencilFuncSeparate(
      GLenum face,
      GLenum func,
      GLint ref,
      GLuint mask) {}
  virtual void stencilMask(GLuint mask) {}
  virtual void stencilMaskSeparate(
      GLenum face,
      GLuint mask) {}
  virtual void stencilOp(
      GLenum fail,
      GLenum zfail,
      GLenum zpass) {}
  virtual void stencilOpSeparate(
      GLenum face,
      GLenum fail,
      GLenum zfail,
      GLenum zpass) {}

  virtual void texImage2D(
      GLenum target,
      GLint level,
      GLenum internalformat,
      GLsizei width,
      GLsizei height,
      GLint border,
      GLenum format,
      GLenum type,
      const void* pixels) {}

  virtual void texParameterf(
      GLenum target,
      GLenum pname,
      GLfloat param) {}
  virtual void texParameteri(
      GLenum target,
      GLenum pname,
      GLint param) {}

  virtual void texSubImage2D(
      GLenum target,
      GLint level,
      GLint xoffset,
      GLint yoffset,
      GLsizei width,
      GLsizei height,
      GLenum format,
      GLenum type,
      const void* pixels) {}

  virtual void uniform1f(GLint location, GLfloat x) {}
  virtual void uniform1fv(
      GLint location,
      GLsizei count,
      const GLfloat* v) {}
  virtual void uniform1i(GLint location, GLint x) {}
  virtual void uniform1iv(
      GLint location,
      GLsizei count,
      const GLint* v) {}
  virtual void uniform2f(
      GLint location,
      GLfloat x,
      GLfloat y) {}
  virtual void uniform2fv(
      GLint location,
      GLsizei count,
      const GLfloat* v) {}
  virtual void uniform2i(
      GLint location,
      GLint x,
      GLint y) {}
  virtual void uniform2iv(
      GLint location,
      GLsizei count,
      const GLint* v) {}
  virtual void uniform3f(
      GLint location,
      GLfloat x,
      GLfloat y,
      GLfloat z) {}
  virtual void uniform3fv(
      GLint location,
      GLsizei count,
      const GLfloat* v) {}
  virtual void uniform3i(
      GLint location,
      GLint x,
      GLint y,
      GLint z) {}
  virtual void uniform3iv(
      GLint location,
      GLsizei count,
      const GLint* v) {}
  virtual void uniform4f(
      GLint location,
      GLfloat x,
      GLfloat y,
      GLfloat z,
      GLfloat w) {}
  virtual void uniform4fv(
      GLint location,
      GLsizei count,
      const GLfloat* v) {}
  virtual void uniform4i(
      GLint location,
      GLint x,
      GLint y,
      GLint z,
      GLint w) {}
  virtual void uniform4iv(
      GLint location,
      GLsizei count,
      const GLint* v) {}
  virtual void uniformMatrix2fv(
      GLint location,
      GLsizei count,
      GLboolean transpose,
      const GLfloat* value) {}
  virtual void uniformMatrix3fv(
      GLint location,
      GLsizei count,
      GLboolean transpose,
      const GLfloat* value) {}
  virtual void uniformMatrix4fv(
      GLint location,
      GLsizei count,
      GLboolean transpose,
      const GLfloat* value) {}

  virtual void useProgram(GLuint program);
  virtual void validateProgram(GLuint program) {}

  virtual void vertexAttrib1f(GLuint index, GLfloat x) {}
  virtual void vertexAttrib1fv(
      GLuint index,
      const GLfloat* values) {}
  virtual void vertexAttrib2f(
      GLuint index,
      GLfloat x,
      GLfloat y) {}
  virtual void vertexAttrib2fv(
      GLuint index,
      const GLfloat* values) {}
  virtual void vertexAttrib3f(
      GLuint index,
      GLfloat x,
      GLfloat y,
      GLfloat z) {}
  virtual void vertexAttrib3fv(
      GLuint index,
      const GLfloat* values) {}
  virtual void vertexAttrib4f(
      GLuint index,
      GLfloat x,
      GLfloat y,
      GLfloat z,
      GLfloat w) {}
  virtual void vertexAttrib4fv(
      GLuint index,
      const GLfloat* values) {}
  virtual void vertexAttribPointer(
      GLuint index,
      GLint size,
      GLenum type,
      GLboolean normalized,
      GLsizei stride,
      GLintptr offset) {}

  virtual void viewport(
      GLint x,
      GLint y,
      GLsizei width,
      GLsizei height) {}

  virtual void genBuffers(GLsizei count, GLuint* ids);
  virtual void genFramebuffers(GLsizei count, GLuint* ids);
  virtual void genRenderbuffers(GLsizei count, GLuint* ids);
  virtual void genTextures(GLsizei count, GLuint* ids);

  virtual void deleteBuffers(GLsizei count, GLuint* ids);
  virtual void deleteFramebuffers(
      GLsizei count, GLuint* ids);
  virtual void deleteRenderbuffers(
      GLsizei count, GLuint* ids);
  virtual void deleteTextures(GLsizei count, GLuint* ids);

  virtual GLuint createBuffer();
  virtual GLuint createFramebuffer();
  virtual GLuint createRenderbuffer();
  virtual GLuint createTexture();

  virtual void deleteBuffer(GLuint id);
  virtual void deleteFramebuffer(GLuint id);
  virtual void deleteRenderbuffer(GLuint id);
  virtual void deleteTexture(GLuint id);

  virtual GLuint createProgram();
  virtual GLuint createShader(GLenum);

  virtual void deleteProgram(GLuint id);
  virtual void deleteShader(GLuint id);

  virtual void texStorage2DEXT(
      GLenum target,
      GLint levels,
      GLuint internalformat,
      GLint width,
      GLint height) {}

  virtual GLuint createQueryEXT();
  virtual void deleteQueryEXT(GLuint query) {}
  virtual GLboolean isQueryEXT(GLuint query);
  virtual void beginQueryEXT(
      GLenum target,
      GLuint query) {}
  virtual void endQueryEXT(GLenum target);
  virtual void getQueryivEXT(
      GLenum target,
      GLenum pname,
      GLint* params) {}
  virtual void getQueryObjectuivEXT(
      GLuint query,
      GLenum pname,
      GLuint* params);

  virtual void loseContextCHROMIUM(GLenum current,
                                   GLenum other);

  virtual void drawBuffersEXT(GLsizei m,
                              const GLenum* bufs) {}

  virtual void bindTexImage2DCHROMIUM(GLenum target,
                                      GLint image_id) {}

  // GL_CHROMIUM_gpu_memory_buffer
  virtual GLuint createImageCHROMIUM(
      GLsizei width,
      GLsizei height,
      GLenum internalformat);
  virtual void destroyImageCHROMIUM(GLuint image_id) {}
  virtual void getImageParameterivCHROMIUM(
      GLuint image_id,
      GLenum pname,
      GLint* params) {}
  virtual void* mapImageCHROMIUM(
      GLuint image_id,
      GLenum access);
  virtual void unmapImageCHROMIUM(GLuint image_id) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeWebGraphicsContext3D);
};

}  // namespace cc

#endif  // CC_TEST_FAKE_WEB_GRAPHICS_CONTEXT_3D_H_
