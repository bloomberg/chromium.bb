// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_WEB_GRAPHICS_CONTEXT_3D_H_
#define CC_TEST_FAKE_WEB_GRAPHICS_CONTEXT_3D_H_

#include <vector>

#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace cc {

// WebGraphicsContext3D base class for use in WebKit unit tests.
// All operations are no-ops (returning 0 if necessary).
class FakeWebGraphicsContext3D : public WebKit::WebGraphicsContext3D {
 public:
  static scoped_ptr<FakeWebGraphicsContext3D> Create() {
    return make_scoped_ptr(new FakeWebGraphicsContext3D());
  }
  static scoped_ptr<FakeWebGraphicsContext3D> Create(
      const WebKit::WebGraphicsContext3D::Attributes& attributes) {
    return make_scoped_ptr(new FakeWebGraphicsContext3D(attributes));
  }
  virtual ~FakeWebGraphicsContext3D();

  virtual bool makeContextCurrent();

  virtual int width();
  virtual int height();

  virtual void reshape(int width, int height);

  virtual bool isGLES2Compliant();

  virtual bool readBackFramebuffer(
      unsigned char* pixels,
      size_t buffer_size,
      WebKit::WebGLId framebuffer,
      int width,
      int height);

  virtual WebKit::WebGLId getPlatformTextureId();

  virtual void prepareTexture() {}

  virtual void postSubBufferCHROMIUM(int x, int y, int width, int height) {}

  virtual void synthesizeGLError(WebKit::WGC3Denum) {}

  virtual bool isContextLost();
  virtual WebKit::WGC3Denum getGraphicsResetStatusARB();

  virtual void* mapBufferSubDataCHROMIUM(
      WebKit::WGC3Denum target,
      WebKit::WGC3Dintptr offset,
      WebKit::WGC3Dsizeiptr size,
      WebKit::WGC3Denum access);

  virtual void unmapBufferSubDataCHROMIUM(const void*) {}
  virtual void* mapTexSubImage2DCHROMIUM(
      WebKit::WGC3Denum target,
      WebKit::WGC3Dint level,
      WebKit::WGC3Dint xoffset,
      WebKit::WGC3Dint yoffset,
      WebKit::WGC3Dsizei width,
      WebKit::WGC3Dsizei height,
      WebKit::WGC3Denum format,
      WebKit::WGC3Denum type,
      WebKit::WGC3Denum access);
  virtual void unmapTexSubImage2DCHROMIUM(const void*) {}

  virtual void setVisibilityCHROMIUM(bool visible) {}

  virtual void discardFramebufferEXT(
      WebKit::WGC3Denum target,
      WebKit::WGC3Dsizei num_attachments,
      const WebKit::WGC3Denum* attachments) {}
  virtual void ensureFramebufferCHROMIUM() {}

  virtual void setMemoryAllocationChangedCallbackCHROMIUM(
      WebGraphicsMemoryAllocationChangedCallbackCHROMIUM* callback) {}

  virtual WebKit::WebString getRequestableExtensionsCHROMIUM();
  virtual void requestExtensionCHROMIUM(const char*) {}

  virtual void blitFramebufferCHROMIUM(
      WebKit::WGC3Dint srcX0, WebKit::WGC3Dint srcY0,
      WebKit::WGC3Dint srcX1, WebKit::WGC3Dint srcY1,
      WebKit::WGC3Dint dstX0, WebKit::WGC3Dint dstY0,
      WebKit::WGC3Dint dstX1, WebKit::WGC3Dint dstY1,
      WebKit::WGC3Dbitfield mask,
      WebKit::WGC3Denum filter) {}
  virtual void renderbufferStorageMultisampleCHROMIUM(
      WebKit::WGC3Denum target,
      WebKit::WGC3Dsizei samples,
      WebKit::WGC3Denum internalformat,
      WebKit::WGC3Dsizei width,
      WebKit::WGC3Dsizei height) {}

