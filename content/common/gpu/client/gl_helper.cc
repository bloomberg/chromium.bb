// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gl_helper.h"

#include <queue>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "ui/gfx/size.h"
#include "ui/gl/gl_bindings.h"

using WebKit::WebGLId;
using WebKit::WebGraphicsContext3D;

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
  typedef void (WebGraphicsContext3D::*DeleteFunc)(WebGLId);
  ScopedWebGLId(WebGraphicsContext3D* context,
                WebGLId id,
                DeleteFunc delete_func)
      : context_(context),
        id_(id),
        delete_func_(delete_func) {
  }

  operator WebGLId() const {
    return id_;
  }

  WebGLId id() const { return id_; }

  WebGLId Detach() {
    WebGLId id = id_;
    id_ = 0;
    return id;
  }

  virtual ~ScopedWebGLId() {
    if (id_ != 0)
      (context_->*delete_func_)(id_);
  }

 private:
  WebGraphicsContext3D* context_;
  WebGLId id_;
  DeleteFunc delete_func_;

  DISALLOW_COPY_AND_ASSIGN(ScopedWebGLId);
};

class ScopedBuffer : public ScopedWebGLId {
 public:
  ScopedBuffer(WebGraphicsContext3D* context,
               WebGLId id)
      : ScopedWebGLId(context,
                      id,
                      &WebGraphicsContext3D::deleteBuffer) {}
};

class ScopedFramebuffer : public ScopedWebGLId {
 public:
  ScopedFramebuffer(WebGraphicsContext3D* context,
                    WebGLId id)
      : ScopedWebGLId(context,
                      id,
                      &WebGraphicsContext3D::deleteFramebuffer) {}
};

class ScopedRenderbuffer : public ScopedWebGLId {
 public:
  ScopedRenderbuffer(WebGraphicsContext3D* context,
                     WebGLId id)
      : ScopedWebGLId(context,
                      id,
                      &WebGraphicsContext3D::deleteRenderbuffer) {}
};

class ScopedProgram : public ScopedWebGLId {
 public:
  ScopedProgram(WebGraphicsContext3D* context,
                WebGLId id)
      : ScopedWebGLId(context,
                      id,
                      &WebGraphicsContext3D::deleteProgram) {}
};

class ScopedShader : public ScopedWebGLId {
 public:
  ScopedShader(WebGraphicsContext3D* context,
               WebGLId id)
      : ScopedWebGLId(context,
                      id,
                      &WebGraphicsContext3D::deleteShader) {}
};

class ScopedTexture : public ScopedWebGLId {
 public:
  ScopedTexture(WebGraphicsContext3D* context,
                WebGLId id)
      : ScopedWebGLId(context,
                      id,
                      &WebGraphicsContext3D::deleteTexture) {}
};

template <WebKit::WGC3Denum target>
class ScopedBinder {
 public:
  typedef void (WebGraphicsContext3D::*BindFunc)(WebKit::WGC3Denum,
                                                         WebGLId);
  ScopedBinder(WebGraphicsContext3D* context,
               WebGLId id,
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
  WebGraphicsContext3D* context_;
  WebGLId id_;
  BindFunc bind_func_;

  DISALLOW_COPY_AND_ASSIGN(ScopedBinder);
};

template <WebKit::WGC3Denum target>
class ScopedBufferBinder : ScopedBinder<target> {
 public:
  ScopedBufferBinder(WebGraphicsContext3D* context,
                     WebGLId id)
      : ScopedBinder<target>(
          context,
          id,
          &WebGraphicsContext3D::bindBuffer) {}
};

template <WebKit::WGC3Denum target>
class ScopedFramebufferBinder : ScopedBinder<target> {
 public:
  ScopedFramebufferBinder(WebGraphicsContext3D* context,
                          WebGLId id)
      : ScopedBinder<target>(
          context,
          id,
          &WebGraphicsContext3D::bindFramebuffer) {}
};

template <WebKit::WGC3Denum target>
class ScopedRenderbufferBinder : ScopedBinder<target> {
 public:
  ScopedRenderbufferBinder(WebGraphicsContext3D* context,
                           WebGLId id)
      : ScopedBinder<target>(
          context,
          id,
          &WebGraphicsContext3D::bindRenderbuffer) {}
};

template <WebKit::WGC3Denum target>
class ScopedTextureBinder : ScopedBinder<target> {
 public:
  ScopedTextureBinder(WebGraphicsContext3D* context,
                      WebGLId id)
      : ScopedBinder<target>(
          context,
          id,
          &WebGraphicsContext3D::bindTexture) {}
};

class ScopedFlush {
 public:
  ScopedFlush(WebGraphicsContext3D* context)
      : context_(context) {
  }

