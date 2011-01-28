// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(ENABLE_GPU)

#include "chrome/renderer/webgraphicscontext3d_command_buffer_impl.h"

#include <GLES2/gl2.h>
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include <GLES2/gl2ext.h>

#include <algorithm>

#include "base/string_tokenizer.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/renderer/gpu_channel_host.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

WebGraphicsContext3DCommandBufferImpl::WebGraphicsContext3DCommandBufferImpl()
    : context_(NULL),
      web_view_(NULL),
      cached_width_(0),
      cached_height_(0),
      bound_fbo_(0) {
}

WebGraphicsContext3DCommandBufferImpl::
    ~WebGraphicsContext3DCommandBufferImpl() {
  if (context_) {
    ggl::DestroyContext(context_);
  }
}

static const char* kWebGraphicsContext3DPerferredGLExtensions =
    "GL_OES_packed_depth_stencil "
    "GL_OES_depth24 "
    "GL_CHROMIUM_webglsl";

bool WebGraphicsContext3DCommandBufferImpl::initialize(
    WebGraphicsContext3D::Attributes attributes,
    WebKit::WebView* web_view,
    bool render_directly_to_web_view) {
  RenderThread* render_thread = RenderThread::current();
  if (!render_thread)
    return false;
  GpuChannelHost* host = render_thread->EstablishGpuChannelSync();
  if (!host)
    return false;
  DCHECK(host->state() == GpuChannelHost::kConnected);

  // Convert WebGL context creation attributes into GGL/EGL size requests.
  const int alpha_size = attributes.alpha ? 8 : 0;
  const int depth_size = attributes.depth ? 24 : 0;
  const int stencil_size = attributes.stencil ? 8 : 0;
  const int samples = attributes.antialias ? 4 : 0;
  const int sample_buffers = attributes.antialias ? 1 : 0;
  const int32 attribs[] = {
    ggl::GGL_ALPHA_SIZE, alpha_size,
    ggl::GGL_DEPTH_SIZE, depth_size,
    ggl::GGL_STENCIL_SIZE, stencil_size,
    ggl::GGL_SAMPLES, samples,
    ggl::GGL_SAMPLE_BUFFERS, sample_buffers,
    ggl::GGL_NONE,
  };

  GPUInfo gpu_info = host->gpu_info();
  UMA_HISTOGRAM_ENUMERATION(
      "GPU.WebGraphicsContext3D_Init_CanLoseContext",
      attributes.canRecoverFromContextLoss * 2 + gpu_info.can_lose_context(),
      4);
  if (attributes.canRecoverFromContextLoss == false) {
    if (gpu_info.can_lose_context())
      return false;
  }

  if (render_directly_to_web_view) {
    RenderView* renderview = RenderView::FromWebView(web_view);
    if (!renderview)
      return false;

    web_view_ = web_view;
    context_ = ggl::CreateViewContext(
        host,
        renderview->routing_id(),
        kWebGraphicsContext3DPerferredGLExtensions,
        attribs);
    if (context_) {
      ggl::SetSwapBuffersCallback(
          context_,
          NewCallback(this,
                      &WebGraphicsContext3DCommandBufferImpl::OnSwapBuffers));
    }
  } else {
    bool compositing_enabled = !CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kDisableAcceleratedCompositing);
    ggl::Context* parent_context = NULL;
    // If GPU compositing is enabled we need to create a GL context that shares
    // resources with the compositor's context.
    if (compositing_enabled) {
      // Asking for the WebGraphicsContext3D on the WebView will force one to
      // be created if it doesn't already exist. When the compositor is created
      // for the view it will use the same context.
      WebKit::WebGraphicsContext3D* view_context =
          web_view->graphicsContext3D();
      if (view_context) {
        WebGraphicsContext3DCommandBufferImpl* context_impl =
            static_cast<WebGraphicsContext3DCommandBufferImpl*>(view_context);
        parent_context = context_impl->context_;
      }
    }
    context_ = ggl::CreateOffscreenContext(
        host,
        parent_context,
        gfx::Size(1, 1),
        kWebGraphicsContext3DPerferredGLExtensions,
        attribs);
    web_view_ = NULL;
  }
  if (!context_)
    return false;
  // TODO(gman): Remove this.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDisableGLSLTranslator)) {
    DisableShaderTranslation(context_);
  }

  // Set attributes_ from created offscreen context.
  {
    attributes_ = attributes;
    GLint alpha_bits = 0;
    getIntegerv(GL_ALPHA_BITS, &alpha_bits);
    attributes_.alpha = alpha_bits > 0;
    GLint depth_bits = 0;
    getIntegerv(GL_DEPTH_BITS, &depth_bits);
    attributes_.depth = depth_bits > 0;
    GLint stencil_bits = 0;
    getIntegerv(GL_STENCIL_BITS, &stencil_bits);
    attributes_.stencil = stencil_bits > 0;
    GLint samples = 0;
    getIntegerv(GL_SAMPLES, &samples);
    attributes_.antialias = samples > 0;
  }

  return true;
}

bool WebGraphicsContext3DCommandBufferImpl::makeContextCurrent() {
  return ggl::MakeCurrent(context_);
}

int WebGraphicsContext3DCommandBufferImpl::width() {
  return cached_width_;
}

int WebGraphicsContext3DCommandBufferImpl::height() {
  return cached_height_;
}