  virtual void activeTexture(WebKit::WGC3Denum texture) {}
  virtual void attachShader(WebKit::WebGLId program, WebKit::WebGLId shader);
  virtual void bindAttribLocation(
      WebKit::WebGLId program,
      WebKit::WGC3Duint index,
      const WebKit::WGC3Dchar* name) {}
  virtual void bindBuffer(WebKit::WGC3Denum target, WebKit::WebGLId buffer);
  virtual void bindFramebuffer(
      WebKit::WGC3Denum target, WebKit::WebGLId framebuffer);
  virtual void bindRenderbuffer(
      WebKit::WGC3Denum target, WebKit::WebGLId renderbuffer);
  virtual void bindTexture(
      WebKit::WGC3Denum target,
      WebKit::WebGLId texture_id);
  virtual void blendColor(
      WebKit::WGC3Dclampf red,
      WebKit::WGC3Dclampf green,
      WebKit::WGC3Dclampf blue,
      WebKit::WGC3Dclampf alpha) {}
  virtual void blendEquation(WebKit::WGC3Denum mode) {}
  virtual void blendEquationSeparate(
      WebKit::WGC3Denum mode_rgb,
      WebKit::WGC3Denum mode_alpha) {}
  virtual void blendFunc(
      WebKit::WGC3Denum sfactor,
      WebKit::WGC3Denum dfactor) {}
  virtual void blendFuncSeparate(
      WebKit::WGC3Denum src_rgb,
      WebKit::WGC3Denum dst_rgb,
      WebKit::WGC3Denum src_alpha,
      WebKit::WGC3Denum dst_alpha) {}

  virtual void bufferData(
      WebKit::WGC3Denum target,
      WebKit::WGC3Dsizeiptr size,
      const void* data,
      WebKit::WGC3Denum usage) {}
  virtual void bufferSubData(
      WebKit::WGC3Denum target,
      WebKit::WGC3Dintptr offset,
      WebKit::WGC3Dsizeiptr size,
      const void* data) {}

  virtual WebKit::WGC3Denum checkFramebufferStatus(WebKit::WGC3Denum target);

  virtual void clear(WebKit::WGC3Dbitfield mask) {}
  virtual void clearColor(
      WebKit::WGC3Dclampf red,
      WebKit::WGC3Dclampf green,
      WebKit::WGC3Dclampf blue,
      WebKit::WGC3Dclampf alpha) {}
  virtual void clearDepth(WebKit::WGC3Dclampf depth) {}
  virtual void clearStencil(WebKit::WGC3Dint s) {}
  virtual void colorMask(
      WebKit::WGC3Dboolean red,
      WebKit::WGC3Dboolean green,
      WebKit::WGC3Dboolean blue,
      WebKit::WGC3Dboolean alpha) {}
  virtual void compileShader(WebKit::WebGLId shader) {}

