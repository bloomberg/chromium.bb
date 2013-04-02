// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gl_helper.h"

#include <queue>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gl/gl_bindings.h"

using WebKit::WebGLId;
using WebKit::WebGraphicsContext3D;

namespace {

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
        bind_func_(bind_func) {
    (context_->*bind_func_)(target, id);
  }

  virtual ~ScopedBinder() {
    (context_->*bind_func_)(target, 0);
  }

 private:
  WebGraphicsContext3D* context_;
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
  explicit ScopedFlush(WebGraphicsContext3D* context)
      : context_(context) {
  }

  virtual ~ScopedFlush() {
    context_->flush();
  }

 private:
  WebGraphicsContext3D* context_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFlush);
};

}  // namespace

namespace content {

// Implements GLHelper::CropScaleReadbackAndCleanTexture and encapsulates the
// data needed for it.
class GLHelper::CopyTextureToImpl :
      public base::SupportsWeakPtr<GLHelper::CopyTextureToImpl> {
 public:
  CopyTextureToImpl(WebGraphicsContext3DCommandBufferImpl* context,
                    GLHelper* helper)
      : context_(context),
        helper_(helper),
        flush_(context),
        program_(context, context->createProgram()),
        vertex_attributes_buffer_(context_, context_->createBuffer()),
        flipped_vertex_attributes_buffer_(context_, context_->createBuffer()) {
    InitBuffer();
    InitProgram();
  }
  ~CopyTextureToImpl() {
    CancelRequests();
  }

  void InitBuffer();
  void InitProgram();

  void CropScaleReadbackAndCleanTexture(
      WebGLId src_texture,
      const gfx::Size& src_size,
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      unsigned char* out,
      const base::Callback<void(bool)>& callback);

  void ReadbackTextureSync(WebGLId texture,
                           const gfx::Rect& src_rect,
                           unsigned char* out);

  WebKit::WebGLId CopyAndScaleTexture(WebGLId texture,
                                      const gfx::Size& src_size,
                                      const gfx::Size& dst_size,
                                      bool vertically_flip_texture);

 private:
  // A single request to CropScaleReadbackAndCleanTexture.
  // The main thread can cancel the request, before it's handled by the helper
  // thread, by resetting the texture and pixels fields. Alternatively, the
  // thread marks that it handles the request by resetting the pixels field
  // (meaning it guarantees that the callback with be called).
  // In either case, the callback must be called exactly once, and the texture
  // must be deleted by the main thread context.
  struct Request {
    Request(WebGLId texture_,
            const gfx::Size& size_,
            unsigned char* pixels_,
            const base::Callback<void(bool)>& callback_)
        : size(size_),
          callback(callback_),
          texture(texture_),
          pixels(pixels_),
          buffer(0) {
    }

    gfx::Size size;
    base::Callback<void(bool)> callback;

    WebGLId texture;
    unsigned char* pixels;
    GLuint buffer;
  };

  // Copies the block of pixels specified with |src_subrect| from |src_texture|,
  // scales it to |dst_size|, writes it into a texture, and returns its ID.
  // |src_size| is the size of |src_texture|.
  WebGLId ScaleTexture(WebGLId src_texture,
                       const gfx::Size& src_size,
                       const gfx::Rect& src_subrect,
                       const gfx::Size& dst_size,
                       bool vertically_flip_texture);

  void ReadbackDone(Request* request);
  void FinishRequest(Request* request, bool result);
  void CancelRequests();

  // Interleaved array of 2-dimentional vertex positions (x, y) and
  // 2-dimentional texture coordinates (s, t).
  static const WebKit::WGC3Dfloat kVertexAttributes[];
  // Interleaved array of 2-dimensional vertex positions (x, y) and
  // 2 dimensional texture coordinates (s, t).
  static const WebKit::WGC3Dfloat kFlippedVertexAttributes[];
  // Shader sources used for GLHelper::CropScaleReadbackAndCleanTexture and
  // GLHelper::ReadbackTextureSync
  static const WebKit::WGC3Dchar kCopyVertexShader[];
  static const WebKit::WGC3Dchar kCopyFragmentShader[];

  WebGraphicsContext3DCommandBufferImpl* context_;
  GLHelper* helper_;

  // A scoped flush that will ensure all resource deletions are flushed when
  // this object is destroyed. Must be declared before other Scoped* fields.
  ScopedFlush flush_;
  // A program for copying a source texture into a destination texture.
  ScopedProgram program_;
  // The buffer that holds the vertices and the texture coordinates data for
  // drawing a quad.
  ScopedBuffer vertex_attributes_buffer_;
  ScopedBuffer flipped_vertex_attributes_buffer_;

  // The location of the position in the program.
  WebKit::WGC3Dint position_location_;
  // The location of the texture coordinate in the program.
  WebKit::WGC3Dint texcoord_location_;
  // The location of the source texture in the program.
  WebKit::WGC3Dint texture_location_;
  // The location of the texture coordinate of the sub-rectangle in the program.
  WebKit::WGC3Dint src_subrect_location_;
  std::queue<Request*> request_queue_;
};

const WebKit::WGC3Dfloat GLHelper::CopyTextureToImpl::kVertexAttributes[] = {
  -1.0f, -1.0f, 0.0f, 0.0f,
  1.0f, -1.0f, 1.0f, 0.0f,
  -1.0f, 1.0f, 0.0f, 1.0f,
  1.0f, 1.0f, 1.0f, 1.0f,
};

const WebKit::WGC3Dfloat
GLHelper::CopyTextureToImpl::kFlippedVertexAttributes[] = {
  -1.0f, -1.0f, 0.0f, 1.0f,
  1.0f, -1.0f, 1.0f, 1.0f,
  -1.0f, 1.0f, 0.0f, 0.0f,
  1.0f, 1.0f, 1.0f, 0.0f,
};

const WebKit::WGC3Dchar GLHelper::CopyTextureToImpl::kCopyVertexShader[] =
    "attribute vec2 a_position;"
    "attribute vec2 a_texcoord;"
    "varying vec2 v_texcoord;"
    "uniform vec4 src_subrect;"
    "void main() {"
    "  gl_Position = vec4(a_position, 0.0, 1.0);"
    "  v_texcoord = src_subrect.xy + a_texcoord * src_subrect.zw;"
    "}";


#if (SK_R32_SHIFT == 16) && !SK_B32_SHIFT
const WebKit::WGC3Dchar GLHelper::CopyTextureToImpl::kCopyFragmentShader[] =
    "precision mediump float;"
    "varying vec2 v_texcoord;"
    "uniform sampler2D s_texture;"
    "void main() {"
    "  gl_FragColor = texture2D(s_texture, v_texcoord).bgra;"
    "}";
#else
const WebKit::WGC3Dchar GLHelper::CopyTextureToImpl::kCopyFragmentShader[] =
    "precision mediump float;"
    "varying vec2 v_texcoord;"
    "uniform sampler2D s_texture;"
    "void main() {"
    "  gl_FragColor = texture2D(s_texture, v_texcoord);"
    "}";
#endif


void GLHelper::CopyTextureToImpl::InitBuffer() {
  ScopedBufferBinder<GL_ARRAY_BUFFER> buffer_binder(
      context_, vertex_attributes_buffer_);
  context_->bufferData(GL_ARRAY_BUFFER,
                       sizeof(kVertexAttributes),
                       kVertexAttributes,
                       GL_STATIC_DRAW);
  ScopedBufferBinder<GL_ARRAY_BUFFER> flipped_buffer_binder(
      context_, flipped_vertex_attributes_buffer_);
  context_->bufferData(GL_ARRAY_BUFFER,
                       sizeof(kFlippedVertexAttributes),
                       kFlippedVertexAttributes,
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
  src_subrect_location_ = context_->getUniformLocation(program_, "src_subrect");
}

WebGLId GLHelper::CopyTextureToImpl::ScaleTexture(
    WebGLId src_texture,
    const gfx::Size& src_size,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    bool vertically_flip_texture) {
  WebGLId dst_texture = context_->createTexture();
  {
    ScopedFramebuffer dst_framebuffer(context_, context_->createFramebuffer());
    ScopedFramebufferBinder<GL_FRAMEBUFFER> framebuffer_binder(context_,
                                                               dst_framebuffer);
    {
      ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(context_, dst_texture);
      context_->texImage2D(GL_TEXTURE_2D,
                           0,
                           GL_RGBA,
                           dst_size.width(),
                           dst_size.height(),
                           0,
                           GL_RGBA,
                           GL_UNSIGNED_BYTE,
                           NULL);
      context_->framebufferTexture2D(GL_FRAMEBUFFER,
                                     GL_COLOR_ATTACHMENT0,
                                     GL_TEXTURE_2D,
                                     dst_texture,
                                     0);
    }

    ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(context_, src_texture);
    WebKit::WebGLId vertex_attributes_buffer = vertically_flip_texture ?
        flipped_vertex_attributes_buffer_ : vertex_attributes_buffer_;
    ScopedBufferBinder<GL_ARRAY_BUFFER> buffer_binder(context_,
                                                      vertex_attributes_buffer);

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

    // Convert |src_subrect| to texture coordinates.
    GLfloat src_subrect_texcoord[] = {
      static_cast<float>(src_subrect.x()) / src_size.width(),
      static_cast<float>(src_subrect.y()) / src_size.height(),
      static_cast<float>(src_subrect.width()) / src_size.width(),
      static_cast<float>(src_subrect.height()) / src_size.height(),
    };

    context_->uniform4fv(src_subrect_location_, 1, src_subrect_texcoord);

    // Conduct texture mapping by drawing a quad composed of two triangles.
    context_->drawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }
  return dst_texture;
}

void GLHelper::CopyTextureToImpl::CropScaleReadbackAndCleanTexture(
    WebGLId src_texture,
    const gfx::Size& src_size,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    unsigned char* out,
    const base::Callback<void(bool)>& callback) {
  WebGLId texture = ScaleTexture(src_texture,
                                 src_size,
                                 src_subrect,
                                 dst_size,
                                 true);
  context_->flush();
  Request* request = new Request(texture, dst_size, out, callback);
  request_queue_.push(request);

  ScopedFlush flush(context_);
  ScopedFramebuffer dst_framebuffer(context_, context_->createFramebuffer());
  gfx::Size size;
  size = request->size;

  ScopedFramebufferBinder<GL_FRAMEBUFFER> framebuffer_binder(context_,
                                                             dst_framebuffer);
  ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(context_, request->texture);
  context_->framebufferTexture2D(GL_FRAMEBUFFER,
                                 GL_COLOR_ATTACHMENT0,
                                 GL_TEXTURE_2D,
                                 request->texture,
                                 0);
  request->buffer = context_->createBuffer();
  context_->bindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM,
                       request->buffer);
  context_->bufferData(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM,
                       4 * size.GetArea(),
                       NULL,
                       GL_STREAM_READ);

  context_->readPixels(0, 0, size.width(), size.height(),
                       GL_RGBA, GL_UNSIGNED_BYTE, 0);
  context_->bindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, 0);
  context_->GetCommandBufferProxy()->SignalSyncPoint(
      context_->insertSyncPoint(),
      base::Bind(&CopyTextureToImpl::ReadbackDone, AsWeakPtr(), request));
}