bool WebGraphicsContext3DCommandBufferImpl::isGLES2Compliant() {
  return true;
}

WebGLId WebGraphicsContext3DCommandBufferImpl::getPlatformTextureId() {
  DCHECK(context_);
  return ggl::GetParentTextureId(context_);
}

void WebGraphicsContext3DCommandBufferImpl::prepareTexture() {
  // Copies the contents of the off-screen render target into the texture
  // used by the compositor.
  ggl::SwapBuffers(context_);
}

void WebGraphicsContext3DCommandBufferImpl::reshape(int width, int height) {
  cached_width_ = width;
  cached_height_ = height;
  makeContextCurrent();

  if (web_view_) {
#if defined(OS_MACOSX)
    ggl::ResizeOnscreenContext(context_, gfx::Size(width, height));
#else
    glResizeCHROMIUM(width, height);
#endif
  } else {
    ggl::ResizeOffscreenContext(context_, gfx::Size(width, height));
    // Force a SwapBuffers to get the framebuffer to resize.
    ggl::SwapBuffers(context_);
  }

#ifdef FLIP_FRAMEBUFFER_VERTICALLY
  scanline_.reset(new uint8[width * 4]);
#endif  // FLIP_FRAMEBUFFER_VERTICALLY
}

WebGLId WebGraphicsContext3DCommandBufferImpl::createCompositorTexture(
    WGC3Dsizei width, WGC3Dsizei height) {
  makeContextCurrent();
  return ggl::CreateParentTexture(context_, gfx::Size(width, height));
}

void WebGraphicsContext3DCommandBufferImpl::deleteCompositorTexture(
    WebGLId parent_texture) {
  makeContextCurrent();
  ggl::DeleteParentTexture(context_, parent_texture);
}

#ifdef FLIP_FRAMEBUFFER_VERTICALLY
void WebGraphicsContext3DCommandBufferImpl::FlipVertically(
    uint8* framebuffer,
    unsigned int width,
    unsigned int height) {
  uint8* scanline = scanline_.get();
  if (!scanline)
    return;
  unsigned int row_bytes = width * 4;
  unsigned int count = height / 2;
  for (unsigned int i = 0; i < count; i++) {
    uint8* row_a = framebuffer + i * row_bytes;
    uint8* row_b = framebuffer + (height - i - 1) * row_bytes;
    // TODO(kbr): this is where the multiplication of the alpha
    // channel into the color buffer will need to occur if the
    // user specifies the "premultiplyAlpha" flag in the context
    // creation attributes.
    memcpy(scanline, row_b, row_bytes);
    memcpy(row_b, row_a, row_bytes);
    memcpy(row_a, scanline, row_bytes);
  }
}
#endif

bool WebGraphicsContext3DCommandBufferImpl::readBackFramebuffer(
    unsigned char* pixels,
    size_t buffer_size) {
  if (buffer_size != static_cast<size_t>(4 * width() * height())) {
    return false;
  }

  makeContextCurrent();

  // Earlier versions of this code used the GPU to flip the
  // framebuffer vertically before reading it back for compositing
  // via software. This code was quite complicated, used a lot of
  // GPU memory, and didn't provide an obvious speedup. Since this
  // vertical flip is only a temporary solution anyway until Chrome
  // is fully GPU composited, it wasn't worth the complexity.

  bool mustRestoreFBO = (bound_fbo_ != 0);
  if (mustRestoreFBO) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }
  glReadPixels(0, 0, cached_width_, cached_height_,
               GL_RGBA, GL_UNSIGNED_BYTE, pixels);

  // Swizzle red and blue channels
  // TODO(kbr): expose GL_BGRA as extension
  for (size_t i = 0; i < buffer_size; i += 4) {
    std::swap(pixels[i], pixels[i + 2]);
  }

  if (mustRestoreFBO) {
    glBindFramebuffer(GL_FRAMEBUFFER, bound_fbo_);
  }

#ifdef FLIP_FRAMEBUFFER_VERTICALLY
  if (pixels) {
    FlipVertically(pixels, cached_width_, cached_height_);
  }
#endif

  return true;
}

void WebGraphicsContext3DCommandBufferImpl::synthesizeGLError(
    WGC3Denum error) {
  if (find(synthetic_errors_.begin(), synthetic_errors_.end(), error) ==
      synthetic_errors_.end()) {
    synthetic_errors_.push_back(error);
  }
}

void* WebGraphicsContext3DCommandBufferImpl::mapBufferSubDataCHROMIUM(
    WGC3Denum target,
    WGC3Dintptr offset,
    WGC3Dsizeiptr size,
    WGC3Denum access) {
  return glMapBufferSubDataCHROMIUM(target, offset, size, access);
}

void WebGraphicsContext3DCommandBufferImpl::unmapBufferSubDataCHROMIUM(
    const void* mem) {
  return glUnmapBufferSubDataCHROMIUM(mem);
}

void* WebGraphicsContext3DCommandBufferImpl::mapTexSubImage2DCHROMIUM(
    WGC3Denum target,
    WGC3Dint level,
    WGC3Dint xoffset,
    WGC3Dint yoffset,
    WGC3Dsizei width,
    WGC3Dsizei height,
    WGC3Denum format,
    WGC3Denum type,
    WGC3Denum access) {
  return glMapTexSubImage2DCHROMIUM(
      target, level, xoffset, yoffset, width, height, format, type, access);
}

