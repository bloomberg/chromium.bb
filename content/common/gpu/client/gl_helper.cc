// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gl_helper.h"

#include <queue>
#include <string>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "cc/resources/sync_point_helper.h"
#include "content/common/gpu/client/gl_helper_scaling.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gl/gl_bindings.h"

using WebKit::WebGLId;
using WebKit::WebGraphicsContext3D;

namespace content {

// Implements GLHelper::CropScaleReadbackAndCleanTexture and encapsulates
// the data needed for it.
class GLHelper::CopyTextureToImpl :
      public base::SupportsWeakPtr<GLHelper::CopyTextureToImpl> {
 public:
  CopyTextureToImpl(WebGraphicsContext3D* context,
                    GLHelper* helper)
      : context_(context),
        helper_(helper),
        flush_(context),
        vertex_attributes_buffer_(context_, context_->createBuffer()) {
  }
  ~CopyTextureToImpl() {
    CancelRequests();
  }

  void CropScaleReadbackAndCleanTexture(
      WebGLId src_texture,
      const gfx::Size& src_size,
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      unsigned char* out,
      const base::Callback<void(bool)>& callback,
      GLHelper::ScalerQuality quality);

  void ReadbackTextureSync(WebGLId texture,
                           const gfx::Rect& src_rect,
                           unsigned char* out);

  WebKit::WebGLId CopyAndScaleTexture(WebGLId texture,
                                      const gfx::Size& src_size,
                                      const gfx::Size& dst_size,
                                      bool vertically_flip_texture,
                                      GLHelper::ScalerQuality quality);

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
                       bool vertically_flip_texture,
                       bool swizzle,
                       GLHelper::ScalerQuality quality);

  void ReadbackDone(Request* request);
  void FinishRequest(Request* request, bool result);
  void CancelRequests();

  // Interleaved array of 2-dimentional vertex positions (x, y) and
  // 2-dimentional texture coordinates (s, t).
  static const WebKit::WGC3Dfloat kVertexAttributes[];

  WebGraphicsContext3D* context_;
  GLHelper* helper_;

  // A scoped flush that will ensure all resource deletions are flushed when
  // this object is destroyed. Must be declared before other Scoped* fields.
  ScopedFlush flush_;

  // The buffer that holds the vertices and the texture coordinates data for
  // drawing a quad.
  ScopedBuffer vertex_attributes_buffer_;

  std::queue<Request*> request_queue_;
};

GLHelper::ScalerInterface* GLHelper::CreateScaler(
    ScalerQuality quality,
    const gfx::Size& src_size,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    bool vertically_flip_texture,
    bool swizzle) {
  InitScalerImpl();
  return scaler_impl_->CreateScaler(quality,
                                    src_size,
                                    src_subrect,
                                    dst_size,
                                    vertically_flip_texture,
                                    swizzle);
}

WebGLId GLHelper::CopyTextureToImpl::ScaleTexture(
    WebGLId src_texture,
    const gfx::Size& src_size,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    bool vertically_flip_texture,
    bool swizzle,
    GLHelper::ScalerQuality quality) {
  scoped_ptr<ScalerInterface> scaler(
      helper_->CreateScaler(quality,
                            src_size,
                            src_subrect,
                            dst_size,
                            vertically_flip_texture,
                            swizzle));

  WebGLId dst_texture = context_->createTexture();
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
  }
  scaler->Scale(src_texture, dst_texture);
  return dst_texture;
}

void GLHelper::CopyTextureToImpl::CropScaleReadbackAndCleanTexture(
    WebGLId src_texture,
    const gfx::Size& src_size,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    unsigned char* out,
    const base::Callback<void(bool)>& callback,
    GLHelper::ScalerQuality quality) {
  WebGLId texture = ScaleTexture(src_texture,
                                 src_size,
                                 src_subrect,
                                 dst_size,
                                 true,
#if (SK_R32_SHIFT == 16) && !SK_B32_SHIFT
                                 true,
#else
                                 false,
#endif
                                 quality);
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
                       GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  context_->bindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, 0);
  cc::SyncPointHelper::SignalSyncPoint(
      context_,
      context_->insertSyncPoint(),
      base::Bind(&CopyTextureToImpl::ReadbackDone, AsWeakPtr(), request));
}

void GLHelper::CopyTextureToImpl::ReadbackTextureSync(
    WebGLId texture,
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
    bool vertically_flip_texture,
    GLHelper::ScalerQuality quality) {
  return ScaleTexture(src_texture,
                      src_size,
                      gfx::Rect(src_size),
                      dst_size,
                      vertically_flip_texture,
                      false,
                      quality);
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

void GLHelper::CopyTextureToImpl::FinishRequest(Request* request,
                                                bool result) {
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

GLHelper::GLHelper(WebKit::WebGraphicsContext3D* context)
    : context_(context) {
}

GLHelper::~GLHelper() {
}

void GLHelper::CropScaleReadbackAndCleanTexture(
    WebGLId src_texture,
    const gfx::Size& src_size,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    unsigned char* out,
    const base::Callback<void(bool)>& callback) {
  InitCopyTextToImpl();
  copy_texture_to_impl_->CropScaleReadbackAndCleanTexture(
      src_texture,
      src_size,
      src_subrect,
      dst_size,
      out,
      callback,
      GLHelper::SCALER_QUALITY_FAST);
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
  return copy_texture_to_impl_->CopyAndScaleTexture(
      texture,
      size,
      size,
      false,
      GLHelper::SCALER_QUALITY_FAST);
}

WebKit::WebGLId GLHelper::CopyAndScaleTexture(
    WebKit::WebGLId texture,
    const gfx::Size& src_size,
    const gfx::Size& dst_size,
    bool vertically_flip_texture,
    ScalerQuality quality) {
  InitCopyTextToImpl();
  return copy_texture_to_impl_->CopyAndScaleTexture(texture,
                                                    src_size,
                                                    dst_size,
                                                    vertically_flip_texture,
                                                    quality);
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
  if (!copy_texture_to_impl_)
    copy_texture_to_impl_.reset(new CopyTextureToImpl(context_, this));
}

void GLHelper::InitScalerImpl() {
  // Lazily initialize |scaler_impl_|
  if (!scaler_impl_)
    scaler_impl_.reset(new GLHelperScaling(context_, this));
}

void GLHelper::CopySubBufferDamage(WebKit::WebGLId texture,
                                   WebKit::WebGLId previous_texture,
                                   const SkRegion& new_damage,
                                   const SkRegion& old_damage) {
  SkRegion region(old_damage);
  if (region.op(new_damage, SkRegion::kDifference_Op)) {
    ScopedFramebuffer dst_framebuffer(context_,
                                      context_->createFramebuffer());
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
