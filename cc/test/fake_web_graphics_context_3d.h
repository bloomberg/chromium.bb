// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_WEB_GRAPHICS_CONTEXT_3D_H_
#define CC_TEST_FAKE_WEB_GRAPHICS_CONTEXT_3D_H_

#include <string>

#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"

namespace cc {

// WebGraphicsContext3D base class for use in unit tests.
// All operations are no-ops (returning 0 if necessary).
class FakeWebGraphicsContext3D : public blink::WebGraphicsContext3D {
 public:
  FakeWebGraphicsContext3D();
  virtual ~FakeWebGraphicsContext3D();

  virtual bool makeContextCurrent();

  virtual bool isGLES2Compliant();

  virtual blink::WebGLId getPlatformTextureId();

  virtual void prepareTexture() {}

  virtual void postSubBufferCHROMIUM(int x, int y, int width, int height) {}

  virtual void synthesizeGLError(blink::WGC3Denum) {}

  virtual bool isContextLost();

  virtual void* mapBufferSubDataCHROMIUM(
      blink::WGC3Denum target,
      blink::WGC3Dintptr offset,
      blink::WGC3Dsizeiptr size,
      blink::WGC3Denum access);

  virtual void unmapBufferSubDataCHROMIUM(const void*) {}
  virtual void* mapTexSubImage2DCHROMIUM(
      blink::WGC3Denum target,
      blink::WGC3Dint level,
      blink::WGC3Dint xoffset,
      blink::WGC3Dint yoffset,
      blink::WGC3Dsizei width,
      blink::WGC3Dsizei height,
      blink::WGC3Denum format,
      blink::WGC3Denum type,
      blink::WGC3Denum access);
  virtual void unmapTexSubImage2DCHROMIUM(const void*) {}

  virtual void setVisibilityCHROMIUM(bool visible) {}

  virtual void discardFramebufferEXT(
      blink::WGC3Denum target,
      blink::WGC3Dsizei num_attachments,
      const blink::WGC3Denum* attachments) {}

  virtual blink::WebString getRequestableExtensionsCHROMIUM();
  virtual void requestExtensionCHROMIUM(const char*) {}

  virtual void blitFramebufferCHROMIUM(
      blink::WGC3Dint src_x0,
      blink::WGC3Dint src_y0,
      blink::WGC3Dint src_x1,
      blink::WGC3Dint src_y1,
      blink::WGC3Dint dst_x0,
      blink::WGC3Dint dst_y0,
      blink::WGC3Dint dst_x1,
      blink::WGC3Dint dst_y1,
      blink::WGC3Dbitfield mask,
      blink::WGC3Denum filter) {}
  virtual void renderbufferStorageMultisampleCHROMIUM(
      blink::WGC3Denum target,
      blink::WGC3Dsizei samples,
      blink::WGC3Denum internalformat,
      blink::WGC3Dsizei width,
      blink::WGC3Dsizei height) {}

  virtual void activeTexture(blink::WGC3Denum texture) {}
  virtual void attachShader(blink::WebGLId program, blink::WebGLId shader);
  virtual void bindAttribLocation(
      blink::WebGLId program,
      blink::WGC3Duint index,
      const blink::WGC3Dchar* name) {}
  virtual void bindBuffer(blink::WGC3Denum target, blink::WebGLId buffer);
  virtual void bindFramebuffer(
      blink::WGC3Denum target, blink::WebGLId framebuffer);
  virtual void bindRenderbuffer(
      blink::WGC3Denum target, blink::WebGLId renderbuffer);
  virtual void bindTexture(
      blink::WGC3Denum target,
      blink::WebGLId texture_id);
  virtual void blendColor(
      blink::WGC3Dclampf red,
      blink::WGC3Dclampf green,
      blink::WGC3Dclampf blue,
      blink::WGC3Dclampf alpha) {}
  virtual void blendEquation(blink::WGC3Denum mode) {}
  virtual void blendEquationSeparate(
      blink::WGC3Denum mode_rgb,
      blink::WGC3Denum mode_alpha) {}
  virtual void blendFunc(
      blink::WGC3Denum sfactor,
      blink::WGC3Denum dfactor) {}
  virtual void blendFuncSeparate(
      blink::WGC3Denum src_rgb,
      blink::WGC3Denum dst_rgb,
      blink::WGC3Denum src_alpha,
      blink::WGC3Denum dst_alpha) {}