void GLHelper::CopyTextureToImpl::ReadbackTextureSync(WebGLId texture,
                                                      const gfx::Rect& src_rect,
                                                      unsigned char* out) {
  ScopedFramebuffer dst_framebuffer(context_, context_->createFramebuffer());
  ScopedFramebufferBinder<GL_FRAMEBUFFER> framebuffer_binder(context_,
                                                             dst_framebuffer);
  ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(context_, texture);
  context_->framebufferTexture2D(GL_FRAMEBUFFER,
                                 GL_COLOR_ATTACHMENT0,
                                 GL_TEXTURE_2D,
                                 texture,
                                 0);
  context_->readPixels(src_rect.x(),
                       src_rect.y(),
                       src_rect.width(),
                       src_rect.height(),
                       GL_RGBA,
                       GL_UNSIGNED_BYTE,
                       out);
}

WebKit::WebGLId GLHelper::CopyTextureToImpl::CopyAndScaleTexture(
    WebGLId src_texture,
    const gfx::Size& src_size,
    const gfx::Size& dst_size,
    bool vertically_flip_texture) {
  return ScaleTexture(src_texture,
                      src_size,
                      gfx::Rect(src_size),
                      dst_size,
                      vertically_flip_texture);
}

void GLHelper::CopyTextureToImpl::ReadbackDone(Request* request) {
  TRACE_EVENT0("mirror",
               "GLHelper::CopyTextureToImpl::CheckReadBackFramebufferComplete");
  DCHECK(request == request_queue_.front());

  bool result = false;
  if (request->buffer != 0) {
    context_->bindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM,
                         request->buffer);
    void* data = context_->mapBufferCHROMIUM(
        GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, GL_READ_ONLY);

    if (data) {
      result = true;
      memcpy(request->pixels, data, request->size.GetArea() * 4);
      context_->unmapBufferCHROMIUM(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM);
    }
    context_->bindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, 0);
  }

  FinishRequest(request, result);
}