  virtual ~ScopedFlush() {
    context_->flush();
  }

 private:
  WebGraphicsContext3D* context_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFlush);
};

void DeleteContext(WebGraphicsContext3D* context) {
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
  CopyTextureToImpl(WebGraphicsContext3D* context,
                    WebGraphicsContext3D* context_for_thread,
                    GLHelper* helper)
      : context_(context),
        context_for_thread_(context_for_thread),
        helper_(helper),
        flush_(context),
        program_(context, context->createProgram()),
        vertex_attributes_buffer_(context_, context_->createBuffer()) {
    InitBuffer();
    InitProgram();
  }
  ~CopyTextureToImpl() {
    CancelRequests();
    DeleteContextForThread();
  }

  void InitBuffer();
  void InitProgram();

  void CopyTextureTo(WebGLId src_texture,
                     const gfx::Size& src_size,
                     const gfx::Size& dst_size,
                     unsigned char* out,
                     const base::Callback<void(bool)>& callback);
 private:
  // A single request to CopyTextureTo.
  // Thread-safety notes: the main thread creates instances of this class. The
  // main thread can cancel the request, before it's handled by the helper
  // thread, by resetting the texture and pixels fields. Alternatively, the
  // thread marks that it handles the request by resetting the pixels field
  // (meaning it guarantees that the callback with be called).
  // In either case, the callback must be called exactly once, and the texture
  // must be deleted by the main thread context.
  struct Request : public base::RefCounted<Request> {
    Request(CopyTextureToImpl* impl,
            WebGLId texture_,
            const gfx::Size& size_,
            unsigned char* pixels_,
            const base::Callback<void(bool)>& callback_)
        : copy_texture_impl(impl),
        size(size_),
        callback(callback_),
        lock(),
        texture(texture_),
        pixels(pixels_) {
    }

    // These members are only accessed on the main thread.
    GLHelper::CopyTextureToImpl* copy_texture_impl;
    gfx::Size size;
    base::Callback<void(bool)> callback;

    // Locks access to below members, which can be accessed on any thread.
    base::Lock lock;
    WebGLId texture;
    unsigned char* pixels;

   private:
    friend class base::RefCounted<Request>;
    ~Request() {}
  };

  // Returns the id of a framebuffer that
  WebGLId ScaleTexture(WebGLId src_texture,
                               const gfx::Size& src_size,
                               const gfx::Size& dst_size);

  // Deletes the context for GLHelperThread.
  void DeleteContextForThread();
  static void ReadBackFramebuffer(scoped_refptr<Request> request,
                                  WebGraphicsContext3D* context,
                                  scoped_refptr<base::TaskRunner> reply_loop);
  static void ReadBackFramebufferComplete(scoped_refptr<Request> request,
                                          bool result);
  void FinishRequest(scoped_refptr<Request> request);
  void CancelRequests();

  // Interleaved array of 2-dimentional vertex positions (x, y) and
  // 2-dimentional texture coordinates (s, t).
  static const WebKit::WGC3Dfloat kVertexAttributes[];
  // Shader sources used for GLHelper::CopyTextureTo
  static const WebKit::WGC3Dchar kCopyVertexShader[];
  static const WebKit::WGC3Dchar kCopyFragmentShader[];

  WebGraphicsContext3D* context_;
  WebGraphicsContext3D* context_for_thread_;
  GLHelper* helper_;

  // A scoped flush that will ensure all resource deletions are flushed when
  // this object is destroyed. Must be declared before other Scoped* fields.
  ScopedFlush flush_;
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
  std::queue<scoped_refptr<Request> > request_queue_;
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
  DCHECK_NE(0U, vertex_shader.id());
  context_->attachShader(program_, vertex_shader);
  ScopedShader fragment_shader(context_, helper_->CompileShaderFromSource(
      kCopyFragmentShader, GL_FRAGMENT_SHADER));
  DCHECK_NE(0U, fragment_shader.id());
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

WebGLId GLHelper::CopyTextureToImpl::ScaleTexture(
    WebGLId src_texture,
    const gfx::Size& src_size,
    const gfx::Size& dst_size) {
  WebGLId dst_texture = context_->createTexture();
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

void GLHelper::CopyTextureToImpl::CopyTextureTo(
    WebGLId src_texture,
    const gfx::Size& src_size,
    const gfx::Size& dst_size,
    unsigned char* out,
    const base::Callback<void(bool)>& callback) {
  if (!context_for_thread_) {
    callback.Run(false);
    return;
  }

  WebGLId texture = ScaleTexture(src_texture, src_size, dst_size);
  context_->flush();
  scoped_refptr<Request> request =
      new Request(this, texture, dst_size, out, callback);
  request_queue_.push(request);

  g_gl_helper_thread.Pointer()->message_loop_proxy()->PostTask(FROM_HERE,
      base::Bind(&ReadBackFramebuffer,
                 request,
                 context_for_thread_,
                 base::MessageLoopProxy::current()));
}

void GLHelper::CopyTextureToImpl::ReadBackFramebuffer(
    scoped_refptr<Request> request,
    WebGraphicsContext3D* context,
    scoped_refptr<base::TaskRunner> reply_loop) {
  DCHECK(context);
  if (!context->makeContextCurrent() || context->isContextLost()) {
    base::AutoLock auto_lock(request->lock);
    if (request->pixels) {
      // Only report failure if the request wasn't canceled (otherwise the
      // failure has already been reported).
      request->pixels = NULL;
      reply_loop->PostTask(
          FROM_HERE, base::Bind(ReadBackFramebufferComplete, request, false));
    }
    return;
  }
  ScopedFlush flush(context);
  ScopedFramebuffer dst_framebuffer(context, context->createFramebuffer());
  unsigned char* pixels = NULL;
  gfx::Size size;
  {
    // Note: We don't want to keep the lock while doing the readBack (since we
    // don't want to block the UI thread). We rely on the fact that once the
    // texture is bound to a FBO (that isn't current), deleting the texture is
    // delayed until the FBO is deleted. We ensure ordering by flushing while
    // the lock is held. Either the main thread cancelled before we get the
    // lock, and we'll exit early, or we ensure that the texture is bound to the
    // framebuffer before the main thread has a chance to delete it.
    base::AutoLock auto_lock(request->lock);
    if (!request->texture || !request->pixels)
      return;
    pixels = request->pixels;
    request->pixels = NULL;
    size = request->size;
    {
      ScopedFlush flush(context);
      ScopedFramebufferBinder<GL_DRAW_FRAMEBUFFER> framebuffer_binder(
          context, dst_framebuffer);
      ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(
          context, request->texture);
      context->framebufferTexture2D(GL_DRAW_FRAMEBUFFER,
                                    GL_COLOR_ATTACHMENT0,
                                    GL_TEXTURE_2D,
                                    request->texture,
                                    0);
    }
  }
  bool result = context->readBackFramebuffer(
        pixels,
        4 * size.GetArea(),
        dst_framebuffer.id(),
        size.width(),
        size.height());
  reply_loop->PostTask(
      FROM_HERE, base::Bind(ReadBackFramebufferComplete, request, result));
}

void GLHelper::CopyTextureToImpl::ReadBackFramebufferComplete(
    scoped_refptr<Request> request,
    bool result) {
  request->callback.Run(result);
  if (request->copy_texture_impl)
    request->copy_texture_impl->FinishRequest(request);
}

void GLHelper::CopyTextureToImpl::FinishRequest(
    scoped_refptr<Request> request) {
  CHECK(request_queue_.front() == request);
  request_queue_.pop();
  base::AutoLock auto_lock(request->lock);
  if (request->texture != 0) {
    context_->deleteTexture(request->texture);
    request->texture = 0;
    context_->flush();
  }
}

void GLHelper::CopyTextureToImpl::CancelRequests() {
  while (!request_queue_.empty()) {
    scoped_refptr<Request> request = request_queue_.front();
    request_queue_.pop();
    request->copy_texture_impl = NULL;
    bool cancelled = false;
    {
      base::AutoLock auto_lock(request->lock);
      if (request->texture != 0) {
        context_->deleteTexture(request->texture);
        request->texture = 0;
      }
      if (request->pixels != NULL) {
        request->pixels = NULL;
        cancelled = true;
      }
    }
    if (cancelled)
      request->callback.Run(false);
  }
}

base::subtle::Atomic32 GLHelper::count_ = 0;

GLHelper::GLHelper(WebGraphicsContext3D* context,
                   WebGraphicsContext3D* context_for_thread)
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

WebGraphicsContext3D* GLHelper::context() const {
  return context_;
}

void GLHelper::CopyTextureTo(WebGLId src_texture,
                             const gfx::Size& src_size,
                             const gfx::Size& dst_size,
                             unsigned char* out,
                             const base::Callback<void(bool)>& callback) {
  // Lazily initialize |copy_texture_to_impl_|
  if (!copy_texture_to_impl_.get())
    copy_texture_to_impl_.reset(new CopyTextureToImpl(context_,
                                                      context_for_thread_,
                                                      this));

  copy_texture_to_impl_->CopyTextureTo(src_texture,
                                       src_size,
                                       dst_size,
                                       out,
                                       callback);
}

WebGLId GLHelper::CompileShaderFromSource(
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