  virtual void compressedTexImage2D(
      WebKit::WGC3Denum target,
      WebKit::WGC3Dint level,
      WebKit::WGC3Denum internal_format,
      WebKit::WGC3Dsizei width,
      WebKit::WGC3Dsizei height,
      WebKit::WGC3Dint border,
      WebKit::WGC3Dsizei imageSize,
      const void* data) {}
  virtual void compressedTexSubImage2D(
      WebKit::WGC3Denum target,
      WebKit::WGC3Dint level,
      WebKit::WGC3Dint xoffset,
      WebKit::WGC3Dint yoffset,
      WebKit::WGC3Dsizei width,
      WebKit::WGC3Dsizei height,
      WebKit::WGC3Denum format,
      WebKit::WGC3Dsizei imageSize,
      const void* data) {}
  virtual void copyTexImage2D(
      WebKit::WGC3Denum target,
      WebKit::WGC3Dint level,
      WebKit::WGC3Denum internalformat,
      WebKit::WGC3Dint x,
      WebKit::WGC3Dint y,
      WebKit::WGC3Dsizei width,
      WebKit::WGC3Dsizei height,
      WebKit::WGC3Dint border) {}
  virtual void copyTexSubImage2D(
      WebKit::WGC3Denum target,
      WebKit::WGC3Dint level,
      WebKit::WGC3Dint xoffset,
      WebKit::WGC3Dint yoffset,
      WebKit::WGC3Dint x,
      WebKit::WGC3Dint y,
      WebKit::WGC3Dsizei width,
      WebKit::WGC3Dsizei height) {}
  virtual void cullFace(WebKit::WGC3Denum mode) {}
  virtual void depthFunc(WebKit::WGC3Denum func) {}
  virtual void depthMask(WebKit::WGC3Dboolean flag) {}
  virtual void depthRange(
      WebKit::WGC3Dclampf zNear,
      WebKit::WGC3Dclampf zFar) {}
  virtual void detachShader(WebKit::WebGLId program, WebKit::WebGLId shader) {}
  virtual void disable(WebKit::WGC3Denum cap) {}
  virtual void disableVertexAttribArray(WebKit::WGC3Duint index) {}
  virtual void drawArrays(
      WebKit::WGC3Denum mode,
      WebKit::WGC3Dint first,
      WebKit::WGC3Dsizei count) {}
  virtual void drawElements(
      WebKit::WGC3Denum mode,
      WebKit::WGC3Dsizei count,
      WebKit::WGC3Denum type,
      WebKit::WGC3Dintptr offset) {}

  virtual void enable(WebKit::WGC3Denum cap) {}
  virtual void enableVertexAttribArray(WebKit::WGC3Duint index) {}
  virtual void finish() {}
  virtual void flush() {}
  virtual void framebufferRenderbuffer(
      WebKit::WGC3Denum target,
      WebKit::WGC3Denum attachment,
      WebKit::WGC3Denum renderbuffertarget,
      WebKit::WebGLId renderbuffer) {}
  virtual void framebufferTexture2D(
      WebKit::WGC3Denum target,
      WebKit::WGC3Denum attachment,
      WebKit::WGC3Denum textarget,
      WebKit::WebGLId texture,
      WebKit::WGC3Dint level) {}
  virtual void frontFace(WebKit::WGC3Denum mode) {}
  virtual void generateMipmap(WebKit::WGC3Denum target) {}

  virtual bool getActiveAttrib(
      WebKit::WebGLId program,
      WebKit::WGC3Duint index, ActiveInfo&);
  virtual bool getActiveUniform(
      WebKit::WebGLId program,
      WebKit::WGC3Duint index,
      ActiveInfo&);
  virtual void getAttachedShaders(
      WebKit::WebGLId program,
      WebKit::WGC3Dsizei maxCount,
      WebKit::WGC3Dsizei* count,
      WebKit::WebGLId* shaders) {}
  virtual WebKit::WGC3Dint getAttribLocation(
      WebKit::WebGLId program,
      const WebKit::WGC3Dchar* name);
  virtual void getBooleanv(
      WebKit::WGC3Denum pname,
      WebKit::WGC3Dboolean* value) {}
  virtual void getBufferParameteriv(
      WebKit::WGC3Denum target,
      WebKit::WGC3Denum pname,
      WebKit::WGC3Dint* value) {}
  virtual Attributes getContextAttributes();
  virtual WebKit::WGC3Denum getError();
  virtual void getFloatv(
      WebKit::WGC3Denum pname,
      WebKit::WGC3Dfloat* value) {}
  virtual void getFramebufferAttachmentParameteriv(
      WebKit::WGC3Denum target,
      WebKit::WGC3Denum attachment,
      WebKit::WGC3Denum pname,
      WebKit::WGC3Dint* value) {}

  virtual void getIntegerv(
      WebKit::WGC3Denum pname,
      WebKit::WGC3Dint* value);

  virtual void getProgramiv(
      WebKit::WebGLId program,
      WebKit::WGC3Denum pname,
      WebKit::WGC3Dint* value);