  virtual void bufferData(
      blink::WGC3Denum target,
      blink::WGC3Dsizeiptr size,
      const void* data,
      blink::WGC3Denum usage) {}
  virtual void bufferSubData(
      blink::WGC3Denum target,
      blink::WGC3Dintptr offset,
      blink::WGC3Dsizeiptr size,
      const void* data) {}

  virtual blink::WGC3Denum checkFramebufferStatus(blink::WGC3Denum target);

  virtual void clear(blink::WGC3Dbitfield mask) {}
  virtual void clearColor(
      blink::WGC3Dclampf red,
      blink::WGC3Dclampf green,
      blink::WGC3Dclampf blue,
      blink::WGC3Dclampf alpha) {}
  virtual void clearDepth(blink::WGC3Dclampf depth) {}
  virtual void clearStencil(blink::WGC3Dint s) {}
  virtual void colorMask(
      blink::WGC3Dboolean red,
      blink::WGC3Dboolean green,
      blink::WGC3Dboolean blue,
      blink::WGC3Dboolean alpha) {}
  virtual void compileShader(blink::WebGLId shader) {}

  virtual void compressedTexImage2D(
      blink::WGC3Denum target,
      blink::WGC3Dint level,
      blink::WGC3Denum internal_format,
      blink::WGC3Dsizei width,
      blink::WGC3Dsizei height,
      blink::WGC3Dint border,
      blink::WGC3Dsizei image_size,
      const void* data) {}
  virtual void compressedTexSubImage2D(
      blink::WGC3Denum target,
      blink::WGC3Dint level,
      blink::WGC3Dint xoffset,
      blink::WGC3Dint yoffset,
      blink::WGC3Dsizei width,
      blink::WGC3Dsizei height,
      blink::WGC3Denum format,
      blink::WGC3Dsizei image_size,
      const void* data) {}
  virtual void copyTexImage2D(
      blink::WGC3Denum target,
      blink::WGC3Dint level,
      blink::WGC3Denum internalformat,
      blink::WGC3Dint x,
      blink::WGC3Dint y,
      blink::WGC3Dsizei width,
      blink::WGC3Dsizei height,
      blink::WGC3Dint border) {}
  virtual void copyTexSubImage2D(
      blink::WGC3Denum target,
      blink::WGC3Dint level,
      blink::WGC3Dint xoffset,
      blink::WGC3Dint yoffset,
      blink::WGC3Dint x,
      blink::WGC3Dint y,
      blink::WGC3Dsizei width,
      blink::WGC3Dsizei height) {}
  virtual void cullFace(blink::WGC3Denum mode) {}
  virtual void depthFunc(blink::WGC3Denum func) {}
  virtual void depthMask(blink::WGC3Dboolean flag) {}
  virtual void depthRange(
      blink::WGC3Dclampf z_near,
      blink::WGC3Dclampf z_far) {}
  virtual void detachShader(blink::WebGLId program, blink::WebGLId shader) {}
  virtual void disable(blink::WGC3Denum cap) {}
  virtual void disableVertexAttribArray(blink::WGC3Duint index) {}
  virtual void drawArrays(
      blink::WGC3Denum mode,
      blink::WGC3Dint first,
      blink::WGC3Dsizei count) {}
  virtual void drawElements(
      blink::WGC3Denum mode,
      blink::WGC3Dsizei count,
      blink::WGC3Denum type,
      blink::WGC3Dintptr offset) {}

  virtual void enable(blink::WGC3Denum cap) {}
  virtual void enableVertexAttribArray(blink::WGC3Duint index) {}
  virtual void finish() {}
  virtual void flush() {}
  virtual void framebufferRenderbuffer(
      blink::WGC3Denum target,
      blink::WGC3Denum attachment,
      blink::WGC3Denum renderbuffertarget,
      blink::WebGLId renderbuffer) {}
  virtual void framebufferTexture2D(
      blink::WGC3Denum target,
      blink::WGC3Denum attachment,
      blink::WGC3Denum textarget,
      blink::WebGLId texture,
      blink::WGC3Dint level) {}
  virtual void frontFace(blink::WGC3Denum mode) {}
  virtual void generateMipmap(blink::WGC3Denum target) {}