void GLHelper::CopyTextureToImpl::FinishRequest(Request* request, bool result) {
  DCHECK(request_queue_.front() == request);
  request_queue_.pop();
  request->callback.Run(result);
  ScopedFlush flush(context_);
  if (request->texture != 0) {
    context_->deleteTexture(request->texture);
    request->texture = 0;
  }
  if (request->buffer != 0) {
    context_->deleteBuffer(request->buffer);
    request->buffer = 0;
  }
  delete request;
}

void GLHelper::CopyTextureToImpl::CancelRequests() {
  while (!request_queue_.empty()) {
    Request* request = request_queue_.front();
    FinishRequest(request, false);
  }
}

GLHelper::GLHelper(WebGraphicsContext3DCommandBufferImpl* context)
    : context_(context) {
}

GLHelper::~GLHelper() {
}

WebGraphicsContext3DCommandBufferImpl* GLHelper::context() const {
  return context_;
}

void GLHelper::CropScaleReadbackAndCleanTexture(
    WebGLId src_texture,
    const gfx::Size& src_size,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    unsigned char* out,
    const base::Callback<void(bool)>& callback) {
  InitCopyTextToImpl();
  copy_texture_to_impl_->CropScaleReadbackAndCleanTexture(src_texture,
                                                          src_size,
                                                          src_subrect,
                                                          dst_size,
                                                          out,
                                                          callback);
}