void WebGraphicsContext3DCommandBufferImpl::unmapTexSubImage2DCHROMIUM(
    const void* mem) {
  glUnmapTexSubImage2DCHROMIUM(mem);
}

void WebGraphicsContext3DCommandBufferImpl::copyTextureToParentTextureCHROMIUM(
    WebGLId texture, WebGLId parentTexture) {
  copyTextureToCompositor(texture, parentTexture);
}

WebKit::WebString WebGraphicsContext3DCommandBufferImpl::
    getRequestableExtensionsCHROMIUM() {
  return WebKit::WebString::fromUTF8(glGetRequestableExtensionsCHROMIUM());
}

void WebGraphicsContext3DCommandBufferImpl::requestExtensionCHROMIUM(
    const char* extension) {
  glRequestExtensionCHROMIUM(extension);
}

void WebGraphicsContext3DCommandBufferImpl::blitFramebufferCHROMIUM(
    WGC3Dint srcX0, WGC3Dint srcY0, WGC3Dint srcX1, WGC3Dint srcY1,
    WGC3Dint dstX0, WGC3Dint dstY0, WGC3Dint dstX1, WGC3Dint dstY1,
    WGC3Dbitfield mask, WGC3Denum filter) {
  glBlitFramebufferEXT(srcX0, srcY0, srcX1, srcY1,
                       dstX0, dstY0, dstX1, dstY1,
                       mask, filter);
}

void WebGraphicsContext3DCommandBufferImpl::
    renderbufferStorageMultisampleCHROMIUM(
        WGC3Denum target, WGC3Dsizei samples, WGC3Denum internalformat,
        WGC3Dsizei width, WGC3Dsizei height) {
  glRenderbufferStorageMultisampleEXT(target, samples, internalformat,
                                      width, height);
}

// Helper macros to reduce the amount of code.

#define DELEGATE_TO_GL(name, glname)                                    \
void WebGraphicsContext3DCommandBufferImpl::name() {                    \
  makeContextCurrent();                                                 \
  gl##glname();                                                         \
}

#define DELEGATE_TO_GL_1(name, glname, t1)                              \
void WebGraphicsContext3DCommandBufferImpl::name(t1 a1) {               \
  makeContextCurrent();                                                 \
  gl##glname(a1);                                                       \
}

#define DELEGATE_TO_GL_1R(name, glname, t1, rt)                         \
rt WebGraphicsContext3DCommandBufferImpl::name(t1 a1) {                 \
  makeContextCurrent();                                                 \
  return gl##glname(a1);                                                \
}

#define DELEGATE_TO_GL_1RB(name, glname, t1, rt)                        \
rt WebGraphicsContext3DCommandBufferImpl::name(t1 a1) {                 \
  makeContextCurrent();                                                 \
  return gl##glname(a1) ? true : false;                                 \
}

#define DELEGATE_TO_GL_2(name, glname, t1, t2)                          \
void WebGraphicsContext3DCommandBufferImpl::name(t1 a1, t2 a2) {        \
  makeContextCurrent();                                                 \
  gl##glname(a1, a2);                                                   \
}

#define DELEGATE_TO_GL_2R(name, glname, t1, t2, rt)                     \
rt WebGraphicsContext3DCommandBufferImpl::name(t1 a1, t2 a2) {          \
  makeContextCurrent();                                                 \
  return gl##glname(a1, a2);                                            \
}

#define DELEGATE_TO_GL_3(name, glname, t1, t2, t3)                      \
void WebGraphicsContext3DCommandBufferImpl::name(t1 a1, t2 a2, t3 a3) { \
  makeContextCurrent();                                                 \
  gl##glname(a1, a2, a3);                                               \
}

#define DELEGATE_TO_GL_4(name, glname, t1, t2, t3, t4)                  \
void WebGraphicsContext3DCommandBufferImpl::name(t1 a1, t2 a2, t3 a3, t4 a4) { \
  makeContextCurrent();                                                 \
  gl##glname(a1, a2, a3, a4);                                           \
}

#define DELEGATE_TO_GL_5(name, glname, t1, t2, t3, t4, t5)              \
void WebGraphicsContext3DCommandBufferImpl::name(t1 a1, t2 a2, t3 a3,   \
                                                 t4 a4, t5 a5) {        \
  makeContextCurrent();                                                 \
  gl##glname(a1, a2, a3, a4, a5);                                       \
}

#define DELEGATE_TO_GL_6(name, glname, t1, t2, t3, t4, t5, t6)          \
void WebGraphicsContext3DCommandBufferImpl::name(t1 a1, t2 a2, t3 a3,   \
                                                 t4 a4, t5 a5, t6 a6) { \
  makeContextCurrent();                                                 \
  gl##glname(a1, a2, a3, a4, a5, a6);                                   \
}

#define DELEGATE_TO_GL_7(name, glname, t1, t2, t3, t4, t5, t6, t7)      \
void WebGraphicsContext3DCommandBufferImpl::name(t1 a1, t2 a2, t3 a3,   \
                                                 t4 a4, t5 a5, t6 a6, t7 a7) { \
  makeContextCurrent();                                                 \
  gl##glname(a1, a2, a3, a4, a5, a6, a7);                               \
}