  virtual WebKit::WebString getProgramInfoLog(WebKit::WebGLId program);
  virtual void getRenderbufferParameteriv(
      WebKit::WGC3Denum target,
      WebKit::WGC3Denum pname,
      WebKit::WGC3Dint* value) {}

  virtual void getShaderiv(
      WebKit::WebGLId shader,
      WebKit::WGC3Denum pname,
      WebKit::WGC3Dint* value);

  virtual WebKit::WebString getShaderInfoLog(WebKit::WebGLId shader);
  virtual void getShaderPrecisionFormat(
      WebKit::WGC3Denum shadertype,
      WebKit::WGC3Denum precisiontype,
      WebKit::WGC3Dint* range,
      WebKit::WGC3Dint* precision) {}
  virtual WebKit::WebString getShaderSource(WebKit::WebGLId shader);
  virtual WebKit::WebString getString(WebKit::WGC3Denum name);
  virtual void getTexParameterfv(
      WebKit::WGC3Denum target,
      WebKit::WGC3Denum pname,
      WebKit::WGC3Dfloat* value) {}
  virtual void getTexParameteriv(
      WebKit::WGC3Denum target,
      WebKit::WGC3Denum pname,
      WebKit::WGC3Dint* value) {}
  virtual void getUniformfv(
      WebKit::WebGLId program,
      WebKit::WGC3Dint location,
      WebKit::WGC3Dfloat* value) {}
  virtual void getUniformiv(
      WebKit::WebGLId program,
      WebKit::WGC3Dint location,
      WebKit::WGC3Dint* value) {}
  virtual WebKit::WGC3Dint getUniformLocation(
      WebKit::WebGLId program,
      const WebKit::WGC3Dchar* name);
  virtual void getVertexAttribfv(
      WebKit::WGC3Duint index,
      WebKit::WGC3Denum pname,
      WebKit::WGC3Dfloat* value) {}
  virtual void getVertexAttribiv(
      WebKit::WGC3Duint index,
      WebKit::WGC3Denum pname,
      WebKit::WGC3Dint* value) {}
  virtual WebKit::WGC3Dsizeiptr getVertexAttribOffset(
      WebKit::WGC3Duint index,
      WebKit::WGC3Denum pname);

  virtual void hint(WebKit::WGC3Denum target, WebKit::WGC3Denum mode) {}
  virtual WebKit::WGC3Dboolean isBuffer(WebKit::WebGLId buffer);
  virtual WebKit::WGC3Dboolean isEnabled(WebKit::WGC3Denum cap);
  virtual WebKit::WGC3Dboolean isFramebuffer(WebKit::WebGLId framebuffer);
  virtual WebKit::WGC3Dboolean isProgram(WebKit::WebGLId program);
  virtual WebKit::WGC3Dboolean isRenderbuffer(WebKit::WebGLId renderbuffer);
  virtual WebKit::WGC3Dboolean isShader(WebKit::WebGLId shader);
  virtual WebKit::WGC3Dboolean isTexture(WebKit::WebGLId texture);
  virtual void lineWidth(WebKit::WGC3Dfloat) {}
  virtual void linkProgram(WebKit::WebGLId program) {}
  virtual void pixelStorei(WebKit::WGC3Denum pname, WebKit::WGC3Dint param) {}
  virtual void polygonOffset(
      WebKit::WGC3Dfloat factor,
      WebKit::WGC3Dfloat units) {}

  virtual void readPixels(
      WebKit::WGC3Dint x,
      WebKit::WGC3Dint y,
      WebKit::WGC3Dsizei width,
      WebKit::WGC3Dsizei height,
      WebKit::WGC3Denum format,
      WebKit::WGC3Denum type,
      void* pixels) {}

  virtual void releaseShaderCompiler() {}

