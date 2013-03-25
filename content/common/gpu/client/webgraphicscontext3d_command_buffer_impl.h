// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_WEBGRAPHICSCONTEXT3D_COMMAND_BUFFER_IMPL_H_
#define CONTENT_COMMON_GPU_CLIENT_WEBGRAPHICSCONTEXT3D_COMMAND_BUFFER_IMPL_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/gpu/client/command_buffer_proxy_impl.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/gl/gpu_preference.h"
#include "ui/gfx/native_widget_types.h"

namespace gpu {

class TransferBuffer;

namespace gles2 {
class GLES2CmdHelper;
class GLES2Implementation;
class GLES2Interface;
}
}

using WebKit::WebGLId;

using WebKit::WGC3Dbyte;
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
using WebKit::WebGraphicsManagedMemoryStats;
using WebKit::WebGraphicsMemoryAllocation;

namespace content {
class GpuChannelHost;
class GpuChannelHostFactory;
struct GpuMemoryAllocationForRenderer;

// TODO(piman): move this logic to the compositor and remove it from the
// context...
class WebGraphicsContext3DSwapBuffersClient {
 public:
  virtual void OnViewContextSwapBuffersPosted() = 0;
  virtual void OnViewContextSwapBuffersComplete() = 0;
  virtual void OnViewContextSwapBuffersAborted() = 0;

 protected:
  virtual ~WebGraphicsContext3DSwapBuffersClient() {}
};

class WebGraphicsContext3DErrorMessageCallback;

class WebGraphicsContext3DCommandBufferImpl
    : public WebKit::WebGraphicsContext3D,
      public base::SupportsWeakPtr<WebGraphicsContext3DCommandBufferImpl> {
 public:
  WebGraphicsContext3DCommandBufferImpl(
      int surface_id,
      const GURL& active_url,
      GpuChannelHostFactory* factory,
      const base::WeakPtr<WebGraphicsContext3DSwapBuffersClient>& swap_client);

  virtual ~WebGraphicsContext3DCommandBufferImpl();

  void InitializeWithCommandBuffer(
      CommandBufferProxyImpl* command_buffer,
      const Attributes& attributes,
      bool bind_generates_resources);

  bool Initialize(const Attributes& attributes,
                  bool bind_generates_resources,
                  CauseForGpuLaunch cause);

  // The following 3 IDs let one uniquely identify this context.
  // Gets the GPU process ID for this context.
  int GetGPUProcessID();

  // Gets the channel ID for this context.
  int GetChannelID();

  // Gets the context ID (relative to the channel).
  int GetContextID();

  CommandBufferProxyImpl* GetCommandBufferProxy() { return command_buffer_; }

  gpu::gles2::GLES2Implementation* GetImplementation() { return real_gl_; }

  // Return true if GPU process reported context lost or there was a
  // problem communicating with the GPU process.
  bool IsCommandBufferContextLost();

  // Create a WebGraphicsContext3DCommandBufferImpl that renders directly to a
  // view. The view and the associated window must not be destroyed until
  // the returned ContentGLContext has been destroyed, otherwise the GPU process
  // might attempt to render to an invalid window handle.
  //
  // NOTE: on Mac OS X, this entry point is only used to set up the
  // accelerated compositor's output. On this platform, we actually pass
  // a gfx::PluginWindowHandle in place of the gfx::NativeViewId,
  // because the facility to allocate a fake PluginWindowHandle is
  // already in place. We could add more entry points and messages to
  // allocate both fake PluginWindowHandles and NativeViewIds and map
  // from fake NativeViewIds to PluginWindowHandles, but this seems like
  // unnecessary complexity at the moment.
  static WebGraphicsContext3DCommandBufferImpl* CreateViewContext(
      GpuChannelHostFactory* factory,
      int32 surface_id,
      const char* allowed_extensions,
      const WebGraphicsContext3D::Attributes& attributes,
      bool bind_generates_resources,
      const GURL& active_url,
      CauseForGpuLaunch cause);

  // Create & initialize a WebGraphicsContext3DCommandBufferImpl.  Return NULL
  // on any failure.
  static WebGraphicsContext3DCommandBufferImpl* CreateOffscreenContext(
      GpuChannelHostFactory* factory,
      const WebGraphicsContext3D::Attributes& attributes,
      const GURL& active_url);

  //----------------------------------------------------------------------
  // WebGraphicsContext3D methods

  // Must be called after initialize() and before any of the following methods.
  // Permanently binds to the first calling thread. Returns false if the
  // graphics context fails to create. Do not call from more than one thread.
  virtual bool makeContextCurrent();

  virtual int width();
  virtual int height();

  virtual bool isGLES2Compliant();

  virtual bool setParentContext(WebGraphicsContext3D* parent_context);

  virtual unsigned int insertSyncPoint();
  virtual void waitSyncPoint(unsigned int);

  virtual void reshape(int width, int height);

  virtual bool readBackFramebuffer(unsigned char* pixels, size_t buffer_size);
  virtual bool readBackFramebuffer(unsigned char* pixels, size_t buffer_size,
                                   WebGLId framebuffer, int width, int height);

  virtual WebGLId getPlatformTextureId();
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

  virtual void getShaderPrecisionFormat(WGC3Denum shadertype,
                                        WGC3Denum precisiontype,
                                        WGC3Dint* range,
                                        WGC3Dint* precision);

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

  virtual void setVisibilityCHROMIUM(bool visible);

  virtual void discardFramebufferEXT(WGC3Denum target,
                                     WGC3Dsizei numAttachments,
                                     const WGC3Denum* attachments);
  virtual void discardBackbufferCHROMIUM();
  virtual void ensureBackbufferCHROMIUM();

  virtual void setMemoryAllocationChangedCallbackCHROMIUM(
      WebGraphicsMemoryAllocationChangedCallbackCHROMIUM* callback);

  virtual void sendManagedMemoryStatsCHROMIUM(
      const WebGraphicsManagedMemoryStats* stats);

  virtual void copyTextureToParentTextureCHROMIUM(
      WebGLId texture, WebGLId parentTexture);

  virtual void rateLimitOffscreenContextCHROMIUM();

  virtual WebKit::WebString getRequestableExtensionsCHROMIUM();
  virtual void requestExtensionCHROMIUM(const char*);

  virtual void blitFramebufferCHROMIUM(
      WGC3Dint srcX0, WGC3Dint srcY0, WGC3Dint srcX1, WGC3Dint srcY1,
      WGC3Dint dstX0, WGC3Dint dstY0, WGC3Dint dstX1, WGC3Dint dstY1,
      WGC3Dbitfield mask, WGC3Denum filter);
  virtual void renderbufferStorageMultisampleCHROMIUM(
      WGC3Denum target, WGC3Dsizei samples, WGC3Denum internalformat,
      WGC3Dsizei width, WGC3Dsizei height);

  virtual WebKit::WebString getTranslatedShaderSourceANGLE(WebGLId shader);

  virtual void setContextLostCallback(
      WebGraphicsContext3D::WebGraphicsContextLostCallback* callback);

  virtual WGC3Denum getGraphicsResetStatusARB();

  virtual void setErrorMessageCallback(
      WebGraphicsContext3D::WebGraphicsErrorMessageCallback* callback);

  virtual void setSwapBuffersCompleteCallbackCHROMIUM(
      WebGraphicsContext3D::
          WebGraphicsSwapBuffersCompleteCallbackCHROMIUM* callback);

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
                                   WGC3Denum internal_format);