#define DELEGATE_TO_GL_8(name, glname, t1, t2, t3, t4, t5, t6, t7, t8)  \
void WebGraphicsContext3DCommandBufferImpl::name(t1 a1, t2 a2, t3 a3,   \
                                                 t4 a4, t5 a5, t6 a6,   \
                                                 t7 a7, t8 a8) {        \
  makeContextCurrent();                                                 \
  gl##glname(a1, a2, a3, a4, a5, a6, a7, a8);                           \
}

#define DELEGATE_TO_GL_9(name, glname, t1, t2, t3, t4, t5, t6, t7, t8, t9) \
void WebGraphicsContext3DCommandBufferImpl::name(t1 a1, t2 a2, t3 a3,   \
                                                 t4 a4, t5 a5, t6 a6,   \
                                                 t7 a7, t8 a8, t9 a9) { \
  makeContextCurrent();                                                 \
  gl##glname(a1, a2, a3, a4, a5, a6, a7, a8, a9);                       \
}

DELEGATE_TO_GL_1(activeTexture, ActiveTexture, WGC3Denum)

DELEGATE_TO_GL_2(attachShader, AttachShader, WebGLId, WebGLId)

DELEGATE_TO_GL_3(bindAttribLocation, BindAttribLocation, WebGLId,
                 WGC3Duint, const WGC3Dchar*)

DELEGATE_TO_GL_2(bindBuffer, BindBuffer, WGC3Denum, WebGLId)

void WebGraphicsContext3DCommandBufferImpl::bindFramebuffer(
    WGC3Denum target,
    WebGLId framebuffer) {
  makeContextCurrent();
  glBindFramebuffer(target, framebuffer);
  bound_fbo_ = framebuffer;
}

DELEGATE_TO_GL_2(bindRenderbuffer, BindRenderbuffer, WGC3Denum, WebGLId)

DELEGATE_TO_GL_2(bindTexture, BindTexture, WGC3Denum, WebGLId)

DELEGATE_TO_GL_4(blendColor, BlendColor,
                 WGC3Dclampf, WGC3Dclampf, WGC3Dclampf, WGC3Dclampf)

DELEGATE_TO_GL_1(blendEquation, BlendEquation, WGC3Denum)

DELEGATE_TO_GL_2(blendEquationSeparate, BlendEquationSeparate,
                 WGC3Denum, WGC3Denum)

DELEGATE_TO_GL_2(blendFunc, BlendFunc, WGC3Denum, WGC3Denum)

DELEGATE_TO_GL_4(blendFuncSeparate, BlendFuncSeparate,
                 WGC3Denum, WGC3Denum, WGC3Denum, WGC3Denum)

DELEGATE_TO_GL_4(bufferData, BufferData,
                 WGC3Denum, WGC3Dsizeiptr, const void*, WGC3Denum)

DELEGATE_TO_GL_4(bufferSubData, BufferSubData,
                 WGC3Denum, WGC3Dintptr, WGC3Dsizeiptr, const void*)

DELEGATE_TO_GL_1R(checkFramebufferStatus, CheckFramebufferStatus,
                  WGC3Denum, WGC3Denum)

DELEGATE_TO_GL_1(clear, Clear, WGC3Dbitfield)

DELEGATE_TO_GL_4(clearColor, ClearColor,
                 WGC3Dclampf, WGC3Dclampf, WGC3Dclampf, WGC3Dclampf)

DELEGATE_TO_GL_1(clearDepth, ClearDepthf, WGC3Dclampf)

DELEGATE_TO_GL_1(clearStencil, ClearStencil, WGC3Dint)

DELEGATE_TO_GL_4(colorMask, ColorMask,
                 WGC3Dboolean, WGC3Dboolean, WGC3Dboolean, WGC3Dboolean)

DELEGATE_TO_GL_1(compileShader, CompileShader, WebGLId)

DELEGATE_TO_GL_8(copyTexImage2D, CopyTexImage2D,
                 WGC3Denum, WGC3Dint, WGC3Denum, WGC3Dint, WGC3Dint,
                 WGC3Dsizei, WGC3Dsizei, WGC3Dint)

DELEGATE_TO_GL_8(copyTexSubImage2D, CopyTexSubImage2D,
                 WGC3Denum, WGC3Dint, WGC3Dint, WGC3Dint, WGC3Dint, WGC3Dint,
                 WGC3Dsizei, WGC3Dsizei)

DELEGATE_TO_GL_1(cullFace, CullFace, WGC3Denum)

DELEGATE_TO_GL_1(depthFunc, DepthFunc, WGC3Denum)

DELEGATE_TO_GL_1(depthMask, DepthMask, WGC3Dboolean)

DELEGATE_TO_GL_2(depthRange, DepthRangef, WGC3Dclampf, WGC3Dclampf)

DELEGATE_TO_GL_2(detachShader, DetachShader, WebGLId, WebGLId)

DELEGATE_TO_GL_1(disable, Disable, WGC3Denum)

DELEGATE_TO_GL_1(disableVertexAttribArray, DisableVertexAttribArray,
                 WGC3Duint)

DELEGATE_TO_GL_3(drawArrays, DrawArrays, WGC3Denum, WGC3Dint, WGC3Dsizei)

