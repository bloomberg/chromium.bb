// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gl_helper.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/size.h"

namespace {

const char kGLHelperThreadName[] = "GLHelperThread";

class GLHelperThread : public base::Thread {
 public:
  GLHelperThread() : base::Thread(kGLHelperThreadName) {
    Start();
  }
  virtual ~GLHelperThread() {}

  DISALLOW_COPY_AND_ASSIGN(GLHelperThread);
};

base::LazyInstance<GLHelperThread> g_gl_helper_thread =
    LAZY_INSTANCE_INITIALIZER;

class ScopedWebGLId {
 public:
  typedef void (WebKit::WebGraphicsContext3D::*DeleteFunc)(WebKit::WebGLId);
  ScopedWebGLId(WebKit::WebGraphicsContext3D* context,
                WebKit::WebGLId id,
                DeleteFunc delete_func)
      : context_(context),
        id_(id),
        delete_func_(delete_func) {
  }

  operator WebKit::WebGLId() const {
    return id_;
  }

  WebKit::WebGLId Detach() {
    WebKit::WebGLId id = id_;
    id_ = 0;
    return id;
  }

  virtual ~ScopedWebGLId() {
    if (id_ != 0)
      (context_->*delete_func_)(id_);
  }

 private:
  WebKit::WebGraphicsContext3D* context_;
  WebKit::WebGLId id_;
  DeleteFunc delete_func_;

  DISALLOW_COPY_AND_ASSIGN(ScopedWebGLId);
};

class ScopedBuffer : public ScopedWebGLId {
 public:
  ScopedBuffer(WebKit::WebGraphicsContext3D* context,
               WebKit::WebGLId id)
      : ScopedWebGLId(context,
                      id,
                      &WebKit::WebGraphicsContext3D::deleteBuffer) {}
};

class ScopedFramebuffer : public ScopedWebGLId {
 public:
  ScopedFramebuffer(WebKit::WebGraphicsContext3D* context,
                    WebKit::WebGLId id)
      : ScopedWebGLId(context,
                      id,
                      &WebKit::WebGraphicsContext3D::deleteFramebuffer) {}
};

class ScopedRenderbuffer : public ScopedWebGLId {
 public:
  ScopedRenderbuffer(WebKit::WebGraphicsContext3D* context,
                     WebKit::WebGLId id)
      : ScopedWebGLId(context,
                      id,
                      &WebKit::WebGraphicsContext3D::deleteRenderbuffer) {}
};

class ScopedProgram : public ScopedWebGLId {
 public:
  ScopedProgram(WebKit::WebGraphicsContext3D* context,
                WebKit::WebGLId id)
      : ScopedWebGLId(context,
                      id,
                      &WebKit::WebGraphicsContext3D::deleteProgram) {}
};

class ScopedShader : public ScopedWebGLId {
 public:
  ScopedShader(WebKit::WebGraphicsContext3D* context,
               WebKit::WebGLId id)
      : ScopedWebGLId(context,
                      id,
                      &WebKit::WebGraphicsContext3D::deleteShader) {}
};

class ScopedTexture : public ScopedWebGLId {
 public:
  ScopedTexture(WebKit::WebGraphicsContext3D* context,
                WebKit::WebGLId id)
      : ScopedWebGLId(context,
                      id,
                      &WebKit::WebGraphicsContext3D::deleteTexture) {}
};

template <WebKit::WGC3Denum target>
class ScopedBinder {
 public:
  typedef void (WebKit::WebGraphicsContext3D::*BindFunc)(WebKit::WGC3Denum,
                                                         WebKit::WebGLId);
  ScopedBinder(WebKit::WebGraphicsContext3D* context,
               WebKit::WebGLId id,
               BindFunc bind_func)
      : context_(context),
        id_(id),
        bind_func_(bind_func) {
    (context_->*bind_func_)(target, id_);
  }

  virtual ~ScopedBinder() {
    if (id_ != 0)
      (context_->*bind_func_)(target, 0);
  }

 private:
  WebKit::WebGraphicsContext3D* context_;
  WebKit::WebGLId id_;
  BindFunc bind_func_;