  virtual void renderbufferStorage(
      WebKit::WGC3Denum target,
      WebKit::WGC3Denum internalformat,
      WebKit::WGC3Dsizei width,
      WebKit::WGC3Dsizei height) {}
  virtual void sampleCoverage(
      WebKit::WGC3Dclampf value,
      WebKit::WGC3Dboolean invert) {}
  virtual void scissor(
      WebKit::WGC3Dint x,
      WebKit::WGC3Dint y,
      WebKit::WGC3Dsizei width,
      WebKit::WGC3Dsizei height) {}
  virtual void shaderSource(
      WebKit::WebGLId shader,
      const WebKit::WGC3Dchar* string) {}
  virtual void stencilFunc(
      WebKit::WGC3Denum func,
      WebKit::WGC3Dint ref,
      WebKit::WGC3Duint mask) {}
  virtual void stencilFuncSeparate(
      WebKit::WGC3Denum face,
      WebKit::WGC3Denum func,
      WebKit::WGC3Dint ref,
      WebKit::WGC3Duint mask) {}
  virtual void stencilMask(WebKit::WGC3Duint mask) {}
  virtual void stencilMaskSeparate(
      WebKit::WGC3Denum face,
      WebKit::WGC3Duint mask) {}
  virtual void stencilOp(
      WebKit::WGC3Denum fail,
      WebKit::WGC3Denum zfail,
      WebKit::WGC3Denum zpass) {}
  virtual void stencilOpSeparate(
      WebKit::WGC3Denum face,
      WebKit::WGC3Denum fail,
      WebKit::WGC3Denum zfail,
      WebKit::WGC3Denum zpass) {}

  virtual void texImage2D(
      WebKit::WGC3Denum target,
      WebKit::WGC3Dint level,
      WebKit::WGC3Denum internalformat,
      WebKit::WGC3Dsizei width,
      WebKit::WGC3Dsizei height,
      WebKit::WGC3Dint border,
      WebKit::WGC3Denum format,
      WebKit::WGC3Denum type,
      const void* pixels) {}

  virtual void texParameterf(
      WebKit::WGC3Denum target,
      WebKit::WGC3Denum pname,
      WebKit::WGC3Dfloat param) {}
  virtual void texParameteri(
      WebKit::WGC3Denum target,
      WebKit::WGC3Denum pname,
      WebKit::WGC3Dint param) {}

  virtual void texSubImage2D(
      WebKit::WGC3Denum target,
      WebKit::WGC3Dint level,
      WebKit::WGC3Dint xoffset,
      WebKit::WGC3Dint yoffset,
      WebKit::WGC3Dsizei width,
      WebKit::WGC3Dsizei height,
      WebKit::WGC3Denum format,
      WebKit::WGC3Denum type,
      const void* pixels) {}