void WebGraphicsContext3DCommandBufferImpl::drawElements(WGC3Denum mode,
                                                         WGC3Dsizei count,
                                                         WGC3Denum type,
                                                         WGC3Dintptr offset) {
  makeContextCurrent();
  glDrawElements(mode, count, type,
                 reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

DELEGATE_TO_GL_1(enable, Enable, WGC3Denum)

DELEGATE_TO_GL_1(enableVertexAttribArray, EnableVertexAttribArray,
                 WGC3Duint)

DELEGATE_TO_GL(finish, Finish)

DELEGATE_TO_GL(flush, Flush)

DELEGATE_TO_GL_4(framebufferRenderbuffer, FramebufferRenderbuffer,
                 WGC3Denum, WGC3Denum, WGC3Denum, WebGLId)

DELEGATE_TO_GL_5(framebufferTexture2D, FramebufferTexture2D,
                 WGC3Denum, WGC3Denum, WGC3Denum, WebGLId, WGC3Dint)

DELEGATE_TO_GL_1(frontFace, FrontFace, WGC3Denum)

DELEGATE_TO_GL_1(generateMipmap, GenerateMipmap, WGC3Denum)

bool WebGraphicsContext3DCommandBufferImpl::getActiveAttrib(
    WebGLId program, WGC3Duint index, ActiveInfo& info) {
  makeContextCurrent();
  if (!program) {
    synthesizeGLError(GL_INVALID_VALUE);
    return false;
  }
  GLint max_name_length = -1;
  glGetProgramiv(program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &max_name_length);
  if (max_name_length < 0)
    return false;
  scoped_array<GLchar> name(new GLchar[max_name_length]);
  if (!name.get()) {
    synthesizeGLError(GL_OUT_OF_MEMORY);
    return false;
  }
  GLsizei length = 0;
  GLint size = -1;
  GLenum type = 0;
  glGetActiveAttrib(program, index, max_name_length,
                    &length, &size, &type, name.get());
  if (size < 0) {
    return false;
  }
  info.name = WebKit::WebString::fromUTF8(name.get(), length);
  info.type = type;
  info.size = size;
  return true;
}

bool WebGraphicsContext3DCommandBufferImpl::getActiveUniform(
    WebGLId program, WGC3Duint index, ActiveInfo& info) {
  makeContextCurrent();
  GLint max_name_length = -1;
  glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_name_length);
  if (max_name_length < 0)
    return false;
  scoped_array<GLchar> name(new GLchar[max_name_length]);
  if (!name.get()) {
    synthesizeGLError(GL_OUT_OF_MEMORY);
    return false;
  }
  GLsizei length = 0;
  GLint size = -1;
  GLenum type = 0;
  glGetActiveUniform(program, index, max_name_length,
                     &length, &size, &type, name.get());
  if (size < 0) {
    return false;
  }
  info.name = WebKit::WebString::fromUTF8(name.get(), length);
  info.type = type;
  info.size = size;
  return true;
}

DELEGATE_TO_GL_4(getAttachedShaders, GetAttachedShaders,
                 WebGLId, WGC3Dsizei, WGC3Dsizei*, WebGLId*)

DELEGATE_TO_GL_2R(getAttribLocation, GetAttribLocation,
                  WebGLId, const WGC3Dchar*, WGC3Dint)

DELEGATE_TO_GL_2(getBooleanv, GetBooleanv, WGC3Denum, WGC3Dboolean*)

DELEGATE_TO_GL_3(getBufferParameteriv, GetBufferParameteriv,
                 WGC3Denum, WGC3Denum, WGC3Dint*)

WebKit::WebGraphicsContext3D::Attributes
WebGraphicsContext3DCommandBufferImpl::getContextAttributes() {
  return attributes_;
}

WGC3Denum WebGraphicsContext3DCommandBufferImpl::getError() {
  if (synthetic_errors_.size() > 0) {
    std::vector<WGC3Denum>::iterator iter = synthetic_errors_.begin();
    WGC3Denum err = *iter;
    synthetic_errors_.erase(iter);
    return err;
  }

  makeContextCurrent();
  return glGetError();
}

bool WebGraphicsContext3DCommandBufferImpl::isContextLost() {
  return ggl::IsCommandBufferContextLost(context_);
}

DELEGATE_TO_GL_2(getFloatv, GetFloatv, WGC3Denum, WGC3Dfloat*)

DELEGATE_TO_GL_4(getFramebufferAttachmentParameteriv,
                 GetFramebufferAttachmentParameteriv,
                 WGC3Denum, WGC3Denum, WGC3Denum, WGC3Dint*)

DELEGATE_TO_GL_2(getIntegerv, GetIntegerv, WGC3Denum, WGC3Dint*)

DELEGATE_TO_GL_3(getProgramiv, GetProgramiv, WebGLId, WGC3Denum, WGC3Dint*)

WebKit::WebString WebGraphicsContext3DCommandBufferImpl::getProgramInfoLog(
    WebGLId program) {
  makeContextCurrent();
  GLint logLength = 0;
  glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
  if (!logLength)
    return WebKit::WebString();
  scoped_array<GLchar> log(new GLchar[logLength]);
  if (!log.get())
    return WebKit::WebString();
  GLsizei returnedLogLength = 0;
  glGetProgramInfoLog(program, logLength, &returnedLogLength, log.get());
  DCHECK_EQ(logLength, returnedLogLength + 1);
  WebKit::WebString res =
      WebKit::WebString::fromUTF8(log.get(), returnedLogLength);
  return res;
}

DELEGATE_TO_GL_3(getRenderbufferParameteriv, GetRenderbufferParameteriv,
                 WGC3Denum, WGC3Denum, WGC3Dint*)

DELEGATE_TO_GL_3(getShaderiv, GetShaderiv, WebGLId, WGC3Denum, WGC3Dint*)

WebKit::WebString WebGraphicsContext3DCommandBufferImpl::getShaderInfoLog(
    WebGLId shader) {
  makeContextCurrent();
  GLint logLength = 0;
  glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
  if (!logLength)
    return WebKit::WebString();
  scoped_array<GLchar> log(new GLchar[logLength]);
  if (!log.get())
    return WebKit::WebString();
  GLsizei returnedLogLength = 0;
  glGetShaderInfoLog(shader, logLength, &returnedLogLength, log.get());
  DCHECK_EQ(logLength, returnedLogLength + 1);
  WebKit::WebString res =
      WebKit::WebString::fromUTF8(log.get(), returnedLogLength);
  return res;
}

WebKit::WebString WebGraphicsContext3DCommandBufferImpl::getShaderSource(
    WebGLId shader) {
  makeContextCurrent();
  GLint logLength = 0;
  glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &logLength);
  if (!logLength)
    return WebKit::WebString();
  scoped_array<GLchar> log(new GLchar[logLength]);
  if (!log.get())
    return WebKit::WebString();
  GLsizei returnedLogLength = 0;
  glGetShaderSource(shader, logLength, &returnedLogLength, log.get());
  DCHECK_EQ(logLength, returnedLogLength + 1);
  WebKit::WebString res =
      WebKit::WebString::fromUTF8(log.get(), returnedLogLength);
  return res;
}