  DISALLOW_COPY_AND_ASSIGN(ScopedBinder);
};

template <WebKit::WGC3Denum target>
class ScopedBufferBinder : ScopedBinder<target> {
 public:
  ScopedBufferBinder(WebKit::WebGraphicsContext3D* context,
                     WebKit::WebGLId id)
      : ScopedBinder<target>(
          context,
          id,
          &WebKit::WebGraphicsContext3D::bindBuffer) {}
};

template <WebKit::WGC3Denum target>
class ScopedFramebufferBinder : ScopedBinder<target> {
 public:
  ScopedFramebufferBinder(WebKit::WebGraphicsContext3D* context,
                          WebKit::WebGLId id)
      : ScopedBinder<target>(
          context,
          id,
          &WebKit::WebGraphicsContext3D::bindFramebuffer) {}
};

template <WebKit::WGC3Denum target>
class ScopedRenderbufferBinder : ScopedBinder<target> {
 public:
  ScopedRenderbufferBinder(WebKit::WebGraphicsContext3D* context,
                           WebKit::WebGLId id)
      : ScopedBinder<target>(
          context,
          id,
          &WebKit::WebGraphicsContext3D::bindRenderbuffer) {}
};

template <WebKit::WGC3Denum target>
class ScopedTextureBinder : ScopedBinder<target> {
 public:
  ScopedTextureBinder(WebKit::WebGraphicsContext3D* context,
                      WebKit::WebGLId id)
      : ScopedBinder<target>(
          context,
          id,
          &WebKit::WebGraphicsContext3D::bindTexture) {}
};

class ScopedFlush {
 public:
  ScopedFlush(WebKit::WebGraphicsContext3D* context)
      : context_(context) {
  }

  virtual ~ScopedFlush() {
    context_->flush();
  }

 private:
  WebKit::WebGraphicsContext3D* context_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFlush);
};

void ReadBackFramebuffer(
    WebKit::WebGraphicsContext3D* context,
    unsigned char* pixels,
    gfx::Size size,
    WebKit::WebGLId dst_texture,
    bool* result) {
  *result = false;
  if (!context->makeContextCurrent())
    return;
  if (context->isContextLost())
    return;
  ScopedFlush flush(context);
  ScopedFramebuffer dst_framebuffer(context, context->createFramebuffer());
  {
    ScopedFramebufferBinder<GL_DRAW_FRAMEBUFFER> framebuffer_binder(
        context, dst_framebuffer);
    ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(
        context, dst_texture);
    context->framebufferTexture2D(GL_DRAW_FRAMEBUFFER,
                                  GL_COLOR_ATTACHMENT0,
                                  GL_TEXTURE_2D,
                                  dst_texture,
                                  0);
  }
  *result = context->readBackFramebuffer(
        pixels,
        4 * size.GetArea(),
        static_cast<WebKit::WebGLId>(dst_framebuffer),
        size.width(),
        size.height());
}

void ReadBackFramebufferComplete(WebKit::WebGraphicsContext3D* context,
                                 WebKit::WebGLId* dst_texture,
                                 base::Callback<void(bool)> callback,
                                 bool* result) {
  callback.Run(*result);
  if (*dst_texture != 0) {
    context->deleteTexture(*dst_texture);
    context->flush();
    *dst_texture = 0;
  }
}

void DeleteContext(WebKit::WebGraphicsContext3D* context) {
  delete context;
}

void SignalWaitableEvent(base::WaitableEvent* event) {
  event->Signal();
}

}  // namespace

namespace content {

// Implements GLHelper::CopyTextureTo and encapsulates the data needed for it.
class GLHelper::CopyTextureToImpl {
 public:
  CopyTextureToImpl(WebKit::WebGraphicsContext3D* context,
                    WebKit::WebGraphicsContext3D* context_for_thread,
                    GLHelper* helper)
      : context_(context),
        context_for_thread_(context_for_thread),
        helper_(helper),
        program_(context, context->createProgram()),
        vertex_attributes_buffer_(context_, context_->createBuffer()),
        dst_texture_(0) {
    InitBuffer();
    InitProgram();
  }
  ~CopyTextureToImpl() {
    DeleteContextForThread();
  }