  virtual void bindUniformLocationCHROMIUM(WebGLId program, WGC3Dint location,
                                           const WGC3Dchar* uniform);

  virtual void shallowFlushCHROMIUM();

  virtual void genMailboxCHROMIUM(WGC3Dbyte* mailbox);
  virtual void produceTextureCHROMIUM(WGC3Denum target,
                                      const WGC3Dbyte* mailbox);
  virtual void consumeTextureCHROMIUM(WGC3Denum target,
                                      const WGC3Dbyte* mailbox);

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

  virtual WebGLId createStreamTextureCHROMIUM(WebGLId texture);
  virtual void destroyStreamTextureCHROMIUM(WebGLId texture);

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

 protected:
  virtual GrGLInterface* onCreateGrGLInterface();

 private:
  // These are the same error codes as used by EGL.
  enum Error {
    SUCCESS               = 0x3000,
    BAD_ATTRIBUTE         = 0x3004,
    CONTEXT_LOST          = 0x300E
  };
  // WebGraphicsContext3DCommandBufferImpl configuration attributes. Those in
  // the 16-bit range are the same as used by EGL. Those outside the 16-bit
  // range are unique to Chromium. Attributes are matched using a closest fit
  // algorithm.
  enum Attribute {
    ALPHA_SIZE                = 0x3021,
    BLUE_SIZE                 = 0x3022,
    GREEN_SIZE                = 0x3023,
    RED_SIZE                  = 0x3024,
    DEPTH_SIZE                = 0x3025,
    STENCIL_SIZE              = 0x3026,
    SAMPLES                   = 0x3031,
    SAMPLE_BUFFERS            = 0x3032,
    HEIGHT                    = 0x3056,
    WIDTH                     = 0x3057,
    NONE                      = 0x3038,  // Attrib list = terminator
    SHARE_RESOURCES           = 0x10000,
    BIND_GENERATES_RESOURCES  = 0x10001
  };
  friend class WebGraphicsContext3DErrorMessageCallback;