WebKit::WebString WebGraphicsContext3DCommandBufferImpl::getString(
    WGC3Denum name) {
  makeContextCurrent();
  return WebKit::WebString::fromUTF8(
      reinterpret_cast<const char*>(glGetString(name)));
}

DELEGATE_TO_GL_3(getTexParameterfv, GetTexParameterfv,
                 WGC3Denum, WGC3Denum, WGC3Dfloat*)

DELEGATE_TO_GL_3(getTexParameteriv, GetTexParameteriv,
                 WGC3Denum, WGC3Denum, WGC3Dint*)

DELEGATE_TO_GL_3(getUniformfv, GetUniformfv, WebGLId, WGC3Dint, WGC3Dfloat*)

DELEGATE_TO_GL_3(getUniformiv, GetUniformiv, WebGLId, WGC3Dint, WGC3Dint*)

DELEGATE_TO_GL_2R(getUniformLocation, GetUniformLocation,
                  WebGLId, const WGC3Dchar*, WGC3Dint)

DELEGATE_TO_GL_3(getVertexAttribfv, GetVertexAttribfv,
                 WGC3Duint, WGC3Denum, WGC3Dfloat*)

DELEGATE_TO_GL_3(getVertexAttribiv, GetVertexAttribiv,
                 WGC3Duint, WGC3Denum, WGC3Dint*)

WGC3Dsizeiptr WebGraphicsContext3DCommandBufferImpl::getVertexAttribOffset(
    WGC3Duint index, WGC3Denum pname) {
  makeContextCurrent();
  GLvoid* value = NULL;
  // NOTE: If pname is ever a value that returns more then 1 element
  // this will corrupt memory.
  glGetVertexAttribPointerv(index, pname, &value);
  return static_cast<WGC3Dsizeiptr>(reinterpret_cast<intptr_t>(value));
}

DELEGATE_TO_GL_2(hint, Hint, WGC3Denum, WGC3Denum)

DELEGATE_TO_GL_1RB(isBuffer, IsBuffer, WebGLId, WGC3Dboolean)

DELEGATE_TO_GL_1RB(isEnabled, IsEnabled, WGC3Denum, WGC3Dboolean)

DELEGATE_TO_GL_1RB(isFramebuffer, IsFramebuffer, WebGLId, WGC3Dboolean)

DELEGATE_TO_GL_1RB(isProgram, IsProgram, WebGLId, WGC3Dboolean)

DELEGATE_TO_GL_1RB(isRenderbuffer, IsRenderbuffer, WebGLId, WGC3Dboolean)

DELEGATE_TO_GL_1RB(isShader, IsShader, WebGLId, WGC3Dboolean)

DELEGATE_TO_GL_1RB(isTexture, IsTexture, WebGLId, WGC3Dboolean)

DELEGATE_TO_GL_1(lineWidth, LineWidth, WGC3Dfloat)

DELEGATE_TO_GL_1(linkProgram, LinkProgram, WebGLId)

DELEGATE_TO_GL_2(pixelStorei, PixelStorei, WGC3Denum, WGC3Dint)

DELEGATE_TO_GL_2(polygonOffset, PolygonOffset, WGC3Dfloat, WGC3Dfloat)

DELEGATE_TO_GL_7(readPixels, ReadPixels,
                 WGC3Dint, WGC3Dint, WGC3Dsizei, WGC3Dsizei, WGC3Denum,
                 WGC3Denum, void*)

void WebGraphicsContext3DCommandBufferImpl::releaseShaderCompiler() {
}

DELEGATE_TO_GL_4(renderbufferStorage, RenderbufferStorage,
                 WGC3Denum, WGC3Denum, WGC3Dsizei, WGC3Dsizei)