  virtual bool getActiveAttrib(
      blink::WebGLId program,
      blink::WGC3Duint index, ActiveInfo&);
  virtual bool getActiveUniform(
      blink::WebGLId program,
      blink::WGC3Duint index,
      ActiveInfo&);
  virtual void getAttachedShaders(
      blink::WebGLId program,
      blink::WGC3Dsizei max_count,
      blink::WGC3Dsizei* count,
      blink::WebGLId* shaders) {}
  virtual blink::WGC3Dint getAttribLocation(
      blink::WebGLId program,
      const blink::WGC3Dchar* name);
  virtual void getBooleanv(
      blink::WGC3Denum pname,
      blink::WGC3Dboolean* value) {}
  virtual void getBufferParameteriv(
      blink::WGC3Denum target,
      blink::WGC3Denum pname,
      blink::WGC3Dint* value) {}
  virtual Attributes getContextAttributes();
  virtual blink::WGC3Denum getError();
  virtual void getFloatv(
      blink::WGC3Denum pname,
      blink::WGC3Dfloat* value) {}
  virtual void getFramebufferAttachmentParameteriv(
      blink::WGC3Denum target,
      blink::WGC3Denum attachment,
      blink::WGC3Denum pname,
      blink::WGC3Dint* value) {}

  virtual void getIntegerv(
      blink::WGC3Denum pname,
      blink::WGC3Dint* value);

  virtual void getProgramiv(
      blink::WebGLId program,
      blink::WGC3Denum pname,
      blink::WGC3Dint* value);

  virtual blink::WebString getProgramInfoLog(blink::WebGLId program);
  virtual void getRenderbufferParameteriv(
      blink::WGC3Denum target,
      blink::WGC3Denum pname,
      blink::WGC3Dint* value) {}

  virtual void getShaderiv(
      blink::WebGLId shader,
      blink::WGC3Denum pname,
      blink::WGC3Dint* value);

  virtual blink::WebString getShaderInfoLog(blink::WebGLId shader);
  virtual void getShaderPrecisionFormat(
      blink::WGC3Denum shadertype,
      blink::WGC3Denum precisiontype,
      blink::WGC3Dint* range,
      blink::WGC3Dint* precision);
  virtual blink::WebString getShaderSource(blink::WebGLId shader);
  virtual blink::WebString getString(blink::WGC3Denum name);
  virtual void getTexParameterfv(
      blink::WGC3Denum target,
      blink::WGC3Denum pname,
      blink::WGC3Dfloat* value) {}
  virtual void getTexParameteriv(
      blink::WGC3Denum target,
      blink::WGC3Denum pname,
      blink::WGC3Dint* value) {}
  virtual void getUniformfv(
      blink::WebGLId program,
      blink::WGC3Dint location,
      blink::WGC3Dfloat* value) {}
  virtual void getUniformiv(
      blink::WebGLId program,
      blink::WGC3Dint location,
      blink::WGC3Dint* value) {}
  virtual blink::WGC3Dint getUniformLocation(
      blink::WebGLId program,
      const blink::WGC3Dchar* name);
  virtual void getVertexAttribfv(
      blink::WGC3Duint index,
      blink::WGC3Denum pname,
      blink::WGC3Dfloat* value) {}
  virtual void getVertexAttribiv(
      blink::WGC3Duint index,
      blink::WGC3Denum pname,
      blink::WGC3Dint* value) {}
  virtual blink::WGC3Dsizeiptr getVertexAttribOffset(
      blink::WGC3Duint index,
      blink::WGC3Denum pname);

