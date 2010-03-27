// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(ENABLE_GPU)

#include "chrome/renderer/webgraphicscontext3d_command_buffer_impl.h"

#include <GLES2/gl2.h>

#include <algorithm>

#include "base/logging.h"
#include "chrome/renderer/gpu_channel_host.h"
#include "chrome/renderer/render_thread.h"

// TODO(kbr): OpenGL may return multiple errors from sequential calls
// to glGetError.
static void checkGLError() {
  GLenum error = glGetError();
  if (error) {
    DLOG(ERROR) << "GL Error " << error;
  }
}

WebGraphicsContext3DCommandBufferImpl::WebGraphicsContext3DCommandBufferImpl()
    : context_(NULL),
      texture_(0),
      fbo_(0),
      depth_buffer_(0),
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

bool WebGraphicsContext3DCommandBufferImpl::initialize(
    WebGraphicsContext3D::Attributes attributes) {
  RenderThread* render_thread = RenderThread::current();
  if (!render_thread)
    return false;
  GpuChannelHost* host = render_thread->EstablishGpuChannelSync();
  if (!host)
    return false;
  DCHECK(host->ready());
  context_ = ggl::CreateOffscreenContext(host, NULL, gfx::Size(1, 1));
  if (!context_)
    return false;
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

int WebGraphicsContext3DCommandBufferImpl::sizeInBytes(int type) {
  switch (type) {
    case GL_BYTE:
      return sizeof(GLbyte);
    case GL_UNSIGNED_BYTE:
      return sizeof(GLubyte);
    case GL_SHORT:
      return sizeof(GLshort);
    case GL_UNSIGNED_SHORT:
      return sizeof(GLushort);
    case GL_INT:
      return sizeof(GLint);
    case GL_UNSIGNED_INT:
      return sizeof(GLuint);
    case GL_FLOAT:
      return sizeof(GLfloat);
  }
  return 0;
}

static int createTextureObject(GLenum target) {
  GLuint texture = 0;
  glGenTextures(1, &texture);
  checkGLError();
  glBindTexture(target, texture);
  checkGLError();
  glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  checkGLError();
  glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  checkGLError();
  return texture;
}

void WebGraphicsContext3DCommandBufferImpl::reshape(int width, int height) {
  cached_width_ = width;
  cached_height_ = height;
  makeContextCurrent();

  checkGLError();

  GLenum target = GL_TEXTURE_2D;

  // TODO(kbr): switch this code to use the default back buffer of
  // GGL / the GLES2 command buffer code.

  // TODO(kbr): determine whether we need to hack in
  // GL_TEXTURE_RECTANGLE_ARB support for Mac OS X -- or resize the
  // framebuffer objects to the next largest power of two.

  if (!texture_) {
    // Generate the texture object
    texture_ = createTextureObject(target);
    // Generate the framebuffer object
    glGenFramebuffers(1, &fbo_);
    checkGLError();
    // Generate the depth buffer
    glGenRenderbuffers(1, &depth_buffer_);
    checkGLError();
  }

  // Reallocate the color and depth buffers
  glBindTexture(target, texture_);
  checkGLError();
  glTexImage2D(target, 0, GL_RGBA, width, height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, 0);
  checkGLError();
  glBindTexture(target, 0);
  checkGLError();

  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
  checkGLError();
  bound_fbo_ = fbo_;
  glBindRenderbuffer(GL_RENDERBUFFER, depth_buffer_);
  checkGLError();
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16,
                        width, height);
  checkGLError();
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  checkGLError();

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                         target, texture_, 0);
  checkGLError();
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, depth_buffer_);
  checkGLError();
  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  checkGLError();
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    DLOG(ERROR) << "WebGraphicsContext3DCommandBufferImpl: "
                << "framebuffer was incomplete";

    // TODO(kbr): cleanup.
    NOTIMPLEMENTED();
  }

#ifdef FLIP_FRAMEBUFFER_VERTICALLY
  scanline_.reset(new uint8[width * 4]);