DELEGATE_TO_GL_2(sampleCoverage, SampleCoverage, WGC3Dfloat, WGC3Dboolean)

DELEGATE_TO_GL_4(scissor, Scissor, WGC3Dint, WGC3Dint, WGC3Dsizei, WGC3Dsizei)

void WebGraphicsContext3DCommandBufferImpl::shaderSource(
    WebGLId shader, const WGC3Dchar* string) {
  makeContextCurrent();
  GLint length = strlen(string);
  glShaderSource(shader, 1, &string, &length);
}

DELEGATE_TO_GL_3(stencilFunc, StencilFunc, WGC3Denum, WGC3Dint, WGC3Duint)

DELEGATE_TO_GL_4(stencilFuncSeparate, StencilFuncSeparate,
                 WGC3Denum, WGC3Denum, WGC3Dint, WGC3Duint)

DELEGATE_TO_GL_1(stencilMask, StencilMask, WGC3Duint)

DELEGATE_TO_GL_2(stencilMaskSeparate, StencilMaskSeparate,
                 WGC3Denum, WGC3Duint)

DELEGATE_TO_GL_3(stencilOp, StencilOp,
                 WGC3Denum, WGC3Denum, WGC3Denum)

DELEGATE_TO_GL_4(stencilOpSeparate, StencilOpSeparate,
                 WGC3Denum, WGC3Denum, WGC3Denum, WGC3Denum)

DELEGATE_TO_GL_9(texImage2D, TexImage2D,
                 WGC3Denum, WGC3Dint, WGC3Denum, WGC3Dsizei, WGC3Dsizei,
                 WGC3Dint, WGC3Denum, WGC3Denum, const void*)

DELEGATE_TO_GL_3(texParameterf, TexParameterf,
                 WGC3Denum, WGC3Denum, WGC3Dfloat);

static const unsigned int kTextureWrapR = 0x8072;

void WebGraphicsContext3DCommandBufferImpl::texParameteri(
    WGC3Denum target, WGC3Denum pname, WGC3Dint param) {
  // TODO(kbr): figure out whether the setting of TEXTURE_WRAP_R in
  // GraphicsContext3D.cpp is strictly necessary to avoid seams at the
  // edge of cube maps, and, if it is, push it into the GLES2 service
  // side code.
  if (pname == kTextureWrapR) {
    return;
  }
  makeContextCurrent();
  glTexParameteri(target, pname, param);
}

DELEGATE_TO_GL_9(texSubImage2D, TexSubImage2D,
                 WGC3Denum, WGC3Dint, WGC3Dint, WGC3Dint, WGC3Dsizei,
                 WGC3Dsizei, WGC3Denum, WGC3Denum, const void*)

DELEGATE_TO_GL_2(uniform1f, Uniform1f, WGC3Dint, WGC3Dfloat)

DELEGATE_TO_GL_3(uniform1fv, Uniform1fv, WGC3Dint, WGC3Dsizei,
                 const WGC3Dfloat*)

DELEGATE_TO_GL_2(uniform1i, Uniform1i, WGC3Dint, WGC3Dint)

DELEGATE_TO_GL_3(uniform1iv, Uniform1iv, WGC3Dint, WGC3Dsizei, const WGC3Dint*)

DELEGATE_TO_GL_3(uniform2f, Uniform2f, WGC3Dint, WGC3Dfloat, WGC3Dfloat)

DELEGATE_TO_GL_3(uniform2fv, Uniform2fv, WGC3Dint, WGC3Dsizei,
                 const WGC3Dfloat*)

DELEGATE_TO_GL_3(uniform2i, Uniform2i, WGC3Dint, WGC3Dint, WGC3Dint)

DELEGATE_TO_GL_3(uniform2iv, Uniform2iv, WGC3Dint, WGC3Dsizei, const WGC3Dint*)

DELEGATE_TO_GL_4(uniform3f, Uniform3f, WGC3Dint,
                 WGC3Dfloat, WGC3Dfloat, WGC3Dfloat)

DELEGATE_TO_GL_3(uniform3fv, Uniform3fv, WGC3Dint, WGC3Dsizei,
                 const WGC3Dfloat*)

DELEGATE_TO_GL_4(uniform3i, Uniform3i, WGC3Dint, WGC3Dint, WGC3Dint, WGC3Dint)

DELEGATE_TO_GL_3(uniform3iv, Uniform3iv, WGC3Dint, WGC3Dsizei, const WGC3Dint*)

DELEGATE_TO_GL_5(uniform4f, Uniform4f, WGC3Dint,
                 WGC3Dfloat, WGC3Dfloat, WGC3Dfloat, WGC3Dfloat)

DELEGATE_TO_GL_3(uniform4fv, Uniform4fv, WGC3Dint, WGC3Dsizei,
                 const WGC3Dfloat*)

DELEGATE_TO_GL_5(uniform4i, Uniform4i, WGC3Dint,
                 WGC3Dint, WGC3Dint, WGC3Dint, WGC3Dint)

DELEGATE_TO_GL_3(uniform4iv, Uniform4iv, WGC3Dint, WGC3Dsizei, const WGC3Dint*)

DELEGATE_TO_GL_4(uniformMatrix2fv, UniformMatrix2fv,
                 WGC3Dint, WGC3Dsizei, WGC3Dboolean, const WGC3Dfloat*)