  virtual void uniform1f(WebKit::WGC3Dint location, WebKit::WGC3Dfloat x) {}
  virtual void uniform1fv(
      WebKit::WGC3Dint location,
      WebKit::WGC3Dsizei count,
      const WebKit::WGC3Dfloat* v) {}
  virtual void uniform1i(WebKit::WGC3Dint location, WebKit::WGC3Dint x) {}
  virtual void uniform1iv(
      WebKit::WGC3Dint location,
      WebKit::WGC3Dsizei count,
      const WebKit::WGC3Dint* v) {}
  virtual void uniform2f(
      WebKit::WGC3Dint location,
      WebKit::WGC3Dfloat x,
      WebKit::WGC3Dfloat y) {}
  virtual void uniform2fv(
      WebKit::WGC3Dint location,
      WebKit::WGC3Dsizei count,
      const WebKit::WGC3Dfloat* v) {}
  virtual void uniform2i(
      WebKit::WGC3Dint location,
      WebKit::WGC3Dint x,
      WebKit::WGC3Dint y) {}
  virtual void uniform2iv(
      WebKit::WGC3Dint location,
      WebKit::WGC3Dsizei count,
      const WebKit::WGC3Dint* v) {}
  virtual void uniform3f(
      WebKit::WGC3Dint location,
      WebKit::WGC3Dfloat x,
      WebKit::WGC3Dfloat y,
      WebKit::WGC3Dfloat z) {}
  virtual void uniform3fv(
      WebKit::WGC3Dint location,
      WebKit::WGC3Dsizei count,
      const WebKit::WGC3Dfloat* v) {}
  virtual void uniform3i(
      WebKit::WGC3Dint location,
      WebKit::WGC3Dint x,
      WebKit::WGC3Dint y,
      WebKit::WGC3Dint z) {}
  virtual void uniform3iv(
      WebKit::WGC3Dint location,
      WebKit::WGC3Dsizei count,
      const WebKit::WGC3Dint* v) {}
  virtual void uniform4f(
      WebKit::WGC3Dint location,
      WebKit::WGC3Dfloat x,
      WebKit::WGC3Dfloat y,
      WebKit::WGC3Dfloat z,
      WebKit::WGC3Dfloat w) {}
  virtual void uniform4fv(
      WebKit::WGC3Dint location,
      WebKit::WGC3Dsizei count,
      const WebKit::WGC3Dfloat* v) {}
  virtual void uniform4i(
      WebKit::WGC3Dint location,
      WebKit::WGC3Dint x,
      WebKit::WGC3Dint y,
      WebKit::WGC3Dint z,
      WebKit::WGC3Dint w) {}
  virtual void uniform4iv(
      WebKit::WGC3Dint location,
      WebKit::WGC3Dsizei count,
      const WebKit::WGC3Dint* v) {}
  virtual void uniformMatrix2fv(
      WebKit::WGC3Dint location,
      WebKit::WGC3Dsizei count,
      WebKit::WGC3Dboolean transpose,
      const WebKit::WGC3Dfloat* value) {}
  virtual void uniformMatrix3fv(
      WebKit::WGC3Dint location,
      WebKit::WGC3Dsizei count,
      WebKit::WGC3Dboolean transpose,
      const WebKit::WGC3Dfloat* value) {}
  virtual void uniformMatrix4fv(
      WebKit::WGC3Dint location,
      WebKit::WGC3Dsizei count,
      WebKit::WGC3Dboolean transpose,
      const WebKit::WGC3Dfloat* value) {}

  virtual void useProgram(WebKit::WebGLId program);
  virtual void validateProgram(WebKit::WebGLId program) {}

  virtual void vertexAttrib1f(WebKit::WGC3Duint index, WebKit::WGC3Dfloat x) {}
  virtual void vertexAttrib1fv(
      WebKit::WGC3Duint index,
      const WebKit::WGC3Dfloat* values) {}
  virtual void vertexAttrib2f(
      WebKit::WGC3Duint index,
      WebKit::WGC3Dfloat x,
      WebKit::WGC3Dfloat y) {}
  virtual void vertexAttrib2fv(
      WebKit::WGC3Duint index,
      const WebKit::WGC3Dfloat* values) {}
  virtual void vertexAttrib3f(
      WebKit::WGC3Duint index,
      WebKit::WGC3Dfloat x,
      WebKit::WGC3Dfloat y,
      WebKit::WGC3Dfloat z) {}
  virtual void vertexAttrib3fv(
      WebKit::WGC3Duint index,
      const WebKit::WGC3Dfloat* values) {}
  virtual void vertexAttrib4f(
      WebKit::WGC3Duint index,
      WebKit::WGC3Dfloat x,
      WebKit::WGC3Dfloat y,
      WebKit::WGC3Dfloat z,
      WebKit::WGC3Dfloat w) {}
  virtual void vertexAttrib4fv(
      WebKit::WGC3Duint index,
      const WebKit::WGC3Dfloat* values) {}
  virtual void vertexAttribPointer(
      WebKit::WGC3Duint index,
      WebKit::WGC3Dint size,
      WebKit::WGC3Denum type,
      WebKit::WGC3Dboolean normalized,
      WebKit::WGC3Dsizei stride,
      WebKit::WGC3Dintptr offset) {}