  // Initialize the underlying GL context. May be called multiple times; second
  // and subsequent calls are ignored. Must be called from the thread that is
  // going to use this object to issue GL commands (which might not be the main
  // thread).
  bool MaybeInitializeGL(const char* allowed_extensions);

  bool InitializeCommandBuffer(
      bool onscreen,
      const char* allowed_extensions);

  bool SetParent(WebGraphicsContext3DCommandBufferImpl* parent_context);

  void Destroy();

  // Create a CommandBufferProxy that renders directly to a view. The view and
  // the associated window must not be destroyed until the returned
  // CommandBufferProxy has been destroyed, otherwise the GPU process might
  // attempt to render to an invalid window handle.
  //
  // NOTE: on Mac OS X, this entry point is only used to set up the
  // accelerated compositor's output. On this platform, we actually pass
  // a gfx::PluginWindowHandle in place of the gfx::NativeViewId,
  // because the facility to allocate a fake PluginWindowHandle is
  // already in place. We could add more entry points and messages to
  // allocate both fake PluginWindowHandles and NativeViewIds and map
  // from fake NativeViewIds to PluginWindowHandles, but this seems like
  // unnecessary complexity at the moment.
  bool CreateContext(bool onscreen,
                     const char* allowed_extensions);

  // SwapBuffers callback.
  void OnSwapBuffersComplete();
  virtual void OnContextLost();
  virtual void OnErrorMessage(const std::string& message, int id);

  // Check if we should call into the swap client. We can only do that on the
  // main thread.
  bool ShouldUseSwapClient();

  // MemoryAllocationChanged callback.
  void OnMemoryAllocationChanged(
      WebGraphicsMemoryAllocationChangedCallbackCHROMIUM* callback,
      const GpuMemoryAllocationForRenderer& allocation);

  // Convert the gpu cutoff enum to the WebKit enum.
  static WebGraphicsMemoryAllocation::PriorityCutoff WebkitPriorityCutoff(
      GpuMemoryAllocationForRenderer::PriorityCutoff priorityCutoff);

  bool initialize_failed_;

  // The channel factory to talk to the GPU process
  GpuChannelHostFactory* factory_;

  bool visible_;
  bool free_command_buffer_when_invisible_;

  // State needed by MaybeInitializeGL.
  scoped_refptr<GpuChannelHost> host_;
  int32 surface_id_;
  GURL active_url_;
  base::WeakPtr<WebGraphicsContext3DSwapBuffersClient> swap_client_;

  WebGraphicsContext3D::WebGraphicsContextLostCallback* context_lost_callback_;
  WGC3Denum context_lost_reason_;

  WebGraphicsContext3D::WebGraphicsErrorMessageCallback*
      error_message_callback_;
  scoped_ptr<WebGraphicsContext3DErrorMessageCallback>
      client_error_message_callback_;

  WebGraphicsContext3D::WebGraphicsSwapBuffersCompleteCallbackCHROMIUM*
      swapbuffers_complete_callback_;

  WebKit::WebGraphicsContext3D::Attributes attributes_;
  gfx::GpuPreference gpu_preference_;
  int cached_width_, cached_height_;

  // For tracking which FBO is bound.
  WebGLId bound_fbo_;

  // Errors raised by synthesizeGLError().
  std::vector<WGC3Denum> synthetic_errors_;

  base::WeakPtrFactory<WebGraphicsContext3DCommandBufferImpl> weak_ptr_factory_;

  std::vector<uint8> scanline_;
  void FlipVertically(uint8* framebuffer,
                      unsigned int width,
                      unsigned int height);

  bool initialized_;
  WebGraphicsContext3DCommandBufferImpl* parent_;
  uint32 parent_texture_id_;
  CommandBufferProxyImpl* command_buffer_;
  gpu::gles2::GLES2CmdHelper* gles2_helper_;
  gpu::TransferBuffer* transfer_buffer_;
  gpu::gles2::GLES2Interface* gl_;
  gpu::gles2::GLES2Implementation* real_gl_;
  gpu::gles2::GLES2Interface* trace_gl_;
  Error last_error_;
  int frame_number_;
  bool bind_generates_resources_;
  bool use_echo_for_swap_ack_;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_WEBGRAPHICSCONTEXT3D_COMMAND_BUFFER_IMPL_H_