DELEGATE_TO_GL_4(uniformMatrix3fv, UniformMatrix3fv,
                 WGC3Dint, WGC3Dsizei, WGC3Dboolean, const WGC3Dfloat*)

DELEGATE_TO_GL_4(uniformMatrix4fv, UniformMatrix4fv,
                 WGC3Dint, WGC3Dsizei, WGC3Dboolean, const WGC3Dfloat*)

DELEGATE_TO_GL_1(useProgram, UseProgram, WebGLId)

DELEGATE_TO_GL_1(validateProgram, ValidateProgram, WebGLId)

DELEGATE_TO_GL_2(vertexAttrib1f, VertexAttrib1f, WGC3Duint, WGC3Dfloat)

DELEGATE_TO_GL_2(vertexAttrib1fv, VertexAttrib1fv, WGC3Duint,
                 const WGC3Dfloat*)

DELEGATE_TO_GL_3(vertexAttrib2f, VertexAttrib2f, WGC3Duint,
                 WGC3Dfloat, WGC3Dfloat)

DELEGATE_TO_GL_2(vertexAttrib2fv, VertexAttrib2fv, WGC3Duint,
                 const WGC3Dfloat*)

DELEGATE_TO_GL_4(vertexAttrib3f, VertexAttrib3f, WGC3Duint,
                 WGC3Dfloat, WGC3Dfloat, WGC3Dfloat)

DELEGATE_TO_GL_2(vertexAttrib3fv, VertexAttrib3fv, WGC3Duint,
                 const WGC3Dfloat*)

DELEGATE_TO_GL_5(vertexAttrib4f, VertexAttrib4f, WGC3Duint,
                 WGC3Dfloat, WGC3Dfloat, WGC3Dfloat, WGC3Dfloat)

DELEGATE_TO_GL_2(vertexAttrib4fv, VertexAttrib4fv, WGC3Duint,
                 const WGC3Dfloat*)

void WebGraphicsContext3DCommandBufferImpl::vertexAttribPointer(
    WGC3Duint index, WGC3Dint size, WGC3Denum type, WGC3Dboolean normalized,
    WGC3Dsizei stride, WGC3Dintptr offset) {
  makeContextCurrent();

  glVertexAttribPointer(index, size, type, normalized, stride,
                        reinterpret_cast<void*>(
                            static_cast<intptr_t>(offset)));
}

DELEGATE_TO_GL_4(viewport, Viewport,
                 WGC3Dint, WGC3Dint, WGC3Dsizei, WGC3Dsizei)

WebGLId WebGraphicsContext3DCommandBufferImpl::createBuffer() {
  makeContextCurrent();
  GLuint o;
  glGenBuffers(1, &o);
  return o;
}

WebGLId WebGraphicsContext3DCommandBufferImpl::createFramebuffer() {
  makeContextCurrent();
  GLuint o = 0;
  glGenFramebuffers(1, &o);
  return o;
}

WebGLId WebGraphicsContext3DCommandBufferImpl::createProgram() {
  makeContextCurrent();
  return glCreateProgram();
}

WebGLId WebGraphicsContext3DCommandBufferImpl::createRenderbuffer() {
  makeContextCurrent();
  GLuint o;
  glGenRenderbuffers(1, &o);
  return o;
}

DELEGATE_TO_GL_1R(createShader, CreateShader, WGC3Denum, WebGLId);

WebGLId WebGraphicsContext3DCommandBufferImpl::createTexture() {
  makeContextCurrent();
  GLuint o;
  glGenTextures(1, &o);
  return o;
}

void WebGraphicsContext3DCommandBufferImpl::deleteBuffer(WebGLId buffer) {
  makeContextCurrent();
  glDeleteBuffers(1, &buffer);
}

void WebGraphicsContext3DCommandBufferImpl::deleteFramebuffer(
    WebGLId framebuffer) {
  makeContextCurrent();
  glDeleteFramebuffers(1, &framebuffer);
}

void WebGraphicsContext3DCommandBufferImpl::deleteProgram(WebGLId program) {
  makeContextCurrent();
  glDeleteProgram(program);
}

void WebGraphicsContext3DCommandBufferImpl::deleteRenderbuffer(
    WebGLId renderbuffer) {
  makeContextCurrent();
  glDeleteRenderbuffers(1, &renderbuffer);
}

void WebGraphicsContext3DCommandBufferImpl::deleteShader(WebGLId shader) {
  makeContextCurrent();
  glDeleteShader(shader);
}

void WebGraphicsContext3DCommandBufferImpl::deleteTexture(WebGLId texture) {
  makeContextCurrent();
  glDeleteTextures(1, &texture);
}

void WebGraphicsContext3DCommandBufferImpl::copyTextureToCompositor(
    WebGLId texture, WebGLId parentTexture) {
  makeContextCurrent();
  glCopyTextureToParentTextureCHROMIUM(texture, parentTexture);
  glFlush();
}

void WebGraphicsContext3DCommandBufferImpl::OnSwapBuffers() {
  // This may be called after tear-down of the RenderView.
  RenderView* renderview =
      web_view_ ? RenderView::FromWebView(web_view_) : NULL;
  if (renderview)
    renderview->DidFlushPaint();
}

#endif  // defined(ENABLE_GPU)