  virtual void viewport(
      WebKit::WGC3Dint x,
      WebKit::WGC3Dint y,
      WebKit::WGC3Dsizei width,
      WebKit::WGC3Dsizei height) {}

  virtual WebKit::WebGLId createBuffer();
  virtual WebKit::WebGLId createFramebuffer();
  virtual WebKit::WebGLId createProgram();
  virtual WebKit::WebGLId createRenderbuffer();
  virtual WebKit::WebGLId createShader(WebKit::WGC3Denum);
  virtual WebKit::WebGLId createTexture();

  virtual void deleteBuffer(WebKit::WebGLId id);
  virtual void deleteFramebuffer(WebKit::WebGLId id);
  virtual void deleteProgram(WebKit::WebGLId id);
  virtual void deleteRenderbuffer(WebKit::WebGLId id);
  virtual void deleteShader(WebKit::WebGLId id);
  virtual void deleteTexture(WebKit::WebGLId texture_id);

  virtual void texStorage2DEXT(
      WebKit::WGC3Denum target,
      WebKit::WGC3Dint levels,
      WebKit::WGC3Duint internalformat,
      WebKit::WGC3Dint width,
      WebKit::WGC3Dint height) {}

  virtual WebKit::WebGLId createQueryEXT();
  virtual void deleteQueryEXT(WebKit::WebGLId query) {}
  virtual WebKit::WGC3Dboolean isQueryEXT(WebKit::WebGLId query);
  virtual void beginQueryEXT(
      WebKit::WGC3Denum target,
      WebKit::WebGLId query) {}
  virtual void endQueryEXT(WebKit::WGC3Denum target);
  virtual void getQueryivEXT(
      WebKit::WGC3Denum target,
      WebKit::WGC3Denum pname,
      WebKit::WGC3Dint* params) {}
  virtual void getQueryObjectuivEXT(
      WebKit::WebGLId query,
      WebKit::WGC3Denum pname,
      WebKit::WGC3Duint* params);

  virtual void setContextLostCallback(
      WebGraphicsContextLostCallback* callback);

  virtual void loseContextCHROMIUM(WebKit::WGC3Denum current,
                                   WebKit::WGC3Denum other);

  // When set, MakeCurrent() will fail after this many times.
  void set_times_make_current_succeeds(int times) {
    times_make_current_succeeds_ = times;
  }
  void set_times_bind_texture_succeeds(int times) {
    times_bind_texture_succeeds_ = times;
  }
  void set_times_end_query_succeeds(int times) {
    times_end_query_succeeds_ = times;
  }

  size_t NumTextures() const { return textures_.size(); }
  WebKit::WebGLId TextureAt(int i) const { return textures_[i]; }

  size_t NumUsedTextures() const { return used_textures_.size(); }
  bool UsedTexture(int texture) const {
    return ContainsKey(used_textures_, texture);
  }
  void ResetUsedTextures() { used_textures_.clear(); }

  void set_have_extension_io_surface(bool have) {
    have_extension_io_surface_ = have;
  }
  void set_have_extension_egl_image(bool have) {
    have_extension_egl_image_ = have;
  }

  static const WebKit::WebGLId kExternalTextureId;
  virtual WebKit::WebGLId NextTextureId();

 protected:
  FakeWebGraphicsContext3D();
  FakeWebGraphicsContext3D(
      const WebKit::WebGraphicsContext3D::Attributes& attributes);

  unsigned context_id_;
  unsigned next_texture_id_;
  Attributes attributes_;
  bool have_extension_io_surface_;
  bool have_extension_egl_image_;
  int times_make_current_succeeds_;
  int times_bind_texture_succeeds_;
  int times_end_query_succeeds_;
  bool context_lost_;
  WebGraphicsContextLostCallback* context_lost_callback_;
  std::vector<WebKit::WebGLId> textures_;
  base::hash_set<WebKit::WebGLId> used_textures_;
  int width_;
  int height_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_WEB_GRAPHICS_CONTEXT_3D_H_