  void InitBuffer();
  void InitProgram();
  void Detach();

  bool CopyTextureTo(WebKit::WebGLId src_texture,
                     const gfx::Size& src_size,
                     const gfx::Size& dst_size,
                     unsigned char* out);

  void AsyncCopyTextureTo(WebKit::WebGLId src_texture,
                          const gfx::Size& src_size,
                          const gfx::Size& dst_size,
                          unsigned char* out,
                          base::Callback<void(bool)> callback);
 private:
  // Returns the id of a framebuffer that
  WebKit::WebGLId ScaleTexture(WebKit::WebGLId src_texture,
                               const gfx::Size& src_size,
                               const gfx::Size& dst_size);

  // Deletes the context for GLHelperThread.
  void DeleteContextForThread();

  // Interleaved array of 2-dimentional vertex positions (x, y) and
  // 2-dimentional texture coordinates (s, t).
  static const WebKit::WGC3Dfloat kVertexAttributes[];
  // Shader sources used for GLHelper::CopyTextureTo
  static const WebKit::WGC3Dchar kCopyVertexShader[];
  static const WebKit::WGC3Dchar kCopyFragmentShader[];

  WebKit::WebGraphicsContext3D* context_;
  WebKit::WebGraphicsContext3D* context_for_thread_;
  GLHelper* helper_;