void GLHelper::ReadbackTextureSync(WebKit::WebGLId texture,
                                   const gfx::Rect& src_rect,
                                   unsigned char* out) {
  InitCopyTextToImpl();
  copy_texture_to_impl_->ReadbackTextureSync(texture,
                                             src_rect,
                                             out);
}

WebKit::WebGLId GLHelper::CopyTexture(WebKit::WebGLId texture,
                                      const gfx::Size& size) {
  InitCopyTextToImpl();
  return copy_texture_to_impl_->CopyAndScaleTexture(texture,
                                                    size,
                                                    size,
                                                    false);
}

WebKit::WebGLId GLHelper::CopyAndScaleTexture(WebKit::WebGLId texture,
                                              const gfx::Size& src_size,
                                              const gfx::Size& dst_size,
                                              bool vertically_flip_texture) {
  InitCopyTextToImpl();
  return copy_texture_to_impl_->CopyAndScaleTexture(texture,
                                                    src_size,
                                                    dst_size,
                                                    vertically_flip_texture);
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

void GLHelper::InitCopyTextToImpl() {
  // Lazily initialize |copy_texture_to_impl_|
  if (!copy_texture_to_impl_.get())
    copy_texture_to_impl_.reset(new CopyTextureToImpl(context_, this));
}

void GLHelper::CopySubBufferDamage(WebKit::WebGLId texture,
                                   WebKit::WebGLId previous_texture,
                                   const SkRegion& new_damage,
                                   const SkRegion& old_damage) {
  SkRegion region(old_damage);
  if (region.op(new_damage, SkRegion::kDifference_Op)) {
    ScopedFramebuffer dst_framebuffer(context_, context_->createFramebuffer());
    ScopedFramebufferBinder<GL_FRAMEBUFFER> framebuffer_binder(context_,
                                                               dst_framebuffer);
    ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(context_, texture);
    context_->framebufferTexture2D(GL_FRAMEBUFFER,
                                   GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D,
                                   previous_texture,
                                   0);
    for (SkRegion::Iterator it(region); !it.done(); it.next()) {
      const SkIRect& rect = it.rect();
      context_->copyTexSubImage2D(GL_TEXTURE_2D, 0,
                                  rect.x(), rect.y(),
                                  rect.x(), rect.y(),
                                  rect.width(), rect.height());
    }
    context_->flush();
  }
}

}  // namespace content