  virtual void hint(blink::WGC3Denum target, blink::WGC3Denum mode) {}
  virtual blink::WGC3Dboolean isBuffer(blink::WebGLId buffer);
  virtual blink::WGC3Dboolean isEnabled(blink::WGC3Denum cap);
  virtual blink::WGC3Dboolean isFramebuffer(blink::WebGLId framebuffer);
  virtual blink::WGC3Dboolean isProgram(blink::WebGLId program);
  virtual blink::WGC3Dboolean isRenderbuffer(blink::WebGLId renderbuffer);
  virtual blink::WGC3Dboolean isShader(blink::WebGLId shader);
  virtual blink::WGC3Dboolean isTexture(blink::WebGLId texture);
  virtual void lineWidth(blink::WGC3Dfloat) {}
  virtual void linkProgram(blink::WebGLId program) {}
  virtual void pixelStorei(blink::WGC3Denum pname, blink::WGC3Dint param) {}
  virtual void polygonOffset(
      blink::WGC3Dfloat factor,
      blink::WGC3Dfloat units) {}

  virtual void readPixels(
      blink::WGC3Dint x,
      blink::WGC3Dint y,
      blink::WGC3Dsizei width,
      blink::WGC3Dsizei height,
      blink::WGC3Denum format,
      blink::WGC3Denum type,
      void* pixels) {}

  virtual void releaseShaderCompiler() {}

  virtual void renderbufferStorage(
      blink::WGC3Denum target,
      blink::WGC3Denum internalformat,
      blink::WGC3Dsizei width,
      blink::WGC3Dsizei height) {}
  virtual void sampleCoverage(
      blink::WGC3Dclampf value,
      blink::WGC3Dboolean invert) {}
  virtual void scissor(
      blink::WGC3Dint x,
      blink::WGC3Dint y,
      blink::WGC3Dsizei width,
      blink::WGC3Dsizei height) {}
  virtual void shaderSource(
      blink::WebGLId shader,
      const blink::WGC3Dchar* string) {}
  virtual void stencilFunc(
      blink::WGC3Denum func,
      blink::WGC3Dint ref,
      blink::WGC3Duint mask) {}
  virtual void stencilFuncSeparate(
      blink::WGC3Denum face,
      blink::WGC3Denum func,
      blink::WGC3Dint ref,
      blink::WGC3Duint mask) {}
  virtual void stencilMask(blink::WGC3Duint mask) {}
  virtual void stencilMaskSeparate(
      blink::WGC3Denum face,
      blink::WGC3Duint mask) {}
  virtual void stencilOp(
      blink::WGC3Denum fail,
      blink::WGC3Denum zfail,
      blink::WGC3Denum zpass) {}
  virtual void stencilOpSeparate(
      blink::WGC3Denum face,
      blink::WGC3Denum fail,
      blink::WGC3Denum zfail,
      blink::WGC3Denum zpass) {}

  virtual void texImage2D(
      blink::WGC3Denum target,
      blink::WGC3Dint level,
      blink::WGC3Denum internalformat,
      blink::WGC3Dsizei width,
      blink::WGC3Dsizei height,
      blink::WGC3Dint border,
      blink::WGC3Denum format,
      blink::WGC3Denum type,
      const void* pixels) {}

  virtual void texParameterf(
      blink::WGC3Denum target,
      blink::WGC3Denum pname,
      blink::WGC3Dfloat param) {}
  virtual void texParameteri(
      blink::WGC3Denum target,
      blink::WGC3Denum pname,
      blink::WGC3Dint param) {}

  virtual void texSubImage2D(
      blink::WGC3Denum target,
      blink::WGC3Dint level,
      blink::WGC3Dint xoffset,
      blink::WGC3Dint yoffset,
      blink::WGC3Dsizei width,
      blink::WGC3Dsizei height,
      blink::WGC3Denum format,
      blink::WGC3Denum type,
      const void* pixels) {}