  // A program for copying a source texture into a destination texture.
  ScopedProgram program_;
  // The buffer that holds the vertices and the texture coordinates data for
  // drawing a quad.
  ScopedBuffer vertex_attributes_buffer_;
  // The location of the position in the program.
  WebKit::WGC3Dint position_location_;
  // The location of the texture coordinate in the program.
  WebKit::WGC3Dint texcoord_location_;
  // The location of the source texture in the program.
  WebKit::WGC3Dint texture_location_;
  // The destination texture id. This is used only for the asynchronous version.
  WebKit::WebGLId dst_texture_;
};

const WebKit::WGC3Dfloat GLHelper::CopyTextureToImpl::kVertexAttributes[] = {
  -1.0f, -1.0f, 0.0f, 0.0f,
  1.0f, -1.0f, 1.0f, 0.0f,
  -1.0f, 1.0f, 0.0f, 1.0f,
  1.0f, 1.0f, 1.0f, 1.0f,
};

const WebKit::WGC3Dchar GLHelper::CopyTextureToImpl::kCopyVertexShader[] =
    "attribute vec2 a_position;"
    "attribute vec2 a_texcoord;"
    "varying vec2 v_texcoord;"
    "void main() {"
    "  gl_Position = vec4(a_position, 0.0, 1.0);"
    "  v_texcoord = a_texcoord;"
    "}";

const WebKit::WGC3Dchar GLHelper::CopyTextureToImpl::kCopyFragmentShader[] =
    "precision mediump float;"
    "varying vec2 v_texcoord;"
    "uniform sampler2D s_texture;"
    "void main() {"
    "  gl_FragColor = texture2D(s_texture, v_texcoord);"
    "}";

void GLHelper::CopyTextureToImpl::InitBuffer() {
  ScopedBufferBinder<GL_ARRAY_BUFFER> buffer_binder(
      context_, vertex_attributes_buffer_);
  context_->bufferData(GL_ARRAY_BUFFER,
                       sizeof(kVertexAttributes),
                       kVertexAttributes,
                       GL_STATIC_DRAW);
}

void GLHelper::CopyTextureToImpl::InitProgram() {
  // Shaders to map the source texture to |dst_texture_|.
  ScopedShader vertex_shader(context_, helper_->CompileShaderFromSource(
      kCopyVertexShader, GL_VERTEX_SHADER));
  DCHECK_NE(0U, static_cast<WebKit::WebGLId>(vertex_shader));
  context_->attachShader(program_, vertex_shader);
  ScopedShader fragment_shader(context_, helper_->CompileShaderFromSource(
      kCopyFragmentShader, GL_FRAGMENT_SHADER));
  DCHECK_NE(0U, static_cast<WebKit::WebGLId>(fragment_shader));
  context_->attachShader(program_, fragment_shader);
  context_->linkProgram(program_);

  WebKit::WGC3Dint link_status = 0;
  context_->getProgramiv(program_, GL_LINK_STATUS, &link_status);
  if (!link_status) {
    LOG(ERROR) << std::string(context_->getProgramInfoLog(program_).utf8());
    return;
  }

  position_location_ = context_->getAttribLocation(program_, "a_position");
  texcoord_location_ = context_->getAttribLocation(program_, "a_texcoord");
  texture_location_ = context_->getUniformLocation(program_, "s_texture");
}

void GLHelper::CopyTextureToImpl::Detach() {
  program_.Detach();
  vertex_attributes_buffer_.Detach();
  dst_texture_ = 0;
}

WebKit::WebGLId GLHelper::CopyTextureToImpl::ScaleTexture(
    WebKit::WebGLId src_texture,
    const gfx::Size& src_size,
    const gfx::Size& dst_size) {
  WebKit::WebGLId dst_texture = context_->createTexture();
  {
    ScopedFramebuffer dst_framebuffer(context_, context_->createFramebuffer());
    ScopedFramebufferBinder<GL_DRAW_FRAMEBUFFER> framebuffer_binder(
        context_, dst_framebuffer);
    {
      ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(
          context_, dst_texture);
      context_->texImage2D(GL_TEXTURE_2D,
                           0,
                           GL_RGBA,
                           dst_size.width(),
                           dst_size.height(),
                           0,
                           GL_RGBA,
                           GL_UNSIGNED_BYTE,
                           NULL);
      context_->framebufferTexture2D(GL_DRAW_FRAMEBUFFER,
                                     GL_COLOR_ATTACHMENT0,
                                     GL_TEXTURE_2D,
                                     dst_texture,
                                     0);
    }

    ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(context_, src_texture);
    ScopedBufferBinder<GL_ARRAY_BUFFER> buffer_binder(
        context_, vertex_attributes_buffer_);

    context_->viewport(0, 0, dst_size.width(), dst_size.height());
    context_->useProgram(program_);

    WebKit::WGC3Dintptr offset = 0;
    context_->vertexAttribPointer(position_location_,
                                  2,
                                  GL_FLOAT,
                                  GL_FALSE,
                                  4 * sizeof(WebKit::WGC3Dfloat),
                                  offset);
    context_->enableVertexAttribArray(position_location_);

    offset += 2 * sizeof(WebKit::WGC3Dfloat);
    context_->vertexAttribPointer(texcoord_location_,
                                  2,
                                  GL_FLOAT,
                                  GL_FALSE,
                                  4 * sizeof(WebKit::WGC3Dfloat),
                                  offset);
    context_->enableVertexAttribArray(texcoord_location_);

    context_->uniform1i(texture_location_, 0);

    // Conduct texture mapping by drawing a quad composed of two triangles.
    context_->drawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }
  return dst_texture;
}

void GLHelper::CopyTextureToImpl::DeleteContextForThread() {
  if (!context_for_thread_)
    return;

  g_gl_helper_thread.Pointer()->message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&DeleteContext,
                 context_for_thread_));
  context_for_thread_ = NULL;
}

bool GLHelper::CopyTextureToImpl::CopyTextureTo(
    WebKit::WebGLId src_texture,
    const gfx::Size& src_size,
    const gfx::Size& dst_size,
    unsigned char* out) {
  ScopedFlush flush(context_);
  ScopedTexture dst_texture(context_,
                            ScaleTexture(src_texture, src_size, dst_size));
  ScopedFramebuffer dst_framebuffer(context_, context_->createFramebuffer());
  {
    ScopedFramebufferBinder<GL_DRAW_FRAMEBUFFER> framebuffer_binder(
        context_, dst_framebuffer);
    ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(
        context_, dst_framebuffer);
    context_->framebufferTexture2D(GL_DRAW_FRAMEBUFFER,
                                   GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D,
                                   dst_texture,
                                   0);
  }
  return context_->readBackFramebuffer(
      out,
      4 * dst_size.GetArea(),
      static_cast<WebKit::WebGLId>(dst_framebuffer),
      dst_size.width(),
      dst_size.height());
}

