// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_WEBGRAPHICSCONTEXT3D_COMMAND_BUFFER_IMPL_H_
#define CHROME_RENDERER_WEBGRAPHICSCONTEXT3D_COMMAND_BUFFER_IMPL_H_
#pragma once

#if defined(ENABLE_GPU)

#include <vector>

#include "base/scoped_ptr.h"
#include "chrome/renderer/ggl/ggl.h"
#include "gfx/native_widget_types.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebGraphicsContext3D.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

#if !defined(OS_MACOSX)
#define FLIP_FRAMEBUFFER_VERTICALLY
#endif

class GpuChannelHost;
class CommandBufferProxy;

namespace gpu {
namespace gles2 {
class GLES2Implementation;
}
}

using WebKit::WebGLId;

#if defined(USE_WGC3D_TYPES)

using WebKit::WGC3Dchar;
using WebKit::WGC3Denum;
using WebKit::WGC3Dboolean;
using WebKit::WGC3Dbitfield;
using WebKit::WGC3Dint;
using WebKit::WGC3Dsizei;
using WebKit::WGC3Duint;
using WebKit::WGC3Dfloat;
using WebKit::WGC3Dclampf;
using WebKit::WGC3Dintptr;
using WebKit::WGC3Dsizeiptr;

class WebGraphicsContext3DCommandBufferImpl
    : public WebKit::WebGraphicsContext3D {
 public:

  WebGraphicsContext3DCommandBufferImpl();
  virtual ~WebGraphicsContext3DCommandBufferImpl();

  //----------------------------------------------------------------------
  // WebGraphicsContext3D methods
  virtual bool initialize(WebGraphicsContext3D::Attributes attributes,
                          WebKit::WebView*,
                          bool renderDirectlyToWebView);

  virtual bool makeContextCurrent();

  virtual int width();
  virtual int height();

  virtual bool isGLES2Compliant();

  virtual void reshape(int width, int height);

  virtual bool readBackFramebuffer(unsigned char* pixels, size_t buffer_size);

  virtual WebGLId getPlatformTextureId();
  virtual void prepareTexture();

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

  virtual Attributes getContextAttributes();

  virtual WGC3Denum getError();

  virtual bool isContextLost();

  virtual void getFloatv(WGC3Denum pname, WGC3Dfloat* value);

  virtual void getFramebufferAttachmentParameteriv(WGC3Denum target,
                                                   WGC3Denum attachment,
                                                   WGC3Denum pname,
                                                   WGC3Dint* value);

  virtual void getIntegerv(WGC3Denum pname, WGC3Dint* value);

  virtual void getProgramiv(WebGLId program, WGC3Denum pname, WGC3Dint* value);

  virtual WebKit::WebString getProgramInfoLog(WebGLId program);

  virtual void getRenderbufferParameteriv(WGC3Denum target,
                                          WGC3Denum pname,
                                          WGC3Dint* value);

  virtual void getShaderiv(WebGLId shader, WGC3Denum pname, WGC3Dint* value);

  virtual WebKit::WebString getShaderInfoLog(WebGLId shader);

  // TBD
  // void glGetShaderPrecisionFormat (GLenum shadertype,
  //                                  GLenum precisiontype,
  //                                  GLint* range,
  //                                  GLint* precision);

  virtual WebKit::WebString getShaderSource(WebGLId shader);
  virtual WebKit::WebString getString(WGC3Denum name);

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

  // Support for buffer creation and deletion
  virtual WebGLId createBuffer();
  virtual WebGLId createFramebuffer();
  virtual WebGLId createProgram();
  virtual WebGLId createRenderbuffer();
  virtual WebGLId createShader(WGC3Denum);
  virtual WebGLId createTexture();

  virtual void deleteBuffer(WebGLId);
  virtual void deleteFramebuffer(WebGLId);
  virtual void deleteProgram(WebGLId);
  virtual void deleteRenderbuffer(WebGLId);
  virtual void deleteShader(WebGLId);
  virtual void deleteTexture(WebGLId);

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

  virtual void copyTextureToParentTextureCHROMIUM(
      WebGLId texture, WebGLId parentTexture);

  virtual WebKit::WebString getRequestableExtensionsCHROMIUM();
  virtual void requestExtensionCHROMIUM(const char*);

  virtual void blitFramebufferCHROMIUM(
      WGC3Dint srcX0, WGC3Dint srcY0, WGC3Dint srcX1, WGC3Dint srcY1,
      WGC3Dint dstX0, WGC3Dint dstY0, WGC3Dint dstX1, WGC3Dint dstY1,
      WGC3Dbitfield mask, WGC3Denum filter);
  virtual void renderbufferStorageMultisampleCHROMIUM(
      WGC3Denum target, WGC3Dsizei samples, WGC3Denum internalformat,
      WGC3Dsizei width, WGC3Dsizei height);

  virtual WebGLId createCompositorTexture(WGC3Dsizei width, WGC3Dsizei height);
  virtual void deleteCompositorTexture(WebGLId parent_texture);
  virtual void copyTextureToCompositor(WebGLId texture,
                                       WebGLId parent_texture);

  ggl::Context* context() { return context_; }

 private:
  // SwapBuffers callback;
  void OnSwapBuffers();

  // The GGL context we use for OpenGL rendering.
  ggl::Context* context_;
  // If rendering directly to WebView, weak pointer to it.
  WebKit::WebView* web_view_;

  WebKit::WebGraphicsContext3D::Attributes attributes_;
  int cached_width_, cached_height_;

  // For tracking which FBO is bound.
  WebGLId bound_fbo_;

  // Errors raised by synthesizeGLError().
  std::vector<WGC3Denum> synthetic_errors_;

#ifdef FLIP_FRAMEBUFFER_VERTICALLY
  scoped_ptr<uint8> scanline_;
  void FlipVertically(uint8* framebuffer,
                      unsigned int width,
                      unsigned int height);
#endif
};