#endif  // FLIP_FRAMEBUFFER_VERTICALLY

  glClear(GL_COLOR_BUFFER_BIT);
  checkGLError();
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

  bool mustRestoreFBO = (bound_fbo_ != fbo_);
  if (mustRestoreFBO)
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
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
    unsigned long error) {
  if (find(synthetic_errors_.begin(), synthetic_errors_.end(), error) ==
      synthetic_errors_.end()) {
    synthetic_errors_.push_back(error);
  }
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

#define DELEGATE_TO_GL_1_C(name, glname, t1)                            \
void WebGraphicsContext3DCommandBufferImpl::name(t1 a1) {               \
  makeContextCurrent();                                                 \
  gl##glname(static_cast<GLclampf>(a1));                                \
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

#define DELEGATE_TO_GL_2_C1(name, glname, t1, t2)                       \
void WebGraphicsContext3DCommandBufferImpl::name(t1 a1, t2 a2) {        \
  makeContextCurrent();                                                 \
  gl##glname(static_cast<GLclampf>(a1), a2);                            \
}

#define DELEGATE_TO_GL_2_C12(name, glname, t1, t2)                      \
void WebGraphicsContext3DCommandBufferImpl::name(t1 a1, t2 a2) {        \
  makeContextCurrent();                                                 \
  gl##glname(static_cast<GLclampf>(a1), static_cast<GLclampf>(a2));     \
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

#define DELEGATE_TO_GL_4_C1234(name, glname, t1, t2, t3, t4)            \
void WebGraphicsContext3DCommandBufferImpl::name(t1 a1, t2 a2, t3 a3, t4 a4) { \
  makeContextCurrent();                                                 \
  gl##glname(static_cast<GLclampf>(a1), static_cast<GLclampf>(a2),      \
             static_cast<GLclampf>(a3), static_cast<GLclampf>(a4));     \
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

DELEGATE_TO_GL_1(activeTexture, ActiveTexture, unsigned long)

DELEGATE_TO_GL_2(attachShader, AttachShader, WebGLId, WebGLId)

DELEGATE_TO_GL_3(bindAttribLocation, BindAttribLocation, WebGLId,
                 unsigned long, const char*)

DELEGATE_TO_GL_2(bindBuffer, BindBuffer, unsigned long, WebGLId)

void WebGraphicsContext3DCommandBufferImpl::bindFramebuffer(
    unsigned long target,
    WebGLId framebuffer) {
  makeContextCurrent();
  if (!framebuffer)
    framebuffer = fbo_;
  glBindFramebuffer(target, framebuffer);
  bound_fbo_ = framebuffer;
}

DELEGATE_TO_GL_2(bindRenderbuffer, BindRenderbuffer, unsigned long, WebGLId)

DELEGATE_TO_GL_2(bindTexture, BindTexture, unsigned long, WebGLId)

DELEGATE_TO_GL_4_C1234(blendColor, BlendColor, double, double, double, double)

DELEGATE_TO_GL_1(blendEquation, BlendEquation, unsigned long)

DELEGATE_TO_GL_2(blendEquationSeparate, BlendEquationSeparate,
                 unsigned long, unsigned long)

DELEGATE_TO_GL_2(blendFunc, BlendFunc, unsigned long, unsigned long)

DELEGATE_TO_GL_4(blendFuncSeparate, BlendFuncSeparate,
                 unsigned long, unsigned long, unsigned long, unsigned long)

DELEGATE_TO_GL_4(bufferData, BufferData,
                 unsigned long, int, const void*, unsigned long)

DELEGATE_TO_GL_4(bufferSubData, BufferSubData,
                 unsigned long, long, int, const void*)

DELEGATE_TO_GL_1R(checkFramebufferStatus, CheckFramebufferStatus,
                  unsigned long, unsigned long)

DELEGATE_TO_GL_1(clear, Clear, unsigned long)

DELEGATE_TO_GL_4_C1234(clearColor, ClearColor, double, double, double, double)

DELEGATE_TO_GL_1_C(clearDepth, ClearDepthf, double)

DELEGATE_TO_GL_1(clearStencil, ClearStencil, long)

DELEGATE_TO_GL_4(colorMask, ColorMask, bool, bool, bool, bool)

DELEGATE_TO_GL_1(compileShader, CompileShader, WebGLId)

DELEGATE_TO_GL_8(copyTexImage2D, CopyTexImage2D,
                 unsigned long, long, unsigned long, long, long,
                 unsigned long, unsigned long, long)

DELEGATE_TO_GL_8(copyTexSubImage2D, CopyTexSubImage2D,
                 unsigned long, long, long, long, long, long,
                 unsigned long, unsigned long)

DELEGATE_TO_GL_1(cullFace, CullFace, unsigned long)

DELEGATE_TO_GL_1(depthFunc, DepthFunc, unsigned long)

DELEGATE_TO_GL_1(depthMask, DepthMask, bool)

DELEGATE_TO_GL_2_C12(depthRange, DepthRangef, double, double)

DELEGATE_TO_GL_2(detachShader, DetachShader, WebGLId, WebGLId)

DELEGATE_TO_GL_1(disable, Disable, unsigned long)

DELEGATE_TO_GL_1(disableVertexAttribArray, DisableVertexAttribArray,
                 unsigned long)

DELEGATE_TO_GL_3(drawArrays, DrawArrays, unsigned long, long, long)

void WebGraphicsContext3DCommandBufferImpl::drawElements(unsigned long mode,
                                                         unsigned long count,
                                                         unsigned long type,
                                                         long offset) {
  makeContextCurrent();
  glDrawElements(mode, count, type,
                 reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

DELEGATE_TO_GL_1(enable, Enable, unsigned long)

DELEGATE_TO_GL_1(enableVertexAttribArray, EnableVertexAttribArray,
                 unsigned long)

DELEGATE_TO_GL(finish, Finish)

DELEGATE_TO_GL(flush, Flush)

DELEGATE_TO_GL_4(framebufferRenderbuffer, FramebufferRenderbuffer,
                 unsigned long, unsigned long, unsigned long, WebGLId)

DELEGATE_TO_GL_5(framebufferTexture2D, FramebufferTexture2D,
                 unsigned long, unsigned long, unsigned long, WebGLId, long)

DELEGATE_TO_GL_1(frontFace, FrontFace, unsigned long)

DELEGATE_TO_GL_1(generateMipmap, GenerateMipmap, unsigned long)

bool WebGraphicsContext3DCommandBufferImpl::getActiveAttrib(
    WebGLId program, unsigned long index, ActiveInfo& info) {
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
    WebGLId program, unsigned long index, ActiveInfo& info) {
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

DELEGATE_TO_GL_2R(getAttribLocation, GetAttribLocation,
                  WebGLId, const char*, int)

DELEGATE_TO_GL_2(getBooleanv, GetBooleanv, unsigned long, unsigned char*)

DELEGATE_TO_GL_3(getBufferParameteriv, GetBufferParameteriv,
                 unsigned long, unsigned long, int*)

WebKit::WebGraphicsContext3D::Attributes
WebGraphicsContext3DCommandBufferImpl::getContextAttributes() {
  return attributes_;
}

unsigned long WebGraphicsContext3DCommandBufferImpl::getError() {
  if (synthetic_errors_.size() > 0) {
    std::vector<unsigned long>::iterator iter = synthetic_errors_.begin();
    unsigned long err = *iter;
    synthetic_errors_.erase(iter);
    return err;
  }

  makeContextCurrent();
  return glGetError();
}

DELEGATE_TO_GL_2(getFloatv, GetFloatv, unsigned long, float*)

DELEGATE_TO_GL_4(getFramebufferAttachmentParameteriv,
                 GetFramebufferAttachmentParameteriv,
                 unsigned long, unsigned long, unsigned long, int*)

DELEGATE_TO_GL_2(getIntegerv, GetIntegerv, unsigned long, int*)

DELEGATE_TO_GL_3(getProgramiv, GetProgramiv, WebGLId, unsigned long, int*)

WebKit::WebString WebGraphicsContext3DCommandBufferImpl::getProgramInfoLog(
    WebGLId program) {
  makeContextCurrent();
  GLint logLength;
  glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
  if (!logLength)
    return WebKit::WebString();
  scoped_array<GLchar> log(new GLchar[logLength]);
  if (!log.get())
    return WebKit::WebString();
  GLsizei returnedLogLength;
  glGetProgramInfoLog(program, logLength, &returnedLogLength, log.get());
  DCHECK_EQ(logLength, returnedLogLength + 1);
  WebKit::WebString res =
      WebKit::WebString::fromUTF8(log.get(), returnedLogLength);
  return res;
}

DELEGATE_TO_GL_3(getRenderbufferParameteriv, GetRenderbufferParameteriv,
                 unsigned long, unsigned long, int*)

DELEGATE_TO_GL_3(getShaderiv, GetShaderiv, WebGLId, unsigned long, int*)

WebKit::WebString WebGraphicsContext3DCommandBufferImpl::getShaderInfoLog(
    WebGLId shader) {
  makeContextCurrent();
  GLint logLength;
  glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
  if (!logLength)
    return WebKit::WebString();
  scoped_array<GLchar> log(new GLchar[logLength]);
  if (!log.get())
    return WebKit::WebString();
  GLsizei returnedLogLength;
  glGetShaderInfoLog(shader, logLength, &returnedLogLength, log.get());
  DCHECK_EQ(logLength, returnedLogLength + 1);
  WebKit::WebString res =
      WebKit::WebString::fromUTF8(log.get(), returnedLogLength);
  return res;
}

WebKit::WebString WebGraphicsContext3DCommandBufferImpl::getShaderSource(
    WebGLId shader) {
  makeContextCurrent();
  GLint logLength;
  glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &logLength);
  if (!logLength)
    return WebKit::WebString();
  scoped_array<GLchar> log(new GLchar[logLength]);
  if (!log.get())
    return WebKit::WebString();
  GLsizei returnedLogLength;
  glGetShaderSource(shader, logLength, &returnedLogLength, log.get());
  DCHECK_EQ(logLength, returnedLogLength + 1);
  WebKit::WebString res =
      WebKit::WebString::fromUTF8(log.get(), returnedLogLength);
  return res;
}

WebKit::WebString WebGraphicsContext3DCommandBufferImpl::getString(
    unsigned long name) {
  makeContextCurrent();
  return WebKit::WebString::fromUTF8(
      reinterpret_cast<const char*>(glGetString(name)));
}

DELEGATE_TO_GL_3(getTexParameterfv, GetTexParameterfv,
                 unsigned long, unsigned long, float*)

DELEGATE_TO_GL_3(getTexParameteriv, GetTexParameteriv,
                 unsigned long, unsigned long, int*)

DELEGATE_TO_GL_3(getUniformfv, GetUniformfv, WebGLId, long, float*)

DELEGATE_TO_GL_3(getUniformiv, GetUniformiv, WebGLId, long, int*)

DELEGATE_TO_GL_2R(getUniformLocation, GetUniformLocation,
                  WebGLId, const char*, long)

DELEGATE_TO_GL_3(getVertexAttribfv, GetVertexAttribfv,
                 unsigned long, unsigned long, float*)

DELEGATE_TO_GL_3(getVertexAttribiv, GetVertexAttribiv,
                 unsigned long, unsigned long, int*)

long WebGraphicsContext3DCommandBufferImpl::getVertexAttribOffset(
    unsigned long index, unsigned long pname) {
  // TODO(kbr): implement.
  NOTIMPLEMENTED();
  return 0;
}

DELEGATE_TO_GL_2(hint, Hint, unsigned long, unsigned long)

DELEGATE_TO_GL_1RB(isBuffer, IsBuffer, WebGLId, bool)

DELEGATE_TO_GL_1RB(isEnabled, IsEnabled, unsigned long, bool)

DELEGATE_TO_GL_1RB(isFramebuffer, IsFramebuffer, WebGLId, bool)

DELEGATE_TO_GL_1RB(isProgram, IsProgram, WebGLId, bool)

DELEGATE_TO_GL_1RB(isRenderbuffer, IsRenderbuffer, WebGLId, bool)

DELEGATE_TO_GL_1RB(isShader, IsShader, WebGLId, bool)

DELEGATE_TO_GL_1RB(isTexture, IsTexture, WebGLId, bool)

DELEGATE_TO_GL_1_C(lineWidth, LineWidth, double)

DELEGATE_TO_GL_1(linkProgram, LinkProgram, WebGLId)

DELEGATE_TO_GL_2(pixelStorei, PixelStorei, unsigned long, long)

DELEGATE_TO_GL_2_C12(polygonOffset, PolygonOffset, double, double)

DELEGATE_TO_GL_7(readPixels, ReadPixels,
                 long, long, unsigned long, unsigned long, unsigned long,
                 unsigned long, void*)

void WebGraphicsContext3DCommandBufferImpl::releaseShaderCompiler() {
}

DELEGATE_TO_GL_4(renderbufferStorage, RenderbufferStorage,
                 unsigned long, unsigned long, unsigned long, unsigned long)

DELEGATE_TO_GL_2_C1(sampleCoverage, SampleCoverage, double, bool)

DELEGATE_TO_GL_4(scissor, Scissor, long, long, unsigned long, unsigned long)

void WebGraphicsContext3DCommandBufferImpl::shaderSource(
    WebGLId shader, const char* string) {
  makeContextCurrent();
  GLint length = strlen(string);
  glShaderSource(shader, 1, &string, &length);
}

DELEGATE_TO_GL_3(stencilFunc, StencilFunc, unsigned long, long, unsigned long)

DELEGATE_TO_GL_4(stencilFuncSeparate, StencilFuncSeparate,
                 unsigned long, unsigned long, long, unsigned long)

DELEGATE_TO_GL_1(stencilMask, StencilMask, unsigned long)

DELEGATE_TO_GL_2(stencilMaskSeparate, StencilMaskSeparate,
                 unsigned long, unsigned long)

DELEGATE_TO_GL_3(stencilOp, StencilOp,
                 unsigned long, unsigned long, unsigned long)

DELEGATE_TO_GL_4(stencilOpSeparate, StencilOpSeparate,
                 unsigned long, unsigned long, unsigned long, unsigned long)

DELEGATE_TO_GL_9(texImage2D, TexImage2D,
                 unsigned, unsigned, unsigned, unsigned, unsigned, unsigned,
                 unsigned, unsigned, const void*)

DELEGATE_TO_GL_3(texParameterf, TexParameterf, unsigned, unsigned, float);

static const unsigned int kTextureWrapR = 0x8072;

void WebGraphicsContext3DCommandBufferImpl::texParameteri(
    unsigned target, unsigned pname, int param) {
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
                 unsigned, unsigned, unsigned, unsigned, unsigned, unsigned,
                 unsigned, unsigned, const void*)

DELEGATE_TO_GL_2(uniform1f, Uniform1f, long, float)

DELEGATE_TO_GL_3(uniform1fv, Uniform1fv, long, int, float*)

DELEGATE_TO_GL_2(uniform1i, Uniform1i, long, int)

DELEGATE_TO_GL_3(uniform1iv, Uniform1iv, long, int, int*)

DELEGATE_TO_GL_3(uniform2f, Uniform2f, long, float, float)

DELEGATE_TO_GL_3(uniform2fv, Uniform2fv, long, int, float*)

DELEGATE_TO_GL_3(uniform2i, Uniform2i, long, int, int)

DELEGATE_TO_GL_3(uniform2iv, Uniform2iv, long, int, int*)

DELEGATE_TO_GL_4(uniform3f, Uniform3f, long, float, float, float)

DELEGATE_TO_GL_3(uniform3fv, Uniform3fv, long, int, float*)

DELEGATE_TO_GL_4(uniform3i, Uniform3i, long, int, int, int)

DELEGATE_TO_GL_3(uniform3iv, Uniform3iv, long, int, int*)

DELEGATE_TO_GL_5(uniform4f, Uniform4f, long, float, float, float, float)

DELEGATE_TO_GL_3(uniform4fv, Uniform4fv, long, int, float*)

DELEGATE_TO_GL_5(uniform4i, Uniform4i, long, int, int, int, int)

DELEGATE_TO_GL_3(uniform4iv, Uniform4iv, long, int, int*)

DELEGATE_TO_GL_4(uniformMatrix2fv, UniformMatrix2fv,
                 long, int, bool, const float*)

DELEGATE_TO_GL_4(uniformMatrix3fv, UniformMatrix3fv,
                 long, int, bool, const float*)

DELEGATE_TO_GL_4(uniformMatrix4fv, UniformMatrix4fv,
                 long, int, bool, const float*)

DELEGATE_TO_GL_1(useProgram, UseProgram, WebGLId)

DELEGATE_TO_GL_1(validateProgram, ValidateProgram, WebGLId)

DELEGATE_TO_GL_2(vertexAttrib1f, VertexAttrib1f, unsigned long, float)

DELEGATE_TO_GL_2(vertexAttrib1fv, VertexAttrib1fv, unsigned long, const float*)

DELEGATE_TO_GL_3(vertexAttrib2f, VertexAttrib2f, unsigned long, float, float)

DELEGATE_TO_GL_2(vertexAttrib2fv, VertexAttrib2fv, unsigned long, const float*)

DELEGATE_TO_GL_4(vertexAttrib3f, VertexAttrib3f,
                 unsigned long, float, float, float)

DELEGATE_TO_GL_2(vertexAttrib3fv, VertexAttrib3fv, unsigned long, const float*)

DELEGATE_TO_GL_5(vertexAttrib4f, VertexAttrib4f,
                 unsigned long, float, float, float, float)

DELEGATE_TO_GL_2(vertexAttrib4fv, VertexAttrib4fv, unsigned long, const float*)

void WebGraphicsContext3DCommandBufferImpl::vertexAttribPointer(
    unsigned long indx, int size, int type, bool normalized,
    unsigned long stride, unsigned long offset) {
  makeContextCurrent();

  glVertexAttribPointer(indx, size, type, normalized, stride,
                        reinterpret_cast<void*>(
                            static_cast<intptr_t>(offset)));
}

DELEGATE_TO_GL_4(viewport, Viewport, long, long, unsigned long, unsigned long)

unsigned WebGraphicsContext3DCommandBufferImpl::createBuffer() {
  makeContextCurrent();
  GLuint o;
  glGenBuffers(1, &o);
  return o;
}

unsigned WebGraphicsContext3DCommandBufferImpl::createFramebuffer() {
  makeContextCurrent();
  GLuint o = 0;
  glGenFramebuffers(1, &o);
  return o;
}

unsigned WebGraphicsContext3DCommandBufferImpl::createProgram() {
  makeContextCurrent();
  return glCreateProgram();
}

unsigned WebGraphicsContext3DCommandBufferImpl::createRenderbuffer() {
  makeContextCurrent();
  GLuint o;
  glGenRenderbuffers(1, &o);
  return o;
}

DELEGATE_TO_GL_1R(createShader, CreateShader, unsigned long, unsigned);

unsigned WebGraphicsContext3DCommandBufferImpl::createTexture() {
  makeContextCurrent();
  GLuint o;
  glGenTextures(1, &o);
  return o;
}

void WebGraphicsContext3DCommandBufferImpl::deleteBuffer(unsigned buffer) {
  makeContextCurrent();
  glDeleteBuffers(1, &buffer);
}

void WebGraphicsContext3DCommandBufferImpl::deleteFramebuffer(
    unsigned framebuffer) {
  makeContextCurrent();
  glDeleteFramebuffers(1, &framebuffer);
}

void WebGraphicsContext3DCommandBufferImpl::deleteProgram(unsigned program) {
  makeContextCurrent();
  glDeleteProgram(program);
}

void WebGraphicsContext3DCommandBufferImpl::deleteRenderbuffer(
    unsigned renderbuffer) {
  makeContextCurrent();
  glDeleteRenderbuffers(1, &renderbuffer);
}

void WebGraphicsContext3DCommandBufferImpl::deleteShader(unsigned shader) {
  makeContextCurrent();
  glDeleteShader(shader);
}

void WebGraphicsContext3DCommandBufferImpl::deleteTexture(unsigned texture) {
  makeContextCurrent();
  glDeleteTextures(1, &texture);
}

#endif  // defined(ENABLE_GPU)