  virtual void uniform1f(blink::WGC3Dint location, blink::WGC3Dfloat x) {}
  virtual void uniform1fv(
      blink::WGC3Dint location,
      blink::WGC3Dsizei count,
      const blink::WGC3Dfloat* v) {}
  virtual void uniform1i(blink::WGC3Dint location, blink::WGC3Dint x) {}
  virtual void uniform1iv(
      blink::WGC3Dint location,
      blink::WGC3Dsizei count,
      const blink::WGC3Dint* v) {}
  virtual void uniform2f(
      blink::WGC3Dint location,
      blink::WGC3Dfloat x,
      blink::WGC3Dfloat y) {}
  virtual void uniform2fv(
      blink::WGC3Dint location,
      blink::WGC3Dsizei count,
      const blink::WGC3Dfloat* v) {}
  virtual void uniform2i(
      blink::WGC3Dint location,
      blink::WGC3Dint x,
      blink::WGC3Dint y) {}
  virtual void uniform2iv(
      blink::WGC3Dint location,
      blink::WGC3Dsizei count,
      const blink::WGC3Dint* v) {}
  virtual void uniform3f(
      blink::WGC3Dint location,
      blink::WGC3Dfloat x,
      blink::WGC3Dfloat y,
      blink::WGC3Dfloat z) {}
  virtual void uniform3fv(
      blink::WGC3Dint location,
      blink::WGC3Dsizei count,
      const blink::WGC3Dfloat* v) {}
  virtual void uniform3i(
      blink::WGC3Dint location,
      blink::WGC3Dint x,
      blink::WGC3Dint y,
      blink::WGC3Dint z) {}
  virtual void uniform3iv(
      blink::WGC3Dint location,
      blink::WGC3Dsizei count,
      const blink::WGC3Dint* v) {}
  virtual void uniform4f(
      blink::WGC3Dint location,
      blink::WGC3Dfloat x,
      blink::WGC3Dfloat y,
      blink::WGC3Dfloat z,
      blink::WGC3Dfloat w) {}
  virtual void uniform4fv(
      blink::WGC3Dint location,
      blink::WGC3Dsizei count,
      const blink::WGC3Dfloat* v) {}
  virtual void uniform4i(
      blink::WGC3Dint location,
      blink::WGC3Dint x,
      blink::WGC3Dint y,
      blink::WGC3Dint z,
      blink::WGC3Dint w) {}
  virtual void uniform4iv(
      blink::WGC3Dint location,
      blink::WGC3Dsizei count,
      const blink::WGC3Dint* v) {}
  virtual void uniformMatrix2fv(
      blink::WGC3Dint location,
      blink::WGC3Dsizei count,
      blink::WGC3Dboolean transpose,
      const blink::WGC3Dfloat* value) {}
  virtual void uniformMatrix3fv(
      blink::WGC3Dint location,
      blink::WGC3Dsizei count,
      blink::WGC3Dboolean transpose,
      const blink::WGC3Dfloat* value) {}
  virtual void uniformMatrix4fv(
      blink::WGC3Dint location,
      blink::WGC3Dsizei count,
      blink::WGC3Dboolean transpose,
      const blink::WGC3Dfloat* value) {}

  virtual void useProgram(blink::WebGLId program);
  virtual void validateProgram(blink::WebGLId program) {}

  virtual void vertexAttrib1f(blink::WGC3Duint index, blink::WGC3Dfloat x) {}
  virtual void vertexAttrib1fv(
      blink::WGC3Duint index,
      const blink::WGC3Dfloat* values) {}
  virtual void vertexAttrib2f(
      blink::WGC3Duint index,
      blink::WGC3Dfloat x,
      blink::WGC3Dfloat y) {}
  virtual void vertexAttrib2fv(
      blink::WGC3Duint index,
      const blink::WGC3Dfloat* values) {}
  virtual void vertexAttrib3f(
      blink::WGC3Duint index,
      blink::WGC3Dfloat x,
      blink::WGC3Dfloat y,
      blink::WGC3Dfloat z) {}
  virtual void vertexAttrib3fv(
      blink::WGC3Duint index,
      const blink::WGC3Dfloat* values) {}
  virtual void vertexAttrib4f(
      blink::WGC3Duint index,
      blink::WGC3Dfloat x,
      blink::WGC3Dfloat y,
      blink::WGC3Dfloat z,
      blink::WGC3Dfloat w) {}
  virtual void vertexAttrib4fv(
      blink::WGC3Duint index,
      const blink::WGC3Dfloat* values) {}
  virtual void vertexAttribPointer(
      blink::WGC3Duint index,
      blink::WGC3Dint size,
      blink::WGC3Denum type,
      blink::WGC3Dboolean normalized,
      blink::WGC3Dsizei stride,
      blink::WGC3Dintptr offset) {}