#else  // USE_WGC3D_TYPES

class WebGraphicsContext3DCommandBufferImpl
    : public WebKit::WebGraphicsContext3D {
 public:

  WebGraphicsContext3DCommandBufferImpl();
  virtual ~WebGraphicsContext3DCommandBufferImpl();

  //----------------------------------------------------------------------
  // WebGraphicsContext3D methods
  virtual bool initialize(WebGraphicsContext3D::Attributes attributes,
                          WebKit::WebView*,
                          bool renderDirectlyToWebView);

  virtual bool makeContextCurrent();

  virtual int width();
  virtual int height();

  virtual int sizeInBytes(int type);

  virtual bool isGLES2Compliant();
  virtual bool isGLES2NPOTStrict();
  virtual bool isErrorGeneratedOnOutOfBoundsAccesses();

  virtual void reshape(int width, int height);

  virtual bool readBackFramebuffer(unsigned char* pixels, size_t buffer_size);

  virtual unsigned int getPlatformTextureId();
  virtual void prepareTexture();

  virtual void activeTexture(unsigned long texture);
  virtual void attachShader(WebGLId program, WebGLId shader);
  virtual void bindAttribLocation(WebGLId program, unsigned long index,
                                  const char* name);
  virtual void bindBuffer(unsigned long target, WebGLId buffer);
  virtual void bindFramebuffer(unsigned long target, WebGLId framebuffer);
  virtual void bindRenderbuffer(unsigned long target, WebGLId renderbuffer);
  virtual void bindTexture(unsigned long target, WebGLId texture);
  virtual void blendColor(double red, double green,
                          double blue, double alpha);
  virtual void blendEquation(unsigned long mode);
  virtual void blendEquationSeparate(unsigned long modeRGB,
                                     unsigned long modeAlpha);
  virtual void blendFunc(unsigned long sfactor, unsigned long dfactor);
  virtual void blendFuncSeparate(unsigned long srcRGB,
                                 unsigned long dstRGB,
                                 unsigned long srcAlpha,
                                 unsigned long dstAlpha);

  virtual void bufferData(unsigned long target, int size,
                          const void* data, unsigned long usage);
  virtual void bufferSubData(unsigned long target, long offset,
                             int size, const void* data);

  virtual unsigned long checkFramebufferStatus(unsigned long target);
  virtual void clear(unsigned long mask);
  virtual void clearColor(double red, double green,
                          double blue, double alpha);
  virtual void clearDepth(double depth);
  virtual void clearStencil(long s);
  virtual void colorMask(bool red, bool green, bool blue, bool alpha);
  virtual void compileShader(WebGLId shader);

  virtual void copyTexImage2D(unsigned long target,
                              long level,
                              unsigned long internalformat,
                              long x,
                              long y,
                              unsigned long width,
                              unsigned long height,
                              long border);
  virtual void copyTexSubImage2D(unsigned long target,
                                 long level,
                                 long xoffset,
                                 long yoffset,
                                 long x,
                                 long y,
                                 unsigned long width,
                                 unsigned long height);
  virtual void cullFace(unsigned long mode);
  virtual void depthFunc(unsigned long func);
  virtual void depthMask(bool flag);
  virtual void depthRange(double zNear, double zFar);
  virtual void detachShader(WebGLId program, WebGLId shader);
  virtual void disable(unsigned long cap);
  virtual void disableVertexAttribArray(unsigned long index);
  virtual void drawArrays(unsigned long mode, long first, long count);
  virtual void drawElements(unsigned long mode,
                            unsigned long count,
                            unsigned long type,
                            long offset);

  virtual void enable(unsigned long cap);
  virtual void enableVertexAttribArray(unsigned long index);
  virtual void finish();
  virtual void flush();
  virtual void framebufferRenderbuffer(unsigned long target,
                                       unsigned long attachment,
                                       unsigned long renderbuffertarget,
                                       WebGLId renderbuffer);
  virtual void framebufferTexture2D(unsigned long target,
                                    unsigned long attachment,
                                    unsigned long textarget,
                                    WebGLId texture,
                                    long level);
  virtual void frontFace(unsigned long mode);
  virtual void generateMipmap(unsigned long target);

  virtual bool getActiveAttrib(WebGLId program,
                               unsigned long index,
                               ActiveInfo&);
  virtual bool getActiveUniform(WebGLId program,
                                unsigned long index,
                                ActiveInfo&);

  virtual void getAttachedShaders(WebGLId program,
                                  int maxCount,
                                  int* count,
                                  unsigned int* shaders);

  virtual int  getAttribLocation(WebGLId program, const char* name);

  virtual void getBooleanv(unsigned long pname, unsigned char* value);

  virtual void getBufferParameteriv(unsigned long target,
                                    unsigned long pname,
                                    int* value);

  virtual Attributes getContextAttributes();

  virtual unsigned long getError();

  virtual bool isContextLost();

  virtual void getFloatv(unsigned long pname, float* value);

  virtual void getFramebufferAttachmentParameteriv(unsigned long target,
                                                   unsigned long attachment,
                                                   unsigned long pname,
                                                   int* value);

  virtual void getIntegerv(unsigned long pname, int* value);

  virtual void getProgramiv(WebGLId program, unsigned long pname, int* value);

  virtual WebKit::WebString getProgramInfoLog(WebGLId program);

  virtual void getRenderbufferParameteriv(unsigned long target,
                                          unsigned long pname,
                                          int* value);

  virtual void getShaderiv(WebGLId shader, unsigned long pname, int* value);

  virtual WebKit::WebString getShaderInfoLog(WebGLId shader);

  // TBD
  // void glGetShaderPrecisionFormat (GLenum shadertype,
  //                                  GLenum precisiontype,
  //                                  GLint* range,
  //                                  GLint* precision);

  virtual WebKit::WebString getShaderSource(WebGLId shader);
  virtual WebKit::WebString getString(unsigned long name);

  virtual void getTexParameterfv(unsigned long target,
                                 unsigned long pname,
                                 float* value);
  virtual void getTexParameteriv(unsigned long target,
                                 unsigned long pname,
                                 int* value);

  virtual void getUniformfv(WebGLId program, long location, float* value);
  virtual void getUniformiv(WebGLId program, long location, int* value);

  virtual long getUniformLocation(WebGLId program, const char* name);

  virtual void getVertexAttribfv(unsigned long index, unsigned long pname,
                                 float* value);
  virtual void getVertexAttribiv(unsigned long index, unsigned long pname,
                                 int* value);

  virtual long getVertexAttribOffset(unsigned long index, unsigned long pname);

  virtual void hint(unsigned long target, unsigned long mode);
  virtual bool isBuffer(WebGLId buffer);
  virtual bool isEnabled(unsigned long cap);
  virtual bool isFramebuffer(WebGLId framebuffer);
  virtual bool isProgram(WebGLId program);
  virtual bool isRenderbuffer(WebGLId renderbuffer);
  virtual bool isShader(WebGLId shader);
  virtual bool isTexture(WebGLId texture);
  virtual void lineWidth(double);
  virtual void linkProgram(WebGLId program);
  virtual void pixelStorei(unsigned long pname, long param);
  virtual void polygonOffset(double factor, double units);

  virtual void readPixels(long x,
                          long y,
                          unsigned long width,
                          unsigned long height,
                          unsigned long format,
                          unsigned long type,
                          void* pixels);

  virtual void releaseShaderCompiler();
  virtual void renderbufferStorage(unsigned long target,
                                   unsigned long internalformat,
                                   unsigned long width,
                                   unsigned long height);
  virtual void sampleCoverage(double value, bool invert);
  virtual void scissor(long x, long y,
                       unsigned long width, unsigned long height);
  virtual void shaderSource(WebGLId shader, const char* string);
  virtual void stencilFunc(unsigned long func, long ref, unsigned long mask);
  virtual void stencilFuncSeparate(unsigned long face,
                                   unsigned long func,
                                   long ref,
                                   unsigned long mask);
  virtual void stencilMask(unsigned long mask);
  virtual void stencilMaskSeparate(unsigned long face, unsigned long mask);
  virtual void stencilOp(unsigned long fail,
                         unsigned long zfail,
                         unsigned long zpass);
  virtual void stencilOpSeparate(unsigned long face,
                                 unsigned long fail,
                                 unsigned long zfail,
                                 unsigned long zpass);

  virtual void texImage2D(unsigned target,
                          unsigned level,
                          unsigned internalformat,
                          unsigned width,
                          unsigned height,
                          unsigned border,
                          unsigned format,
                          unsigned type,
                          const void* pixels);

  virtual void texParameterf(unsigned target, unsigned pname, float param);
  virtual void texParameteri(unsigned target, unsigned pname, int param);

  virtual void texSubImage2D(unsigned target,
                             unsigned level,
                             unsigned xoffset,
                             unsigned yoffset,
                             unsigned width,
                             unsigned height,
                             unsigned format,
                             unsigned type,
                             const void* pixels);

  virtual void uniform1f(long location, float x);
  virtual void uniform1fv(long location, int count, float* v);
  virtual void uniform1i(long location, int x);
  virtual void uniform1iv(long location, int count, int* v);
  virtual void uniform2f(long location, float x, float y);
  virtual void uniform2fv(long location, int count, float* v);
  virtual void uniform2i(long location, int x, int y);
  virtual void uniform2iv(long location, int count, int* v);
  virtual void uniform3f(long location, float x, float y, float z);
  virtual void uniform3fv(long location, int count, float* v);
  virtual void uniform3i(long location, int x, int y, int z);
  virtual void uniform3iv(long location, int count, int* v);
  virtual void uniform4f(long location, float x, float y, float z, float w);
  virtual void uniform4fv(long location, int count, float* v);
  virtual void uniform4i(long location, int x, int y, int z, int w);
  virtual void uniform4iv(long location, int count, int* v);
  virtual void uniformMatrix2fv(long location, int count, bool transpose,
                                const float* value);
  virtual void uniformMatrix3fv(long location, int count, bool transpose,
                                const float* value);
  virtual void uniformMatrix4fv(long location, int count, bool transpose,
                                const float* value);

  virtual void useProgram(WebGLId program);
  virtual void validateProgram(WebGLId program);

  virtual void vertexAttrib1f(unsigned long indx, float x);
  virtual void vertexAttrib1fv(unsigned long indx, const float* values);
  virtual void vertexAttrib2f(unsigned long indx, float x, float y);
  virtual void vertexAttrib2fv(unsigned long indx, const float* values);
  virtual void vertexAttrib3f(unsigned long indx, float x, float y, float z);
  virtual void vertexAttrib3fv(unsigned long indx, const float* values);
  virtual void vertexAttrib4f(unsigned long indx,
                              float x, float y, float z, float w);
  virtual void vertexAttrib4fv(unsigned long indx, const float* values);
  virtual void vertexAttribPointer(unsigned long indx, int size, int type,
                                   bool normalized, unsigned long stride,
                                   unsigned long offset);

  virtual void viewport(long x, long y,
                        unsigned long width, unsigned long height);

  // Support for buffer creation and deletion
  virtual unsigned createBuffer();
  virtual unsigned createFramebuffer();
  virtual unsigned createProgram();
  virtual unsigned createRenderbuffer();
  virtual unsigned createShader(unsigned long);
  virtual unsigned createTexture();

  virtual void deleteBuffer(unsigned);
  virtual void deleteFramebuffer(unsigned);
  virtual void deleteProgram(unsigned);
  virtual void deleteRenderbuffer(unsigned);
  virtual void deleteShader(unsigned);
  virtual void deleteTexture(unsigned);

  virtual void synthesizeGLError(unsigned long error);

  virtual void* mapBufferSubDataCHROMIUM(
      unsigned target, int offset, int size, unsigned access);
  virtual void unmapBufferSubDataCHROMIUM(const void*);
  virtual void* mapTexSubImage2DCHROMIUM(
      unsigned target,
      int level,
      int xoffset,
      int yoffset,
      int width,
      int height,
      unsigned format,
      unsigned type,
      unsigned access);
  virtual void unmapTexSubImage2DCHROMIUM(const void*);

  virtual void copyTextureToParentTextureCHROMIUM(
      unsigned texture, unsigned parentTexture);

  virtual WebKit::WebString getRequestableExtensionsCHROMIUM();
  virtual void requestExtensionCHROMIUM(const char*);

  virtual void blitFramebufferCHROMIUM(
      int srcX0, int srcY0, int srcX1, int srcY1,
      int dstX0, int dstY0, int dstX1, int dstY1,
      unsigned mask, unsigned filter);
  virtual void renderbufferStorageMultisampleCHROMIUM(
      unsigned long target, int samples, unsigned internalformat,
      unsigned width, unsigned height);

  virtual unsigned createCompositorTexture(unsigned width, unsigned height);
  virtual void deleteCompositorTexture(unsigned parent_texture);
  virtual void copyTextureToCompositor(unsigned texture,
                                       unsigned parent_texture);

  ggl::Context* context() { return context_; }

 private:
  // SwapBuffers callback;
  void OnSwapBuffers();

  // The GGL context we use for OpenGL rendering.
  ggl::Context* context_;
  // If rendering directly to WebView, weak pointer to it.
  WebKit::WebView* web_view_;

  WebKit::WebGraphicsContext3D::Attributes attributes_;
  int cached_width_, cached_height_;

  // For tracking which FBO is bound.
  unsigned int bound_fbo_;

  // Errors raised by synthesizeGLError().
  std::vector<unsigned long> synthetic_errors_;

#ifdef FLIP_FRAMEBUFFER_VERTICALLY
  scoped_ptr<uint8> scanline_;
  void FlipVertically(uint8* framebuffer,
                      unsigned int width,
                      unsigned int height);
#endif
};

#endif  // USE_WGC3D_TYPES

#endif  // defined(ENABLE_GPU)
#endif  // CHROME_RENDERER_WEBGRAPHICSCONTEXT3D_COMMAND_BUFFER_IMPL_H_

