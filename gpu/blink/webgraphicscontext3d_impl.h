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

  virtual void loseContextCHROMIUM(blink::WGC3Denum current,
                                   blink::WGC3Denum other);

  virtual void reshapeWithScaleFactor(
      int width, int height, float scale_factor);

  virtual void prepareTexture();
  virtual void postSubBufferCHROMIUM(int x, int y, int width, int height);

  virtual void activeTexture(blink::WGC3Denum texture);
  virtual void attachShader(blink::WebGLId program, blink::WebGLId shader);
  virtual void bindAttribLocation(blink::WebGLId program,
                                  blink::WGC3Duint index,
                                  const blink::WGC3Dchar* name);
  virtual void bindBuffer(blink::WGC3Denum target, blink::WebGLId buffer);
  virtual void bindFramebuffer(blink::WGC3Denum target,
                               blink::WebGLId framebuffer);
  virtual void bindRenderbuffer(blink::WGC3Denum target,
                                blink::WebGLId renderbuffer);
  virtual void bindTexture(blink::WGC3Denum target, blink::WebGLId texture);
  virtual void blendColor(blink::WGC3Dclampf red,
                          blink::WGC3Dclampf green,
                          blink::WGC3Dclampf blue,
                          blink::WGC3Dclampf alpha);
  virtual void blendEquation(blink::WGC3Denum mode);
  virtual void blendEquationSeparate(blink::WGC3Denum modeRGB,
                                     blink::WGC3Denum modeAlpha);
  virtual void blendFunc(blink::WGC3Denum sfactor, blink::WGC3Denum dfactor);
  virtual void blendFuncSeparate(blink::WGC3Denum srcRGB,
                                 blink::WGC3Denum dstRGB,
                                 blink::WGC3Denum srcAlpha,
                                 blink::WGC3Denum dstAlpha);

  virtual void bufferData(blink::WGC3Denum target,
                          blink::WGC3Dsizeiptr size,
                          const void* data,
                          blink::WGC3Denum usage);
  virtual void bufferSubData(blink::WGC3Denum target,
                             blink::WGC3Dintptr offset,
                             blink::WGC3Dsizeiptr size,
                             const void* data);

  virtual blink::WGC3Denum checkFramebufferStatus(blink::WGC3Denum target);
  virtual void clear(blink::WGC3Dbitfield mask);
  virtual void clearColor(blink::WGC3Dclampf red,
                          blink::WGC3Dclampf green,
                          blink::WGC3Dclampf blue,
                          blink::WGC3Dclampf alpha);
  virtual void clearDepth(blink::WGC3Dclampf depth);
  virtual void clearStencil(blink::WGC3Dint s);
  virtual void colorMask(blink::WGC3Dboolean red,
                         blink::WGC3Dboolean green,
                         blink::WGC3Dboolean blue,
                         blink::WGC3Dboolean alpha);
  virtual void compileShader(blink::WebGLId shader);

  virtual void compressedTexImage2D(blink::WGC3Denum target,
                                    blink::WGC3Dint level,
                                    blink::WGC3Denum internalformat,
                                    blink::WGC3Dsizei width,
                                    blink::WGC3Dsizei height,
                                    blink::WGC3Dint border,
                                    blink::WGC3Dsizei imageSize,
                                    const void* data);
  virtual void compressedTexSubImage2D(blink::WGC3Denum target,
                                       blink::WGC3Dint level,
                                       blink::WGC3Dint xoffset,
                                       blink::WGC3Dint yoffset,
                                       blink::WGC3Dsizei width,
                                       blink::WGC3Dsizei height,
                                       blink::WGC3Denum format,
                                       blink::WGC3Dsizei imageSize,
                                       const void* data);
  virtual void copyTexImage2D(blink::WGC3Denum target,
                              blink::WGC3Dint level,
                              blink::WGC3Denum internalformat,
                              blink::WGC3Dint x,
                              blink::WGC3Dint y,
                              blink::WGC3Dsizei width,
                              blink::WGC3Dsizei height,
                              blink::WGC3Dint border);
  virtual void copyTexSubImage2D(blink::WGC3Denum target,
                                 blink::WGC3Dint level,
                                 blink::WGC3Dint xoffset,
                                 blink::WGC3Dint yoffset,
                                 blink::WGC3Dint x,
                                 blink::WGC3Dint y,
                                 blink::WGC3Dsizei width,
                                 blink::WGC3Dsizei height);
  virtual void cullFace(blink::WGC3Denum mode);
  virtual void depthFunc(blink::WGC3Denum func);
  virtual void depthMask(blink::WGC3Dboolean flag);
  virtual void depthRange(blink::WGC3Dclampf zNear, blink::WGC3Dclampf zFar);
  virtual void detachShader(blink::WebGLId program, blink::WebGLId shader);
  virtual void disable(blink::WGC3Denum cap);
  virtual void disableVertexAttribArray(blink::WGC3Duint index);
  virtual void drawArrays(blink::WGC3Denum mode,
                          blink::WGC3Dint first,
                          blink::WGC3Dsizei count);
  virtual void drawElements(blink::WGC3Denum mode,
                            blink::WGC3Dsizei count,
                            blink::WGC3Denum type,
                            blink::WGC3Dintptr offset);

  virtual void enable(blink::WGC3Denum cap);
  virtual void enableVertexAttribArray(blink::WGC3Duint index);
  virtual void finish();
  virtual void flush();
  virtual void framebufferRenderbuffer(blink::WGC3Denum target,
                                       blink::WGC3Denum attachment,
                                       blink::WGC3Denum renderbuffertarget,
                                       blink::WebGLId renderbuffer);
  virtual void framebufferTexture2D(blink::WGC3Denum target,
                                    blink::WGC3Denum attachment,
                                    blink::WGC3Denum textarget,
                                    blink::WebGLId texture,
                                    blink::WGC3Dint level);
  virtual void frontFace(blink::WGC3Denum mode);
  virtual void generateMipmap(blink::WGC3Denum target);

  virtual bool getActiveAttrib(blink::WebGLId program,
                               blink::WGC3Duint index,
                               ActiveInfo&);
  virtual bool getActiveUniform(blink::WebGLId program,
                                blink::WGC3Duint index,
                                ActiveInfo&);

  virtual void getAttachedShaders(blink::WebGLId program,
                                  blink::WGC3Dsizei maxCount,
                                  blink::WGC3Dsizei* count,
                                  blink::WebGLId* shaders);

  virtual blink::WGC3Dint getAttribLocation(blink::WebGLId program,
                                            const blink::WGC3Dchar* name);

  virtual void getBooleanv(blink::WGC3Denum pname, blink::WGC3Dboolean* value);

  virtual void getBufferParameteriv(blink::WGC3Denum target,
                                    blink::WGC3Denum pname,
                                    blink::WGC3Dint* value);

  virtual blink::WGC3Denum getError();

  virtual void getFloatv(blink::WGC3Denum pname, blink::WGC3Dfloat* value);

  virtual void getFramebufferAttachmentParameteriv(blink::WGC3Denum target,
                                                   blink::WGC3Denum attachment,
                                                   blink::WGC3Denum pname,
                                                   blink::WGC3Dint* value);

  virtual void getIntegerv(blink::WGC3Denum pname, blink::WGC3Dint* value);

  virtual void getProgramiv(blink::WebGLId program,
                            blink::WGC3Denum pname,
                            blink::WGC3Dint* value);

  virtual blink::WebString getProgramInfoLog(blink::WebGLId program);

  virtual void getRenderbufferParameteriv(blink::WGC3Denum target,
                                          blink::WGC3Denum pname,
                                          blink::WGC3Dint* value);

  virtual void getShaderiv(blink::WebGLId shader,
                           blink::WGC3Denum pname,
                           blink::WGC3Dint* value);

  virtual blink::WebString getShaderInfoLog(blink::WebGLId shader);

  virtual void getShaderPrecisionFormat(blink::WGC3Denum shadertype,
                                        blink::WGC3Denum precisiontype,
                                        blink::WGC3Dint* range,
                                        blink::WGC3Dint* precision);

  virtual blink::WebString getShaderSource(blink::WebGLId shader);
  virtual blink::WebString getString(blink::WGC3Denum name);

  virtual void getTexParameterfv(blink::WGC3Denum target,
                                 blink::WGC3Denum pname,
                                 blink::WGC3Dfloat* value);
  virtual void getTexParameteriv(blink::WGC3Denum target,
                                 blink::WGC3Denum pname,
                                 blink::WGC3Dint* value);

  virtual void getUniformfv(blink::WebGLId program,
                            blink::WGC3Dint location,
                            blink::WGC3Dfloat* value);
  virtual void getUniformiv(blink::WebGLId program,
                            blink::WGC3Dint location,
                            blink::WGC3Dint* value);

  virtual blink::WGC3Dint getUniformLocation(blink::WebGLId program,
                                             const blink::WGC3Dchar* name);

  virtual void getVertexAttribfv(blink::WGC3Duint index,
                                 blink::WGC3Denum pname,
                                 blink::WGC3Dfloat* value);
  virtual void getVertexAttribiv(blink::WGC3Duint index,
                                 blink::WGC3Denum pname,
                                 blink::WGC3Dint* value);

  virtual blink::WGC3Dsizeiptr getVertexAttribOffset(blink::WGC3Duint index,
                                                     blink::WGC3Denum pname);

  virtual void hint(blink::WGC3Denum target, blink::WGC3Denum mode);
  virtual blink::WGC3Dboolean isBuffer(blink::WebGLId buffer);
  virtual blink::WGC3Dboolean isEnabled(blink::WGC3Denum cap);
  virtual blink::WGC3Dboolean isFramebuffer(blink::WebGLId framebuffer);
  virtual blink::WGC3Dboolean isProgram(blink::WebGLId program);
  virtual blink::WGC3Dboolean isRenderbuffer(blink::WebGLId renderbuffer);
  virtual blink::WGC3Dboolean isShader(blink::WebGLId shader);
  virtual blink::WGC3Dboolean isTexture(blink::WebGLId texture);
  virtual void lineWidth(blink::WGC3Dfloat);
  virtual void linkProgram(blink::WebGLId program);
  virtual void pixelStorei(blink::WGC3Denum pname, blink::WGC3Dint param);
  virtual void polygonOffset(blink::WGC3Dfloat factor, blink::WGC3Dfloat units);

  virtual void readPixels(blink::WGC3Dint x,
                          blink::WGC3Dint y,
                          blink::WGC3Dsizei width,
                          blink::WGC3Dsizei height,
                          blink::WGC3Denum format,
                          blink::WGC3Denum type,
                          void* pixels);

  virtual void releaseShaderCompiler();
  virtual void renderbufferStorage(blink::WGC3Denum target,
                                   blink::WGC3Denum internalformat,
                                   blink::WGC3Dsizei width,
                                   blink::WGC3Dsizei height);
  virtual void sampleCoverage(blink::WGC3Dfloat value,
                              blink::WGC3Dboolean invert);
  virtual void scissor(blink::WGC3Dint x,
                       blink::WGC3Dint y,
                       blink::WGC3Dsizei width,
                       blink::WGC3Dsizei height);
  virtual void shaderSource(blink::WebGLId shader,
                            const blink::WGC3Dchar* string);
  virtual void stencilFunc(blink::WGC3Denum func,
                           blink::WGC3Dint ref,
                           blink::WGC3Duint mask);
  virtual void stencilFuncSeparate(blink::WGC3Denum face,
                                   blink::WGC3Denum func,
                                   blink::WGC3Dint ref,
                                   blink::WGC3Duint mask);
  virtual void stencilMask(blink::WGC3Duint mask);
  virtual void stencilMaskSeparate(blink::WGC3Denum face,
                                   blink::WGC3Duint mask);
  virtual void stencilOp(blink::WGC3Denum fail,
                         blink::WGC3Denum zfail,
                         blink::WGC3Denum zpass);
  virtual void stencilOpSeparate(blink::WGC3Denum face,
                                 blink::WGC3Denum fail,
                                 blink::WGC3Denum zfail,
                                 blink::WGC3Denum zpass);

  virtual void texImage2D(blink::WGC3Denum target,
                          blink::WGC3Dint level,
                          blink::WGC3Denum internalformat,
                          blink::WGC3Dsizei width,
                          blink::WGC3Dsizei height,
                          blink::WGC3Dint border,
                          blink::WGC3Denum format,
                          blink::WGC3Denum type,
                          const void* pixels);

  virtual void texParameterf(blink::WGC3Denum target,
                             blink::WGC3Denum pname,
                             blink::WGC3Dfloat param);
  virtual void texParameteri(blink::WGC3Denum target,
                             blink::WGC3Denum pname,
                             blink::WGC3Dint param);

  virtual void texSubImage2D(blink::WGC3Denum target,
                             blink::WGC3Dint level,
                             blink::WGC3Dint xoffset,
                             blink::WGC3Dint yoffset,
                             blink::WGC3Dsizei width,
                             blink::WGC3Dsizei height,
                             blink::WGC3Denum format,
                             blink::WGC3Denum type,
                             const void* pixels);

  virtual void uniform1f(blink::WGC3Dint location, blink::WGC3Dfloat x);
  virtual void uniform1fv(blink::WGC3Dint location,
                          blink::WGC3Dsizei count,
                          const blink::WGC3Dfloat* v);
  virtual void uniform1i(blink::WGC3Dint location, blink::WGC3Dint x);
  virtual void uniform1iv(blink::WGC3Dint location,
                          blink::WGC3Dsizei count,
                          const blink::WGC3Dint* v);
  virtual void uniform2f(blink::WGC3Dint location,
                         blink::WGC3Dfloat x,
                         blink::WGC3Dfloat y);
  virtual void uniform2fv(blink::WGC3Dint location,
                          blink::WGC3Dsizei count,
                          const blink::WGC3Dfloat* v);
  virtual void uniform2i(blink::WGC3Dint location,
                         blink::WGC3Dint x,
                         blink::WGC3Dint y);
  virtual void uniform2iv(blink::WGC3Dint location,
                          blink::WGC3Dsizei count,
                          const blink::WGC3Dint* v);
  virtual void uniform3f(blink::WGC3Dint location,
                         blink::WGC3Dfloat x,
                         blink::WGC3Dfloat y,
                         blink::WGC3Dfloat z);
  virtual void uniform3fv(blink::WGC3Dint location,
                          blink::WGC3Dsizei count,
                          const blink::WGC3Dfloat* v);
  virtual void uniform3i(blink::WGC3Dint location,
                         blink::WGC3Dint x,
                         blink::WGC3Dint y,
                         blink::WGC3Dint z);
  virtual void uniform3iv(blink::WGC3Dint location,
                          blink::WGC3Dsizei count,
                          const blink::WGC3Dint* v);
  virtual void uniform4f(blink::WGC3Dint location,
                         blink::WGC3Dfloat x,
                         blink::WGC3Dfloat y,
                         blink::WGC3Dfloat z,
                         blink::WGC3Dfloat w);
  virtual void uniform4fv(blink::WGC3Dint location,
                          blink::WGC3Dsizei count,
                          const blink::WGC3Dfloat* v);
  virtual void uniform4i(blink::WGC3Dint location,
                         blink::WGC3Dint x,
                         blink::WGC3Dint y,
                         blink::WGC3Dint z,
                         blink::WGC3Dint w);
  virtual void uniform4iv(blink::WGC3Dint location,
                          blink::WGC3Dsizei count,
                          const blink::WGC3Dint* v);
  virtual void uniformMatrix2fv(blink::WGC3Dint location,
                                blink::WGC3Dsizei count,
                                blink::WGC3Dboolean transpose,
                                const blink::WGC3Dfloat* value);
  virtual void uniformMatrix3fv(blink::WGC3Dint location,
                                blink::WGC3Dsizei count,
                                blink::WGC3Dboolean transpose,
                                const blink::WGC3Dfloat* value);
  virtual void uniformMatrix4fv(blink::WGC3Dint location,
                                blink::WGC3Dsizei count,
                                blink::WGC3Dboolean transpose,
                                const blink::WGC3Dfloat* value);

  virtual void useProgram(blink::WebGLId program);
  virtual void validateProgram(blink::WebGLId program);

  virtual void vertexAttrib1f(blink::WGC3Duint index, blink::WGC3Dfloat x);
  virtual void vertexAttrib1fv(blink::WGC3Duint index,
                               const blink::WGC3Dfloat* values);
  virtual void vertexAttrib2f(blink::WGC3Duint index,
                              blink::WGC3Dfloat x,
                              blink::WGC3Dfloat y);
  virtual void vertexAttrib2fv(blink::WGC3Duint index,
                               const blink::WGC3Dfloat* values);
  virtual void vertexAttrib3f(blink::WGC3Duint index,
                              blink::WGC3Dfloat x,
                              blink::WGC3Dfloat y,
                              blink::WGC3Dfloat z);
  virtual void vertexAttrib3fv(blink::WGC3Duint index,
                               const blink::WGC3Dfloat* values);
  virtual void vertexAttrib4f(blink::WGC3Duint index,
                              blink::WGC3Dfloat x,
                              blink::WGC3Dfloat y,
                              blink::WGC3Dfloat z,
                              blink::WGC3Dfloat w);
  virtual void vertexAttrib4fv(blink::WGC3Duint index,
                               const blink::WGC3Dfloat* values);
  virtual void vertexAttribPointer(blink::WGC3Duint index,
                                   blink::WGC3Dint size,
                                   blink::WGC3Denum type,
                                   blink::WGC3Dboolean normalized,
                                   blink::WGC3Dsizei stride,
                                   blink::WGC3Dintptr offset);

  virtual void viewport(blink::WGC3Dint x,
                        blink::WGC3Dint y,
                        blink::WGC3Dsizei width,
                        blink::WGC3Dsizei height);

  virtual blink::WebGLId createBuffer();
  virtual blink::WebGLId createFramebuffer();
  virtual blink::WebGLId createRenderbuffer();
  virtual blink::WebGLId createTexture();

  virtual void deleteBuffer(blink::WebGLId);
  virtual void deleteFramebuffer(blink::WebGLId);
  virtual void deleteRenderbuffer(blink::WebGLId);
  virtual void deleteTexture(blink::WebGLId);

  virtual blink::WebGLId createProgram();
  virtual blink::WebGLId createShader(blink::WGC3Denum);

  virtual void deleteProgram(blink::WebGLId);
  virtual void deleteShader(blink::WebGLId);

  virtual void synthesizeGLError(blink::WGC3Denum);

  virtual void* mapBufferSubDataCHROMIUM(blink::WGC3Denum target,
                                         blink::WGC3Dintptr offset,
                                         blink::WGC3Dsizeiptr size,
                                         blink::WGC3Denum access);
  virtual void unmapBufferSubDataCHROMIUM(const void*);
  virtual void* mapTexSubImage2DCHROMIUM(blink::WGC3Denum target,
                                         blink::WGC3Dint level,
                                         blink::WGC3Dint xoffset,
                                         blink::WGC3Dint yoffset,
                                         blink::WGC3Dsizei width,
                                         blink::WGC3Dsizei height,
                                         blink::WGC3Denum format,
                                         blink::WGC3Denum type,
                                         blink::WGC3Denum access);
  virtual void unmapTexSubImage2DCHROMIUM(const void*);

  virtual void setVisibilityCHROMIUM(bool visible);

  virtual void discardFramebufferEXT(blink::WGC3Denum target,
                                     blink::WGC3Dsizei numAttachments,
                                     const blink::WGC3Denum* attachments);
  virtual void copyTextureToParentTextureCHROMIUM(blink::WebGLId texture,
                                                  blink::WebGLId parentTexture);

  virtual blink::WebString getRequestableExtensionsCHROMIUM();
  virtual void requestExtensionCHROMIUM(const char*);

  virtual void blitFramebufferCHROMIUM(blink::WGC3Dint srcX0,
                                       blink::WGC3Dint srcY0,
                                       blink::WGC3Dint srcX1,
                                       blink::WGC3Dint srcY1,
                                       blink::WGC3Dint dstX0,
                                       blink::WGC3Dint dstY0,
                                       blink::WGC3Dint dstX1,
                                       blink::WGC3Dint dstY1,
                                       blink::WGC3Dbitfield mask,
                                       blink::WGC3Denum filter);
  virtual void renderbufferStorageMultisampleCHROMIUM(
      blink::WGC3Denum target,
      blink::WGC3Dsizei samples,
      blink::WGC3Denum internalformat,
      blink::WGC3Dsizei width,
      blink::WGC3Dsizei height);

  virtual blink::WebString getTranslatedShaderSourceANGLE(
      blink::WebGLId shader);

  virtual void setContextLostCallback(
      WebGraphicsContext3D::WebGraphicsContextLostCallback* callback);

  virtual void setErrorMessageCallback(
      WebGraphicsContext3D::WebGraphicsErrorMessageCallback* callback);

  virtual void texImageIOSurface2DCHROMIUM(blink::WGC3Denum target,
                                           blink::WGC3Dint width,
                                           blink::WGC3Dint height,
                                           blink::WGC3Duint ioSurfaceId,
                                           blink::WGC3Duint plane);

  virtual void texStorage2DEXT(blink::WGC3Denum target,
                               blink::WGC3Dint levels,
                               blink::WGC3Duint internalformat,
                               blink::WGC3Dint width,
                               blink::WGC3Dint height);

  virtual blink::WebGLId createQueryEXT();
  virtual void deleteQueryEXT(blink::WebGLId query);
  virtual blink::WGC3Dboolean isQueryEXT(blink::WGC3Duint query);
  virtual void beginQueryEXT(blink::WGC3Denum target, blink::WebGLId query);
  virtual void endQueryEXT(blink::WGC3Denum target);
  virtual void getQueryivEXT(blink::WGC3Denum target,
                             blink::WGC3Denum pname,
                             blink::WGC3Dint* params);
  virtual void getQueryObjectuivEXT(blink::WebGLId query,
                                    blink::WGC3Denum pname,
                                    blink::WGC3Duint* params);

  // TODO(dshwang): Remove |level| in Blink and then remove it.
  void copyTextureCHROMIUM(blink::WGC3Denum target,
                           blink::WebGLId source_id,
                           blink::WebGLId dest_id,
                           blink::WGC3Dint level,
                           blink::WGC3Denum internal_format,
                           blink::WGC3Denum dest_type) override;

  void copySubTextureCHROMIUM(blink::WGC3Denum target,
                              blink::WebGLId source_id,
                              blink::WebGLId dest_id,
                              blink::WGC3Dint level,
                              blink::WGC3Dint xoffset,
                              blink::WGC3Dint yoffset) override;

  virtual void copyTextureCHROMIUM(blink::WGC3Denum target,
                                   blink::WebGLId source_id,
                                   blink::WebGLId dest_id,
                                   blink::WGC3Denum internal_format,
                                   blink::WGC3Denum dest_type);

  virtual void copySubTextureCHROMIUM(blink::WGC3Denum target,
                                      blink::WebGLId sourceId,
                                      blink::WebGLId destId,
                                      blink::WGC3Dint xoffset,
                                      blink::WGC3Dint yoffset);

  virtual void bindUniformLocationCHROMIUM(blink::WebGLId program,
                                           blink::WGC3Dint location,
                                           const blink::WGC3Dchar* uniform);

  virtual void shallowFlushCHROMIUM();
  virtual void shallowFinishCHROMIUM();

  virtual void genMailboxCHROMIUM(blink::WGC3Dbyte* mailbox);
  virtual void produceTextureCHROMIUM(blink::WGC3Denum target,
                                      const blink::WGC3Dbyte* mailbox);
  virtual void produceTextureDirectCHROMIUM(blink::WebGLId texture,
                                            blink::WGC3Denum target,
                                            const blink::WGC3Dbyte* mailbox);
  virtual void consumeTextureCHROMIUM(blink::WGC3Denum target,
                                      const blink::WGC3Dbyte* mailbox);
  virtual blink::WebGLId createAndConsumeTextureCHROMIUM(
      blink::WGC3Denum target,
      const blink::WGC3Dbyte* mailbox);

  virtual void genValuebuffersCHROMIUM(blink::WGC3Dsizei count,
                                       blink::WebGLId* ids);
  virtual blink::WebGLId createValuebufferCHROMIUM();
  virtual void deleteValuebuffersCHROMIUM(blink::WGC3Dsizei count,
                                          blink::WebGLId* ids);
  virtual void deleteValuebufferCHROMIUM(blink::WebGLId);
  virtual void bindValuebufferCHROMIUM(blink::WGC3Denum target,
                                       blink::WebGLId valuebuffer);
  virtual blink::WGC3Dboolean isValuebufferCHROMIUM(
      blink::WebGLId renderbuffer);
  virtual void subscribeValueCHROMIUM(blink::WGC3Denum target,
                                      blink::WGC3Denum subscription);
  virtual void populateSubscribedValuesCHROMIUM(blink::WGC3Denum target);
  virtual void uniformValuebufferCHROMIUM(blink::WGC3Dint location,
                                          blink::WGC3Denum target,
                                          blink::WGC3Denum subscription);
  virtual void traceBeginCHROMIUM(const blink::WGC3Dchar* category_name,
                                  const blink::WGC3Dchar* trace_name);
  virtual void traceEndCHROMIUM();

  virtual void insertEventMarkerEXT(const blink::WGC3Dchar* marker);
  virtual void pushGroupMarkerEXT(const blink::WGC3Dchar* marker);
  virtual void popGroupMarkerEXT();

  // GL_OES_vertex_array_object
  virtual blink::WebGLId createVertexArrayOES();
  virtual void deleteVertexArrayOES(blink::WebGLId array);
  virtual blink::WGC3Dboolean isVertexArrayOES(blink::WebGLId array);
  virtual void bindVertexArrayOES(blink::WebGLId array);

  virtual void bindTexImage2DCHROMIUM(blink::WGC3Denum target,
                                      blink::WGC3Dint image_id);
  virtual void releaseTexImage2DCHROMIUM(blink::WGC3Denum target,
                                         blink::WGC3Dint image_id);

  virtual void* mapBufferCHROMIUM(blink::WGC3Denum target,
                                  blink::WGC3Denum access);
  virtual blink::WGC3Dboolean unmapBufferCHROMIUM(blink::WGC3Denum target);

  // Async pixel transfer functions.
  virtual void asyncTexImage2DCHROMIUM(blink::WGC3Denum target,
                                       blink::WGC3Dint level,
                                       blink::WGC3Denum internalformat,
                                       blink::WGC3Dsizei width,
                                       blink::WGC3Dsizei height,
                                       blink::WGC3Dint border,
                                       blink::WGC3Denum format,
                                       blink::WGC3Denum type,
                                       const void* pixels);
  virtual void asyncTexSubImage2DCHROMIUM(blink::WGC3Denum target,
                                          blink::WGC3Dint level,
                                          blink::WGC3Dint xoffset,
                                          blink::WGC3Dint yoffset,
                                          blink::WGC3Dsizei width,
                                          blink::WGC3Dsizei height,
                                          blink::WGC3Denum format,
                                          blink::WGC3Denum type,
                                          const void* pixels);
  virtual void waitAsyncTexImage2DCHROMIUM(blink::WGC3Denum target);

  // GL_EXT_draw_buffers
  virtual void drawBuffersEXT(blink::WGC3Dsizei n,
                              const blink::WGC3Denum* bufs);

  // GL_ANGLE_instanced_arrays
  virtual void drawArraysInstancedANGLE(blink::WGC3Denum mode,
                                        blink::WGC3Dint first,
                                        blink::WGC3Dsizei count,
                                        blink::WGC3Dsizei primcount);
  virtual void drawElementsInstancedANGLE(blink::WGC3Denum mode,
                                          blink::WGC3Dsizei count,
                                          blink::WGC3Denum type,
                                          blink::WGC3Dintptr offset,
                                          blink::WGC3Dsizei primcount);
  virtual void vertexAttribDivisorANGLE(blink::WGC3Duint index,
                                        blink::WGC3Duint divisor);

  // GL_CHROMIUM_gpu_memory_buffer_image
  virtual blink::WGC3Duint createGpuMemoryBufferImageCHROMIUM(
      blink::WGC3Dsizei width,
      blink::WGC3Dsizei height,
      blink::WGC3Denum internalformat,
      blink::WGC3Denum usage);
  virtual void destroyImageCHROMIUM(blink::WGC3Duint image_id);

  // GL_EXT_multisampled_render_to_texture
  virtual void framebufferTexture2DMultisampleEXT(blink::WGC3Denum target,
                                                  blink::WGC3Denum attachment,
                                                  blink::WGC3Denum textarget,
                                                  blink::WebGLId texture,
                                                  blink::WGC3Dint level,
                                                  blink::WGC3Dsizei samples);
  virtual void renderbufferStorageMultisampleEXT(
      blink::WGC3Denum target,
      blink::WGC3Dsizei samples,
      blink::WGC3Denum internalformat,
      blink::WGC3Dsizei width,
      blink::WGC3Dsizei height);

  // OpenGL ES 3.0 functions not represented by pre-existing extensions
  virtual void beginTransformFeedback(blink::WGC3Denum primitiveMode);
  virtual void bindBufferBase(blink::WGC3Denum target,
                              blink::WGC3Duint index,
                              blink::WGC3Duint buffer);
  virtual void bindBufferRange(blink::WGC3Denum target,
                               blink::WGC3Duint index,
                               blink::WGC3Duint buffer,
                               blink::WGC3Dintptr offset,
                               blink::WGC3Dsizeiptr size);
  virtual void bindSampler(blink::WGC3Duint unit, blink::WebGLId sampler);
  virtual void bindTransformFeedback(blink::WGC3Denum target,
                                     blink::WebGLId transformfeedback);
  virtual void clearBufferfi(blink::WGC3Denum buffer,
                             blink::WGC3Dint drawbuffer,
                             blink::WGC3Dfloat depth,
                             blink::WGC3Dint stencil);
  virtual void clearBufferfv(blink::WGC3Denum buffer,
                             blink::WGC3Dint drawbuffer,
                             const blink::WGC3Dfloat* value);
  virtual void clearBufferiv(blink::WGC3Denum buffer,
                             blink::WGC3Dint drawbuffer,
                             const blink::WGC3Dint* value);
  virtual void clearBufferuiv(blink::WGC3Denum buffer,
                              blink::WGC3Dint drawbuffer,
                              const blink::WGC3Duint* value);
  // virtual blink::WGC3Denum clientWaitSync(blink::WebGLId sync,
  //                                         blink::WGC3Dbitfield flags,
  //                                         blink::WGC3Duint64 timeout);
  // virtual void compressedTexImage3D(blink::WGC3Denum target, blink::WGC3Dint
  // level,
  //    blink::WGC3Denum internalformat, blink::WGC3Dsizei width,
  //    WGC3Dsizei height, WGC3Dsizei depth, blink::WGC3Dint border,
  //    WGC3Dsizei imageSize, const void *data);
  // virtual void compressedTexSubImage3D(blink::WGC3Denum target,
  //    blink::WGC3Dint level, blink::WGC3Dint xoffset, blink::WGC3Dint yoffset,
  //    blink::WGC3Dint zoffset,
  //    blink::WGC3Dsizei width, blink::WGC3Dsizei height,
  //    blink::WGC3Dsizei depth, blink::WGC3Denum format,
  //    blink::WGC3Dsizei imageSize, const void *data);
  virtual void copyBufferSubData(blink::WGC3Denum readTarget,
                                 blink::WGC3Denum writeTarget,
                                 blink::WGC3Dintptr readOffset,
                                 blink::WGC3Dintptr writeOffset,
                                 blink::WGC3Dsizeiptr size);
  virtual void copyTexSubImage3D(blink::WGC3Denum target,
                                 blink::WGC3Dint level,
                                 blink::WGC3Dint xoffset,
                                 blink::WGC3Dint yoffset,
                                 blink::WGC3Dint zoffset,
                                 blink::WGC3Dint x,
                                 blink::WGC3Dint y,
                                 blink::WGC3Dsizei width,
                                 blink::WGC3Dsizei height);
  virtual blink::WebGLId createSampler();
  virtual blink::WebGLId createTransformFeedback();
  virtual void deleteSampler(blink::WebGLId sampler);
  // virtual void deleteSync(blink::WebGLId sync);
  virtual void deleteTransformFeedback(blink::WebGLId transformfeedback);
  virtual void endTransformFeedback(void);
  // virtual blink::WebGLId fenceSync(blink::WGC3Denum condition,
  //                                  blink::WGC3Dbitfield flags);
  virtual void framebufferTextureLayer(blink::WGC3Denum target,
                                       blink::WGC3Denum attachment,
                                       blink::WGC3Duint texture,
                                       blink::WGC3Dint level,
                                       blink::WGC3Dint layer);
  virtual void getActiveUniformBlockName(blink::WGC3Duint program,
                                         blink::WGC3Duint uniformBlockIndex,
                                         blink::WGC3Dsizei bufSize,
                                         blink::WGC3Dsizei* length,
                                         blink::WGC3Dchar* uniformBlockName);
  virtual void getActiveUniformBlockiv(blink::WGC3Duint program,
                                       blink::WGC3Duint uniformBlockIndex,
                                       blink::WGC3Denum pname,
                                       blink::WGC3Dint* params);
  // virtual void getActiveUniformsiv(blink::WGC3Duint program,
  //    blink::WGC3Dsizei uniformCount, const blink::WGC3Duint *uniformIndices,
  //    blink::WGC3Denum pname, blink::WGC3Dint *params);
  virtual blink::WGC3Dint getFragDataLocation(blink::WGC3Duint program,
                                              const blink::WGC3Dchar* name);
  virtual void getInternalformativ(blink::WGC3Denum target,
                                   blink::WGC3Denum internalformat,
                                   blink::WGC3Denum pname,
                                   blink::WGC3Dsizei bufSize,
                                   blink::WGC3Dint* params);
  virtual void getSamplerParameterfv(blink::WGC3Duint sampler,
                                     blink::WGC3Denum pname,
                                     blink::WGC3Dfloat* params);
  virtual void getSamplerParameteriv(blink::WGC3Duint sampler,
                                     blink::WGC3Denum pname,
                                     blink::WGC3Dint* params);
  // virtual void getTransformFeedbackVarying(blink::WGC3Duint program,
  // blink::WGC3Duint index,
  //    blink::WGC3Dsizei bufSize, blink::WGC3Dsizei *length,
  //    blink::WGC3Dsizei *size, blink::WGC3Denum *type,
  //    blink::WGC3Dchar *name);
  virtual blink::WGC3Duint getUniformBlockIndex(
      blink::WGC3Duint program,
      const blink::WGC3Dchar* uniformBlockName);
  // virtual void getUniformIndices(blink::WGC3Duint program,
  //    blink::WGC3Dsizei uniformCount,
  //    const blink::WGC3Dchar *const*uniformNames,
  //    blink::WGC3Duint *uniformIndices);
  // virtual void getUniformuiv(blink::WGC3Duint program, blink::WGC3Dint
  // location,
  //    blink::WGC3Duint *params);
  // virtual void getVertexAttribIiv(blink::WGC3Duint index, blink::WGC3Denum
  // pname,
  //    blink::WGC3Dint *params);
  // virtual void getVertexAttribIuiv(blink::WGC3Duint index, blink::WGC3Denum
  // pname,
  //    blink::WGC3Duint *params);
  virtual void invalidateFramebuffer(blink::WGC3Denum target,
                                     blink::WGC3Dsizei numAttachments,
                                     const blink::WGC3Denum* attachments);
  virtual void invalidateSubFramebuffer(blink::WGC3Denum target,
                                        blink::WGC3Dsizei numAttachments,
                                        const blink::WGC3Denum* attachments,
                                        blink::WGC3Dint x,
                                        blink::WGC3Dint y,
                                        blink::WGC3Dsizei width,
                                        blink::WGC3Dsizei height);
  virtual blink::WGC3Dboolean isSampler(blink::WebGLId sampler);
  // virtual blink::WGC3Dboolean isSync(blink::WebGLId sync);
  virtual blink::WGC3Dboolean isTransformFeedback(blink::WGC3Duint id);
  virtual void pauseTransformFeedback(void);
  // virtual void programParameteri(blink::WGC3Duint program, blink::WGC3Denum
  // pname,
  //    blink::WGC3Dint value);
  virtual void readBuffer(blink::WGC3Denum src);
  virtual void resumeTransformFeedback(void);
  virtual void samplerParameterf(blink::WGC3Duint sampler,
                                 blink::WGC3Denum pname,
                                 blink::WGC3Dfloat param);
  virtual void samplerParameterfv(blink::WGC3Duint sampler,
                                  blink::WGC3Denum pname,
                                  const blink::WGC3Dfloat* param);
  virtual void samplerParameteri(blink::WGC3Duint sampler,
                                 blink::WGC3Denum pname,
                                 blink::WGC3Dint param);
  virtual void samplerParameteriv(blink::WGC3Duint sampler,
                                  blink::WGC3Denum pname,
                                  const blink::WGC3Dint* param);
  virtual void texImage3D(blink::WGC3Denum target,
                          blink::WGC3Dint level,
                          blink::WGC3Dint internalformat,
                          blink::WGC3Dsizei width,
                          blink::WGC3Dsizei height,
                          blink::WGC3Dsizei depth,
                          blink::WGC3Dint border,
                          blink::WGC3Denum format,
                          blink::WGC3Denum type,
                          const void* pixels);
  virtual void texStorage3D(blink::WGC3Denum target,
                            blink::WGC3Dsizei levels,
                            blink::WGC3Denum internalformat,
                            blink::WGC3Dsizei width,
                            blink::WGC3Dsizei height,
                            blink::WGC3Dsizei depth);
  virtual void texSubImage3D(blink::WGC3Denum target,
                             blink::WGC3Dint level,
                             blink::WGC3Dint xoffset,
                             blink::WGC3Dint yoffset,
                             blink::WGC3Dint zoffset,
                             blink::WGC3Dsizei width,
                             blink::WGC3Dsizei height,
                             blink::WGC3Dsizei depth,
                             blink::WGC3Denum format,
                             blink::WGC3Denum type,
                             const void* pixels);
  virtual void transformFeedbackVaryings(
      blink::WGC3Duint program,
      blink::WGC3Dsizei count,
      const blink::WGC3Dchar* const* varyings,
      blink::WGC3Denum bufferMode);
  virtual void uniform1ui(blink::WGC3Dint location, blink::WGC3Duint x);
  virtual void uniform1uiv(blink::WGC3Dint location,
                           blink::WGC3Dsizei count,
                           const blink::WGC3Duint* value);
  virtual void uniform2ui(blink::WGC3Dint location,
                          blink::WGC3Duint x,
                          blink::WGC3Duint y);
  virtual void uniform2uiv(blink::WGC3Dint location,
                           blink::WGC3Dsizei count,
                           const blink::WGC3Duint* value);
  virtual void uniform3ui(blink::WGC3Dint location,
                          blink::WGC3Duint x,
                          blink::WGC3Duint y,
                          blink::WGC3Duint z);
  virtual void uniform3uiv(blink::WGC3Dint location,
                           blink::WGC3Dsizei count,
                           const blink::WGC3Duint* value);
  virtual void uniform4ui(blink::WGC3Dint location,
                          blink::WGC3Duint x,
                          blink::WGC3Duint y,
                          blink::WGC3Duint z,
                          blink::WGC3Duint w);
  virtual void uniform4uiv(blink::WGC3Dint location,
                           blink::WGC3Dsizei count,
                           const blink::WGC3Duint* value);
  virtual void uniformBlockBinding(blink::WGC3Duint program,
                                   blink::WGC3Duint uniformBlockIndex,
                                   blink::WGC3Duint uniformBlockBinding);
  virtual void uniformMatrix2x3fv(blink::WGC3Dint location,
                                  blink::WGC3Dsizei count,
                                  blink::WGC3Dboolean transpose,
                                  const blink::WGC3Dfloat* value);
  virtual void uniformMatrix2x4fv(blink::WGC3Dint location,
                                  blink::WGC3Dsizei count,
                                  blink::WGC3Dboolean transpose,
                                  const blink::WGC3Dfloat* value);
  virtual void uniformMatrix3x2fv(blink::WGC3Dint location,
                                  blink::WGC3Dsizei count,
                                  blink::WGC3Dboolean transpose,
                                  const blink::WGC3Dfloat* value);
  virtual void uniformMatrix3x4fv(blink::WGC3Dint location,
                                  blink::WGC3Dsizei count,
                                  blink::WGC3Dboolean transpose,
                                  const blink::WGC3Dfloat* value);
  virtual void uniformMatrix4x2fv(blink::WGC3Dint location,
                                  blink::WGC3Dsizei count,
                                  blink::WGC3Dboolean transpose,
                                  const blink::WGC3Dfloat* value);
  virtual void uniformMatrix4x3fv(blink::WGC3Dint location,
                                  blink::WGC3Dsizei count,
                                  blink::WGC3Dboolean transpose,
                                  const blink::WGC3Dfloat* value);
  virtual void vertexAttribI4i(blink::WGC3Duint index,
                               blink::WGC3Dint x,
                               blink::WGC3Dint y,
                               blink::WGC3Dint z,
                               blink::WGC3Dint w);
  virtual void vertexAttribI4iv(blink::WGC3Duint index,
                                const blink::WGC3Dint* v);
  virtual void vertexAttribI4ui(blink::WGC3Duint index,
                                blink::WGC3Duint x,
                                blink::WGC3Duint y,
                                blink::WGC3Duint z,
                                blink::WGC3Duint w);
  virtual void vertexAttribI4uiv(blink::WGC3Duint index,
                                 const blink::WGC3Duint* v);
  virtual void vertexAttribIPointer(blink::WGC3Duint index,
                                    blink::WGC3Dint size,
                                    blink::WGC3Denum type,
                                    blink::WGC3Dsizei stride,
                                    blink::WGC3Dintptr pointer);
  // virtual void waitSync(blink::WebGLId sync, blink::WGC3Dbitfield flags,
  //    blink::WGC3Duint64 timeout);

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
  blink::WGC3Denum context_lost_reason_;

  WebGraphicsContext3D::WebGraphicsErrorMessageCallback*
      error_message_callback_;
  scoped_ptr<WebGraphicsContext3DErrorMessageCallback>
      client_error_message_callback_;

  // Errors raised by synthesizeGLError().
  std::vector<blink::WGC3Denum> synthetic_errors_;

  ::gpu::gles2::GLES2Interface* gl_;
  bool lose_context_when_out_of_memory_;
  uint32_t flush_id_;
};

}  // namespace gpu_blink

#endif  // GPU_BLINK_WEBGRAPHICSCONTEXT3D_IMPL_H_