void GLHelper::CopyTextureToImpl::AsyncCopyTextureTo(
    WebKit::WebGLId src_texture,
    const gfx::Size& src_size,
    const gfx::Size& dst_size,
    unsigned char* out,
    base::Callback<void(bool)> callback) {
  if (!context_for_thread_) {
    callback.Run(false);
    return;
  }

  dst_texture_ = ScaleTexture(src_texture, src_size, dst_size);
  context_->flush();
  bool* result = new bool(false);
  g_gl_helper_thread.Pointer()->message_loop_proxy()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&ReadBackFramebuffer,
                 context_for_thread_,
                 out,
                 dst_size,
                 dst_texture_,
                 result),
      base::Bind(&ReadBackFramebufferComplete,
                 context_,
                 &dst_texture_,
                 callback,
                 base::Owned(result)));
}

base::subtle::Atomic32 GLHelper::count_ = 0;

GLHelper::GLHelper(WebKit::WebGraphicsContext3D* context,
                   WebKit::WebGraphicsContext3D* context_for_thread)
    : context_(context),
      context_for_thread_(context_for_thread) {
  base::subtle::NoBarrier_AtomicIncrement(&count_, 1);
}

GLHelper::~GLHelper() {
  DCHECK_NE(MessageLoop::current(),
            g_gl_helper_thread.Pointer()->message_loop());
  base::subtle::Atomic32 decremented_count =
    base::subtle::NoBarrier_AtomicIncrement(&count_, -1);
  if (decremented_count == 0) {
    // When this is the last instance, we synchronize with the pending
    // operations on GLHelperThread. Otherwise on shutdown we may kill the GPU
    // process infrastructure (BrowserGpuChannelHostFactory) before they have
    // a chance to complete, likely leading to a crash.
    base::WaitableEvent event(false, false);
    g_gl_helper_thread.Pointer()->message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(&SignalWaitableEvent,
                   &event));
    // http://crbug.com/125415
    base::ThreadRestrictions::ScopedAllowWait allow_wait;
    event.Wait();
  }
}

WebKit::WebGraphicsContext3D* GLHelper::context() const {
  return context_;
}

void GLHelper::Detach() {
  if (copy_texture_to_impl_.get())
    copy_texture_to_impl_->Detach();
}

bool GLHelper::CopyTextureTo(WebKit::WebGLId src_texture,
                             const gfx::Size& src_size,
                             const gfx::Size& dst_size,
                             unsigned char* out) {
  // Lazily initialize |copy_texture_to_impl_|
  if (!copy_texture_to_impl_.get())
    copy_texture_to_impl_.reset(new CopyTextureToImpl(context_,
                                                      context_for_thread_,
                                                      this));

  return copy_texture_to_impl_->CopyTextureTo(
      src_texture,
      src_size,
      dst_size,
      out);
}

void GLHelper::AsyncCopyTextureTo(WebKit::WebGLId src_texture,
                                  const gfx::Size& src_size,
                                  const gfx::Size& dst_size,
                                  unsigned char* out,
                                  const base::Callback<void(bool)>& callback) {
  // Lazily initialize |copy_texture_to_impl_|
  if (!copy_texture_to_impl_.get())
    copy_texture_to_impl_.reset(new CopyTextureToImpl(context_,
                                                      context_for_thread_,
                                                      this));

  copy_texture_to_impl_->AsyncCopyTextureTo(src_texture,
                                            src_size,
                                            dst_size,
                                            out,
                                            callback);
}

WebKit::WebGLId GLHelper::CompileShaderFromSource(
    const WebKit::WGC3Dchar* source,
    WebKit::WGC3Denum type) {
  ScopedShader shader(context_, context_->createShader(type));
  context_->shaderSource(shader, source);
  context_->compileShader(shader);
  WebKit::WGC3Dint compile_status = 0;
  context_->getShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
  if (!compile_status) {
    LOG(ERROR) << std::string(context_->getShaderInfoLog(shader).utf8());
    return 0;
  }
  return shader.Detach();
}

}  // namespace content
