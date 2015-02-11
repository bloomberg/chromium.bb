// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_BLINK_WEBGRAPHICSCONTEXT3D_IMPL_H_
#define GPU_BLINK_WEBGRAPHICSCONTEXT3D_IMPL_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "gpu/blink/gpu_blink_export.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace gpu {

namespace gles2 {
class GLES2Interface;
class GLES2ImplementationErrorMessageCallback;
struct ContextCreationAttribHelper;
}
}

using blink::WebGLId;

using blink::WGC3Dbyte;
using blink::WGC3Dchar;
using blink::WGC3Denum;
using blink::WGC3Dboolean;
using blink::WGC3Dbitfield;
using blink::WGC3Dint;
using blink::WGC3Dsizei;
using blink::WGC3Duint;
using blink::WGC3Dfloat;
using blink::WGC3Dclampf;
using blink::WGC3Dintptr;
using blink::WGC3Dsizeiptr;
using blink::WGC3Duint64;

namespace gpu_blink {

class WebGraphicsContext3DErrorMessageCallback;

class GPU_BLINK_EXPORT WebGraphicsContext3DImpl
    : public NON_EXPORTED_BASE(blink::WebGraphicsContext3D) {
 public:
  virtual ~WebGraphicsContext3DImpl();

  //----------------------------------------------------------------------
  // WebGraphicsContext3D methods

  virtual uint32_t lastFlushID();

  virtual unsigned int insertSyncPoint();
  virtual void waitSyncPoint(unsigned int sync_point);

  virtual void loseContextCHROMIUM(WGC3Denum current, WGC3Denum other);

  virtual void reshapeWithScaleFactor(
      int width, int height, float scale_factor);

  virtual void prepareTexture();
  virtual void postSubBufferCHROMIUM(int x, int y, int width, int height);

  virtual void activeTexture(WGC3Denum texture);
  virtual void attachShader(WebGLId program, WebGLId shader);
  virtual void bindAttribLocation(WebGLId program, WGC3Duint index,
                                  const WGC3Dchar* name);
  virtual void bindBuffer(WGC3Denum target, WebGLId buffer);
  virtual void bindFramebuffer(WGC3Denum target, WebGLId framebuffer);
  virtual void bindRenderbuffer(WGC3Denum target, WebGLId renderbuffer);
  virtual void bindTexture(WGC3Denum target, WebGLId texture);
  virtual void blendColor(WGC3Dclampf red, WGC3Dclampf green,
                          WGC3Dclampf blue, WGC3Dclampf alpha);
  virtual void blendEquation(WGC3Denum mode);
  virtual void blendEquationSeparate(WGC3Denum modeRGB,
                                     WGC3Denum modeAlpha);
  virtual void blendFunc(WGC3Denum sfactor, WGC3Denum dfactor);
  virtual void blendFuncSeparate(WGC3Denum srcRGB,
                                 WGC3Denum dstRGB,
                                 WGC3Denum srcAlpha,
                                 WGC3Denum dstAlpha);

  virtual void bufferData(WGC3Denum target, WGC3Dsizeiptr size,
                          const void* data, WGC3Denum usage);
  virtual void bufferSubData(WGC3Denum target, WGC3Dintptr offset,
                             WGC3Dsizeiptr size, const void* data);

  virtual WGC3Denum checkFramebufferStatus(WGC3Denum target);
  virtual void clear(WGC3Dbitfield mask);
  virtual void clearColor(WGC3Dclampf red, WGC3Dclampf green,
                          WGC3Dclampf blue, WGC3Dclampf alpha);
  virtual void clearDepth(WGC3Dclampf depth);
  virtual void clearStencil(WGC3Dint s);
  virtual void colorMask(WGC3Dboolean red, WGC3Dboolean green,
                         WGC3Dboolean blue, WGC3Dboolean alpha);
  virtual void compileShader(WebGLId shader);

  virtual void compressedTexImage2D(WGC3Denum target,
                                    WGC3Dint level,
                                    WGC3Denum internalformat,
                                    WGC3Dsizei width,
                                    WGC3Dsizei height,
                                    WGC3Dint border,
                                    WGC3Dsizei imageSize,
                                    const void* data);
  virtual void compressedTexSubImage2D(WGC3Denum target,
                                       WGC3Dint level,
                                       WGC3Dint xoffset,
                                       WGC3Dint yoffset,
                                       WGC3Dsizei width,
                                       WGC3Dsizei height,
                                       WGC3Denum format,
                                       WGC3Dsizei imageSize,
                                       const void* data);
  virtual void copyTexImage2D(WGC3Denum target,
                              WGC3Dint level,
                              WGC3Denum internalformat,
                              WGC3Dint x,
                              WGC3Dint y,
                              WGC3Dsizei width,
                              WGC3Dsizei height,
                              WGC3Dint border);
  virtual void copyTexSubImage2D(WGC3Denum target,
                                 WGC3Dint level,
                                 WGC3Dint xoffset,
                                 WGC3Dint yoffset,
                                 WGC3Dint x,
                                 WGC3Dint y,
                                 WGC3Dsizei width,
                                 WGC3Dsizei height);
  virtual void cullFace(WGC3Denum mode);
  virtual void depthFunc(WGC3Denum func);
  virtual void depthMask(WGC3Dboolean flag);
  virtual void depthRange(WGC3Dclampf zNear, WGC3Dclampf zFar);
  virtual void detachShader(WebGLId program, WebGLId shader);
  virtual void disable(WGC3Denum cap);
  virtual void disableVertexAttribArray(WGC3Duint index);
  virtual void drawArrays(WGC3Denum mode, WGC3Dint first, WGC3Dsizei count);
  virtual void drawElements(WGC3Denum mode,
                            WGC3Dsizei count,
                            WGC3Denum type,
                            WGC3Dintptr offset);

  virtual void enable(WGC3Denum cap);
  virtual void enableVertexAttribArray(WGC3Duint index);
  virtual void finish();
  virtual void flush();
  virtual void framebufferRenderbuffer(WGC3Denum target,
                                       WGC3Denum attachment,
                                       WGC3Denum renderbuffertarget,
                                       WebGLId renderbuffer);
  virtual void framebufferTexture2D(WGC3Denum target,
                                    WGC3Denum attachment,
                                    WGC3Denum textarget,
                                    WebGLId texture,
                                    WGC3Dint level);
  virtual void frontFace(WGC3Denum mode);
  virtual void generateMipmap(WGC3Denum target);

  virtual bool getActiveAttrib(WebGLId program,
                               WGC3Duint index,
                               ActiveInfo&);
  virtual bool getActiveUniform(WebGLId program,
                                WGC3Duint index,
                                ActiveInfo&);

  virtual void getAttachedShaders(WebGLId program,
                                  WGC3Dsizei maxCount,
                                  WGC3Dsizei* count,
                                  WebGLId* shaders);

  virtual WGC3Dint  getAttribLocation(WebGLId program, const WGC3Dchar* name);

  virtual void getBooleanv(WGC3Denum pname, WGC3Dboolean* value);

  virtual void getBufferParameteriv(WGC3Denum target,
                                    WGC3Denum pname,
                                    WGC3Dint* value);

  virtual WGC3Denum getError();

  virtual void getFloatv(WGC3Denum pname, WGC3Dfloat* value);

  virtual void getFramebufferAttachmentParameteriv(WGC3Denum target,
                                                   WGC3Denum attachment,
                                                   WGC3Denum pname,
                                                   WGC3Dint* value);

  virtual void getIntegerv(WGC3Denum pname, WGC3Dint* value);

  virtual void getProgramiv(WebGLId program, WGC3Denum pname, WGC3Dint* value);

  virtual blink::WebString getProgramInfoLog(WebGLId program);

  virtual void getRenderbufferParameteriv(WGC3Denum target,
                                          WGC3Denum pname,
                                          WGC3Dint* value);

  virtual void getShaderiv(WebGLId shader, WGC3Denum pname, WGC3Dint* value);

  virtual blink::WebString getShaderInfoLog(WebGLId shader);

  virtual void getShaderPrecisionFormat(WGC3Denum shadertype,
                                        WGC3Denum precisiontype,
                                        WGC3Dint* range,
                                        WGC3Dint* precision);

  virtual blink::WebString getShaderSource(WebGLId shader);
  virtual blink::WebString getString(WGC3Denum name);

  virtual void getTexParameterfv(WGC3Denum target,
                                 WGC3Denum pname,
                                 WGC3Dfloat* value);
  virtual void getTexParameteriv(WGC3Denum target,
                                 WGC3Denum pname,
                                 WGC3Dint* value);

  virtual void getUniformfv(WebGLId program,
                            WGC3Dint location,
                            WGC3Dfloat* value);
  virtual void getUniformiv(WebGLId program,
                            WGC3Dint location,
                            WGC3Dint* value);

  virtual WGC3Dint getUniformLocation(WebGLId program, const WGC3Dchar* name);

  virtual void getVertexAttribfv(WGC3Duint index, WGC3Denum pname,
                                 WGC3Dfloat* value);
  virtual void getVertexAttribiv(WGC3Duint index, WGC3Denum pname,
                                 WGC3Dint* value);

  virtual WGC3Dsizeiptr getVertexAttribOffset(WGC3Duint index, WGC3Denum pname);

  virtual void hint(WGC3Denum target, WGC3Denum mode);
  virtual WGC3Dboolean isBuffer(WebGLId buffer);
  virtual WGC3Dboolean isEnabled(WGC3Denum cap);
  virtual WGC3Dboolean isFramebuffer(WebGLId framebuffer);
  virtual WGC3Dboolean isProgram(WebGLId program);
  virtual WGC3Dboolean isRenderbuffer(WebGLId renderbuffer);
  virtual WGC3Dboolean isShader(WebGLId shader);
  virtual WGC3Dboolean isTexture(WebGLId texture);
  virtual void lineWidth(WGC3Dfloat);
  virtual void linkProgram(WebGLId program);
  virtual void pixelStorei(WGC3Denum pname, WGC3Dint param);
  virtual void polygonOffset(WGC3Dfloat factor, WGC3Dfloat units);

  virtual void readPixels(WGC3Dint x,
                          WGC3Dint y,
                          WGC3Dsizei width,
                          WGC3Dsizei height,
                          WGC3Denum format,
                          WGC3Denum type,
                          void* pixels);

  virtual void releaseShaderCompiler();
  virtual void renderbufferStorage(WGC3Denum target,
                                   WGC3Denum internalformat,
                                   WGC3Dsizei width,
                                   WGC3Dsizei height);
  virtual void sampleCoverage(WGC3Dfloat value, WGC3Dboolean invert);
  virtual void scissor(WGC3Dint x, WGC3Dint y,
                       WGC3Dsizei width, WGC3Dsizei height);
  virtual void shaderSource(WebGLId shader, const WGC3Dchar* string);
  virtual void stencilFunc(WGC3Denum func, WGC3Dint ref, WGC3Duint mask);
  virtual void stencilFuncSeparate(WGC3Denum face,
                                   WGC3Denum func,
                                   WGC3Dint ref,
                                   WGC3Duint mask);
  virtual void stencilMask(WGC3Duint mask);
  virtual void stencilMaskSeparate(WGC3Denum face, WGC3Duint mask);
  virtual void stencilOp(WGC3Denum fail,
                         WGC3Denum zfail,
                         WGC3Denum zpass);
  virtual void stencilOpSeparate(WGC3Denum face,
                                 WGC3Denum fail,
                                 WGC3Denum zfail,
                                 WGC3Denum zpass);

  virtual void texImage2D(WGC3Denum target,
                          WGC3Dint level,
                          WGC3Denum internalformat,
                          WGC3Dsizei width,
                          WGC3Dsizei height,
                          WGC3Dint border,
                          WGC3Denum format,
                          WGC3Denum type,
                          const void* pixels);

  virtual void texParameterf(WGC3Denum target,
                             WGC3Denum pname,
                             WGC3Dfloat param);
  virtual void texParameteri(WGC3Denum target,
                             WGC3Denum pname,
                             WGC3Dint param);

  virtual void texSubImage2D(WGC3Denum target,
                             WGC3Dint level,
                             WGC3Dint xoffset,
                             WGC3Dint yoffset,
                             WGC3Dsizei width,
                             WGC3Dsizei height,
                             WGC3Denum format,
                             WGC3Denum type,
                             const void* pixels);

  virtual void uniform1f(WGC3Dint location, WGC3Dfloat x);
  virtual void uniform1fv(WGC3Dint location,
                          WGC3Dsizei count, const WGC3Dfloat* v);
  virtual void uniform1i(WGC3Dint location, WGC3Dint x);
  virtual void uniform1iv(WGC3Dint location,
                          WGC3Dsizei count, const WGC3Dint* v);
  virtual void uniform2f(WGC3Dint location, WGC3Dfloat x, WGC3Dfloat y);
  virtual void uniform2fv(WGC3Dint location,
                          WGC3Dsizei count, const WGC3Dfloat* v);
  virtual void uniform2i(WGC3Dint location, WGC3Dint x, WGC3Dint y);
  virtual void uniform2iv(WGC3Dint location,
                          WGC3Dsizei count, const WGC3Dint* v);
  virtual void uniform3f(WGC3Dint location,
                         WGC3Dfloat x, WGC3Dfloat y, WGC3Dfloat z);
  virtual void uniform3fv(WGC3Dint location,
                          WGC3Dsizei count, const WGC3Dfloat* v);
  virtual void uniform3i(WGC3Dint location,
                         WGC3Dint x, WGC3Dint y, WGC3Dint z);
  virtual void uniform3iv(WGC3Dint location,
                          WGC3Dsizei count, const WGC3Dint* v);
  virtual void uniform4f(WGC3Dint location,
                         WGC3Dfloat x, WGC3Dfloat y,
                         WGC3Dfloat z, WGC3Dfloat w);
  virtual void uniform4fv(WGC3Dint location,
                          WGC3Dsizei count, const WGC3Dfloat* v);
  virtual void uniform4i(WGC3Dint location,
                         WGC3Dint x, WGC3Dint y, WGC3Dint z, WGC3Dint w);
  virtual void uniform4iv(WGC3Dint location,
                          WGC3Dsizei count, const WGC3Dint* v);
  virtual void uniformMatrix2fv(WGC3Dint location,
                                WGC3Dsizei count,
                                WGC3Dboolean transpose,
                                const WGC3Dfloat* value);
  virtual void uniformMatrix3fv(WGC3Dint location,
                                WGC3Dsizei count,
                                WGC3Dboolean transpose,
                                const WGC3Dfloat* value);
  virtual void uniformMatrix4fv(WGC3Dint location,
                                WGC3Dsizei count,
                                WGC3Dboolean transpose,
                                const WGC3Dfloat* value);

  virtual void useProgram(WebGLId program);
  virtual void validateProgram(WebGLId program);

  virtual void vertexAttrib1f(WGC3Duint index, WGC3Dfloat x);
  virtual void vertexAttrib1fv(WGC3Duint index, const WGC3Dfloat* values);
  virtual void vertexAttrib2f(WGC3Duint index, WGC3Dfloat x, WGC3Dfloat y);
  virtual void vertexAttrib2fv(WGC3Duint index, const WGC3Dfloat* values);
  virtual void vertexAttrib3f(WGC3Duint index,
                              WGC3Dfloat x, WGC3Dfloat y, WGC3Dfloat z);
  virtual void vertexAttrib3fv(WGC3Duint index, const WGC3Dfloat* values);
  virtual void vertexAttrib4f(WGC3Duint index,
                              WGC3Dfloat x, WGC3Dfloat y,
                              WGC3Dfloat z, WGC3Dfloat w);
  virtual void vertexAttrib4fv(WGC3Duint index, const WGC3Dfloat* values);
  virtual void vertexAttribPointer(WGC3Duint index,
                                   WGC3Dint size,
                                   WGC3Denum type,
                                   WGC3Dboolean normalized,
                                   WGC3Dsizei stride,
                                   WGC3Dintptr offset);

  virtual void viewport(WGC3Dint x, WGC3Dint y,
                        WGC3Dsizei width, WGC3Dsizei height);

  virtual WebGLId createBuffer();
  virtual WebGLId createFramebuffer();
  virtual WebGLId createRenderbuffer();
  virtual WebGLId createTexture();

  virtual void deleteBuffer(WebGLId);
  virtual void deleteFramebuffer(WebGLId);
  virtual void deleteRenderbuffer(WebGLId);
  virtual void deleteTexture(WebGLId);

  virtual WebGLId createProgram();
  virtual WebGLId createShader(WGC3Denum);

  virtual void deleteProgram(WebGLId);
  virtual void deleteShader(WebGLId);

  virtual void synthesizeGLError(WGC3Denum);

  virtual void* mapBufferSubDataCHROMIUM(
      WGC3Denum target, WGC3Dintptr offset,
      WGC3Dsizeiptr size, WGC3Denum access);
  virtual void unmapBufferSubDataCHROMIUM(const void*);
  virtual void* mapTexSubImage2DCHROMIUM(
      WGC3Denum target,
      WGC3Dint level,
      WGC3Dint xoffset,
      WGC3Dint yoffset,
      WGC3Dsizei width,
      WGC3Dsizei height,
      WGC3Denum format,
      WGC3Denum type,
      WGC3Denum access);
  virtual void unmapTexSubImage2DCHROMIUM(const void*);

  virtual void setVisibilityCHROMIUM(bool visible);

  virtual void discardFramebufferEXT(WGC3Denum target,
                                     WGC3Dsizei numAttachments,
                                     const WGC3Denum* attachments);
  virtual void copyTextureToParentTextureCHROMIUM(
      WebGLId texture, WebGLId parentTexture);

  virtual blink::WebString getRequestableExtensionsCHROMIUM();
  virtual void requestExtensionCHROMIUM(const char*);

  virtual void blitFramebufferCHROMIUM(
      WGC3Dint srcX0, WGC3Dint srcY0, WGC3Dint srcX1, WGC3Dint srcY1,
      WGC3Dint dstX0, WGC3Dint dstY0, WGC3Dint dstX1, WGC3Dint dstY1,
      WGC3Dbitfield mask, WGC3Denum filter);
  virtual void renderbufferStorageMultisampleCHROMIUM(
      WGC3Denum target, WGC3Dsizei samples, WGC3Denum internalformat,
      WGC3Dsizei width, WGC3Dsizei height);

  virtual blink::WebString getTranslatedShaderSourceANGLE(WebGLId shader);

  virtual void setContextLostCallback(
      WebGraphicsContext3D::WebGraphicsContextLostCallback* callback);

  virtual void setErrorMessageCallback(
      WebGraphicsContext3D::WebGraphicsErrorMessageCallback* callback);

  virtual void texImageIOSurface2DCHROMIUM(
      WGC3Denum target, WGC3Dint width, WGC3Dint height,
      WGC3Duint ioSurfaceId, WGC3Duint plane);

  virtual void texStorage2DEXT(
      WGC3Denum target, WGC3Dint levels, WGC3Duint internalformat,
      WGC3Dint width, WGC3Dint height);

  virtual WebGLId createQueryEXT();
  virtual void deleteQueryEXT(WebGLId query);
  virtual WGC3Dboolean isQueryEXT(WGC3Duint query);
  virtual void beginQueryEXT(WGC3Denum target, WebGLId query);
  virtual void endQueryEXT(WGC3Denum target);
  virtual void getQueryivEXT(
      WGC3Denum target, WGC3Denum pname, WGC3Dint* params);
  virtual void getQueryObjectuivEXT(
      WebGLId query, WGC3Denum pname, WGC3Duint* params);

  virtual void copyTextureCHROMIUM(WGC3Denum target, WebGLId source_id,
                                   WebGLId dest_id, WGC3Dint level,
                                   WGC3Denum internal_format,
                                   WGC3Denum dest_type);

  virtual void bindUniformLocationCHROMIUM(WebGLId program, WGC3Dint location,
                                           const WGC3Dchar* uniform);

  virtual void shallowFlushCHROMIUM();
  virtual void shallowFinishCHROMIUM();

  virtual void genMailboxCHROMIUM(WGC3Dbyte* mailbox);
  virtual void produceTextureCHROMIUM(WGC3Denum target,
                                      const WGC3Dbyte* mailbox);
  virtual void produceTextureDirectCHROMIUM(WebGLId texture, WGC3Denum target,
                                      const WGC3Dbyte* mailbox);
  virtual void consumeTextureCHROMIUM(WGC3Denum target,
                                      const WGC3Dbyte* mailbox);
  virtual WebGLId createAndConsumeTextureCHROMIUM(WGC3Denum target,
                                      const WGC3Dbyte* mailbox);

  virtual void genValuebuffersCHROMIUM(WGC3Dsizei count, WebGLId* ids);
  virtual WebGLId createValuebufferCHROMIUM();
  virtual void deleteValuebuffersCHROMIUM(WGC3Dsizei count, WebGLId* ids);
  virtual void deleteValuebufferCHROMIUM(WebGLId);
  virtual void bindValuebufferCHROMIUM(WGC3Denum target, WebGLId valuebuffer);
  virtual WGC3Dboolean isValuebufferCHROMIUM(WebGLId renderbuffer);
  virtual void subscribeValueCHROMIUM(WGC3Denum target, WGC3Denum subscription);
  virtual void populateSubscribedValuesCHROMIUM(WGC3Denum target);
  virtual void uniformValuebufferCHROMIUM(WGC3Dint location,
                                          WGC3Denum target,
                                          WGC3Denum subscription);
  virtual void traceBeginCHROMIUM(const WGC3Dchar* category_name,
                                  const WGC3Dchar* trace_name);
  virtual void traceEndCHROMIUM();

  virtual void insertEventMarkerEXT(const WGC3Dchar* marker);
  virtual void pushGroupMarkerEXT(const WGC3Dchar* marker);
  virtual void popGroupMarkerEXT();

  // GL_OES_vertex_array_object
  virtual WebGLId createVertexArrayOES();
  virtual void deleteVertexArrayOES(WebGLId array);
  virtual WGC3Dboolean isVertexArrayOES(WebGLId array);
  virtual void bindVertexArrayOES(WebGLId array);

  virtual void bindTexImage2DCHROMIUM(WGC3Denum target, WGC3Dint image_id);
  virtual void releaseTexImage2DCHROMIUM(WGC3Denum target, WGC3Dint image_id);

  virtual void* mapBufferCHROMIUM(WGC3Denum target, WGC3Denum access);
  virtual WGC3Dboolean unmapBufferCHROMIUM(WGC3Denum target);

  // Async pixel transfer functions.
  virtual void asyncTexImage2DCHROMIUM(
      WGC3Denum target,
      WGC3Dint level,
      WGC3Denum internalformat,
      WGC3Dsizei width,
      WGC3Dsizei height,
      WGC3Dint border,
      WGC3Denum format,
      WGC3Denum type,
      const void* pixels);
  virtual void asyncTexSubImage2DCHROMIUM(
      WGC3Denum target,
      WGC3Dint level,
      WGC3Dint xoffset,
      WGC3Dint yoffset,
      WGC3Dsizei width,
      WGC3Dsizei height,
      WGC3Denum format,
      WGC3Denum type,
      const void* pixels);
  virtual void waitAsyncTexImage2DCHROMIUM(WGC3Denum target);

  // GL_EXT_draw_buffers
  virtual void drawBuffersEXT(
      WGC3Dsizei n,
      const WGC3Denum* bufs);

  // GL_ANGLE_instanced_arrays
  virtual void drawArraysInstancedANGLE(WGC3Denum mode, WGC3Dint first,
      WGC3Dsizei count, WGC3Dsizei primcount);
  virtual void drawElementsInstancedANGLE(WGC3Denum mode, WGC3Dsizei count,
      WGC3Denum type, WGC3Dintptr offset, WGC3Dsizei primcount);
  virtual void vertexAttribDivisorANGLE(WGC3Duint index, WGC3Duint divisor);

  // GL_CHROMIUM_gpu_memory_buffer_image
  virtual WGC3Duint createGpuMemoryBufferImageCHROMIUM(WGC3Dsizei width,
                                                       WGC3Dsizei height,
                                                       WGC3Denum internalformat,
                                                       WGC3Denum usage);
  virtual void destroyImageCHROMIUM(WGC3Duint image_id);

  // GL_EXT_multisampled_render_to_texture
  virtual void framebufferTexture2DMultisampleEXT(WGC3Denum target,
                                    WGC3Denum attachment,
                                    WGC3Denum textarget,
                                    WebGLId texture,
                                    WGC3Dint level,
                                    WGC3Dsizei samples);
  virtual void renderbufferStorageMultisampleEXT(
      WGC3Denum target, WGC3Dsizei samples, WGC3Denum internalformat,
      WGC3Dsizei width, WGC3Dsizei height);

  // OpenGL ES 3.0 functions not represented by pre-existing extensions
  virtual void beginTransformFeedback(WGC3Denum primitiveMode);
  virtual void bindBufferBase(WGC3Denum target, WGC3Duint index,
      WGC3Duint buffer);
  virtual void bindBufferRange(WGC3Denum target, WGC3Duint index,
      WGC3Duint buffer, WGC3Dintptr offset, WGC3Dsizeiptr size);
  virtual void bindSampler(WGC3Duint unit, WebGLId sampler);
  virtual void bindTransformFeedback(WGC3Denum target,
      WebGLId transformfeedback);
  virtual void clearBufferfi(WGC3Denum buffer, WGC3Dint drawbuffer,
      WGC3Dfloat depth, WGC3Dint stencil);
  virtual void clearBufferfv(WGC3Denum buffer, WGC3Dint drawbuffer,
      const WGC3Dfloat *value);
  virtual void clearBufferiv(WGC3Denum buffer, WGC3Dint drawbuffer,
      const WGC3Dint *value);
  virtual void clearBufferuiv(WGC3Denum buffer, WGC3Dint drawbuffer,
      const WGC3Duint *value);
  //virtual WGC3Denum clientWaitSync(WebGLId sync, WGC3Dbitfield flags,
  //    WGC3Duint64 timeout);
  //virtual void compressedTexImage3D(WGC3Denum target, WGC3Dint level,
  //    WGC3Denum internalformat, WGC3Dsizei width, WGC3Dsizei height,
  //    WGC3Dsizei depth, WGC3Dint border, WGC3Dsizei imageSize,
  //    const void *data);
  //virtual void compressedTexSubImage3D(WGC3Denum target, WGC3Dint level,
  //    WGC3Dint xoffset, WGC3Dint yoffset, WGC3Dint zoffset, WGC3Dsizei width,
  //    WGC3Dsizei height, WGC3Dsizei depth, WGC3Denum format,
  //    WGC3Dsizei imageSize, const void *data);
  virtual void copyBufferSubData(WGC3Denum readTarget, WGC3Denum writeTarget,
      WGC3Dintptr readOffset, WGC3Dintptr writeOffset, WGC3Dsizeiptr size);
  virtual void copyTexSubImage3D(WGC3Denum target, WGC3Dint level,
      WGC3Dint xoffset, WGC3Dint yoffset, WGC3Dint zoffset, WGC3Dint x,
      WGC3Dint y, WGC3Dsizei width, WGC3Dsizei height);
  virtual WebGLId createSampler();
  virtual WebGLId createTransformFeedback();
  virtual void deleteSampler(WebGLId sampler);
  //virtual void deleteSync(WebGLId sync);
  virtual void deleteTransformFeedback(WebGLId transformfeedback);
  virtual void endTransformFeedback(void);
  //virtual WebGLId fenceSync(WGC3Denum condition, WGC3Dbitfield flags);
  virtual void framebufferTextureLayer(WGC3Denum target, WGC3Denum attachment,
      WGC3Duint texture, WGC3Dint level, WGC3Dint layer);
  virtual void getActiveUniformBlockName(WGC3Duint program,
      WGC3Duint uniformBlockIndex, WGC3Dsizei bufSize, WGC3Dsizei *length,
      WGC3Dchar *uniformBlockName);
  virtual void getActiveUniformBlockiv(WGC3Duint program,
      WGC3Duint uniformBlockIndex, WGC3Denum pname, WGC3Dint *params);
  //virtual void getActiveUniformsiv(WGC3Duint program, WGC3Dsizei uniformCount,
  //    const WGC3Duint *uniformIndices, WGC3Denum pname, WGC3Dint *params);
  virtual WGC3Dint getFragDataLocation(WGC3Duint program,
      const WGC3Dchar *name);
  virtual void getInternalformativ(WGC3Denum target, WGC3Denum internalformat,
      WGC3Denum pname, WGC3Dsizei bufSize, WGC3Dint *params);
  virtual void getSamplerParameterfv(WGC3Duint sampler, WGC3Denum pname,
      WGC3Dfloat *params);
  virtual void getSamplerParameteriv(WGC3Duint sampler, WGC3Denum pname,
      WGC3Dint *params);
  //virtual void getTransformFeedbackVarying(WGC3Duint program, WGC3Duint index,
  //    WGC3Dsizei bufSize, WGC3Dsizei *length, WGC3Dsizei *size,
  //    WGC3Denum *type, WGC3Dchar *name);
  virtual WGC3Duint getUniformBlockIndex(WGC3Duint program,
      const WGC3Dchar *uniformBlockName);
  //virtual void getUniformIndices(WGC3Duint program, WGC3Dsizei uniformCount,
  //    const WGC3Dchar *const*uniformNames, WGC3Duint *uniformIndices);
  //virtual void getUniformuiv(WGC3Duint program, WGC3Dint location,
  //    WGC3Duint *params);
  //virtual void getVertexAttribIiv(WGC3Duint index, WGC3Denum pname,
  //    WGC3Dint *params);
  //virtual void getVertexAttribIuiv(WGC3Duint index, WGC3Denum pname,
  //    WGC3Duint *params);
  virtual void invalidateFramebuffer(WGC3Denum target,
      WGC3Dsizei numAttachments, const WGC3Denum *attachments);
  virtual void invalidateSubFramebuffer(WGC3Denum target,
      WGC3Dsizei numAttachments, const WGC3Denum *attachments, WGC3Dint x,
      WGC3Dint y, WGC3Dsizei width, WGC3Dsizei height);
  virtual WGC3Dboolean isSampler(WebGLId sampler);
  //virtual WGC3Dboolean isSync(WebGLId sync);
  virtual WGC3Dboolean isTransformFeedback(WGC3Duint id);
  virtual void pauseTransformFeedback(void);
  //virtual void programParameteri(WGC3Duint program, WGC3Denum pname,
  //    WGC3Dint value);
  virtual void readBuffer(WGC3Denum src);
  virtual void resumeTransformFeedback(void);
  virtual void samplerParameterf(WGC3Duint sampler, WGC3Denum pname,
      WGC3Dfloat param);
  virtual void samplerParameterfv(WGC3Duint sampler, WGC3Denum pname,
      const WGC3Dfloat *param);
  virtual void samplerParameteri(WGC3Duint sampler, WGC3Denum pname,
      WGC3Dint param);
  virtual void samplerParameteriv(WGC3Duint sampler, WGC3Denum pname,
      const WGC3Dint *param);
  virtual void texImage3D(WGC3Denum target, WGC3Dint level,
      WGC3Dint internalformat, WGC3Dsizei width, WGC3Dsizei height,
      WGC3Dsizei depth, WGC3Dint border, WGC3Denum format, WGC3Denum type,
      const void *pixels);
  virtual void texStorage3D(WGC3Denum target, WGC3Dsizei levels,
      WGC3Denum internalformat, WGC3Dsizei width, WGC3Dsizei height,
      WGC3Dsizei depth);
  virtual void texSubImage3D(WGC3Denum target, WGC3Dint level, WGC3Dint xoffset,
      WGC3Dint yoffset, WGC3Dint zoffset, WGC3Dsizei width, WGC3Dsizei height,
      WGC3Dsizei depth, WGC3Denum format, WGC3Denum type, const void *pixels);
  virtual void transformFeedbackVaryings(WGC3Duint program, WGC3Dsizei count,
      const WGC3Dchar *const*varyings, WGC3Denum bufferMode);
  virtual void uniform1ui(WGC3Dint location, WGC3Duint x);
  virtual void uniform1uiv(WGC3Dint location, WGC3Dsizei count,
      const WGC3Duint *value);
  virtual void uniform2ui(WGC3Dint location, WGC3Duint x, WGC3Duint y);
  virtual void uniform2uiv(WGC3Dint location, WGC3Dsizei count,
      const WGC3Duint *value);
  virtual void uniform3ui(WGC3Dint location, WGC3Duint x, WGC3Duint y,
      WGC3Duint z);
  virtual void uniform3uiv(WGC3Dint location, WGC3Dsizei count,
      const WGC3Duint *value);
  virtual void uniform4ui(WGC3Dint location, WGC3Duint x, WGC3Duint y,
      WGC3Duint z, WGC3Duint w);
  virtual void uniform4uiv(WGC3Dint location, WGC3Dsizei count,
      const WGC3Duint *value);
  virtual void uniformBlockBinding(WGC3Duint program,
      WGC3Duint uniformBlockIndex, WGC3Duint uniformBlockBinding);
  virtual void uniformMatrix2x3fv(WGC3Dint location, WGC3Dsizei count,
      WGC3Dboolean transpose, const WGC3Dfloat* value);
  virtual void uniformMatrix2x4fv(WGC3Dint location, WGC3Dsizei count,
      WGC3Dboolean transpose, const WGC3Dfloat* value);
  virtual void uniformMatrix3x2fv(WGC3Dint location, WGC3Dsizei count,
      WGC3Dboolean transpose, const WGC3Dfloat* value);
  virtual void uniformMatrix3x4fv(WGC3Dint location, WGC3Dsizei count,
      WGC3Dboolean transpose, const WGC3Dfloat* value);
  virtual void uniformMatrix4x2fv(WGC3Dint location, WGC3Dsizei count,
      WGC3Dboolean transpose, const WGC3Dfloat* value);
  virtual void uniformMatrix4x3fv(WGC3Dint location, WGC3Dsizei count,
      WGC3Dboolean transpose, const WGC3Dfloat* value);
  virtual void vertexAttribI4i(WGC3Duint index, WGC3Dint x, WGC3Dint y,
      WGC3Dint z, WGC3Dint w);
  virtual void vertexAttribI4iv(WGC3Duint index, const WGC3Dint *v);
  virtual void vertexAttribI4ui(WGC3Duint index, WGC3Duint x, WGC3Duint y,
      WGC3Duint z, WGC3Duint w);
  virtual void vertexAttribI4uiv(WGC3Duint index, const WGC3Duint *v);
  virtual void vertexAttribIPointer(WGC3Duint index, WGC3Dint size,
      WGC3Denum type, WGC3Dsizei stride, WGC3Dintptr pointer);
  //virtual void waitSync(WebGLId sync, WGC3Dbitfield flags,
  //    WGC3Duint64 timeout);

  virtual GrGLInterface* createGrGLInterface();

  ::gpu::gles2::GLES2Interface* GetGLInterface() {
    return gl_;
  }

  // Convert WebGL context creation attributes into command buffer / EGL size
  // requests.
  static void ConvertAttributes(
      const blink::WebGraphicsContext3D::Attributes& attributes,
      ::gpu::gles2::ContextCreationAttribHelper* output_attribs);

 protected:
  friend class WebGraphicsContext3DErrorMessageCallback;

  WebGraphicsContext3DImpl();

  ::gpu::gles2::GLES2ImplementationErrorMessageCallback*
      getErrorMessageCallback();
  virtual void OnErrorMessage(const std::string& message, int id);

  void setGLInterface(::gpu::gles2::GLES2Interface* gl) {
    gl_ = gl;
  }

  bool initialized_;
  bool initialize_failed_;

  WebGraphicsContext3D::WebGraphicsContextLostCallback* context_lost_callback_;
  WGC3Denum context_lost_reason_;

  WebGraphicsContext3D::WebGraphicsErrorMessageCallback*
      error_message_callback_;
  scoped_ptr<WebGraphicsContext3DErrorMessageCallback>
      client_error_message_callback_;

  // Errors raised by synthesizeGLError().
  std::vector<WGC3Denum> synthetic_errors_;

  ::gpu::gles2::GLES2Interface* gl_;
  bool lose_context_when_out_of_memory_;
  uint32_t flush_id_;
};

}  // namespace gpu_blink

#endif  // GPU_BLINK_WEBGRAPHICSCONTEXT3D_IMPL_H_