  virtual void viewport(
      blink::WGC3Dint x,
      blink::WGC3Dint y,
      blink::WGC3Dsizei width,
      blink::WGC3Dsizei height) {}

  virtual void genBuffers(blink::WGC3Dsizei count, blink::WebGLId* ids);
  virtual void genFramebuffers(blink::WGC3Dsizei count, blink::WebGLId* ids);
  virtual void genRenderbuffers(blink::WGC3Dsizei count, blink::WebGLId* ids);
  virtual void genTextures(blink::WGC3Dsizei count, blink::WebGLId* ids);

  virtual void deleteBuffers(blink::WGC3Dsizei count, blink::WebGLId* ids);
  virtual void deleteFramebuffers(
      blink::WGC3Dsizei count, blink::WebGLId* ids);
  virtual void deleteRenderbuffers(
      blink::WGC3Dsizei count, blink::WebGLId* ids);
  virtual void deleteTextures(blink::WGC3Dsizei count, blink::WebGLId* ids);

  virtual blink::WebGLId createBuffer();
  virtual blink::WebGLId createFramebuffer();
  virtual blink::WebGLId createRenderbuffer();
  virtual blink::WebGLId createTexture();

  virtual void deleteBuffer(blink::WebGLId id);
  virtual void deleteFramebuffer(blink::WebGLId id);
  virtual void deleteRenderbuffer(blink::WebGLId id);
  virtual void deleteTexture(blink::WebGLId id);

  virtual blink::WebGLId createProgram();
  virtual blink::WebGLId createShader(blink::WGC3Denum);

  virtual void deleteProgram(blink::WebGLId id);
  virtual void deleteShader(blink::WebGLId id);

  virtual void texStorage2DEXT(
      blink::WGC3Denum target,
      blink::WGC3Dint levels,
      blink::WGC3Duint internalformat,
      blink::WGC3Dint width,
      blink::WGC3Dint height) {}

  virtual blink::WebGLId createQueryEXT();
  virtual void deleteQueryEXT(blink::WebGLId query) {}
  virtual blink::WGC3Dboolean isQueryEXT(blink::WebGLId query);
  virtual void beginQueryEXT(
      blink::WGC3Denum target,
      blink::WebGLId query) {}
  virtual void endQueryEXT(blink::WGC3Denum target);
  virtual void getQueryivEXT(
      blink::WGC3Denum target,
      blink::WGC3Denum pname,
      blink::WGC3Dint* params) {}
  virtual void getQueryObjectuivEXT(
      blink::WebGLId query,
      blink::WGC3Denum pname,
      blink::WGC3Duint* params);

  virtual void setContextLostCallback(
      WebGraphicsContextLostCallback* callback);

  virtual void loseContextCHROMIUM(blink::WGC3Denum current,
                                   blink::WGC3Denum other);

  virtual void drawBuffersEXT(blink::WGC3Dsizei m,
                              const blink::WGC3Denum* bufs) {}

  virtual void bindTexImage2DCHROMIUM(blink::WGC3Denum target,
                                      blink::WGC3Dint image_id) {}

  // GL_CHROMIUM_gpu_memory_buffer
  virtual blink::WGC3Duint createImageCHROMIUM(
      blink::WGC3Dsizei width,
      blink::WGC3Dsizei height,
      blink::WGC3Denum internalformat);
  virtual void destroyImageCHROMIUM(blink::WGC3Duint image_id) {}
  virtual void getImageParameterivCHROMIUM(
      blink::WGC3Duint image_id,
      blink::WGC3Denum pname,
      blink::WGC3Dint* params) {}
  virtual void* mapImageCHROMIUM(
      blink::WGC3Duint image_id,
      blink::WGC3Denum access);
  virtual void unmapImageCHROMIUM(blink::WGC3Duint image_id) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeWebGraphicsContext3D);
};

}  // namespace cc

#endif  // CC_TEST_FAKE_WEB_GRAPHICS_CONTEXT_3D_H_
