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
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gl/gl_bindings.h"

using WebKit::WebGLId;
using WebKit::WebGraphicsContext3D;

namespace {

// Helper class for holding a scaler, a texture for the output of that
// scaler and an associated frame buffer. This is inteded to be used
// when the output of a scaler is t be sent to a readback.
class ScalerHolder {
 public:
  ScalerHolder(WebGraphicsContext3D* context,
               content::GLHelper::ScalerInterface *scaler)
      : scaler_(scaler),
        texture_(context, context->createTexture()),
        framebuffer_(context, context->createFramebuffer()) {
    content::ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(context,
                                                               texture_);
    context->texImage2D(GL_TEXTURE_2D,
                        0,
                        GL_RGBA,
                        scaler_->DstSize().width(),
                        scaler_->DstSize().height(),
                        0,
                        GL_RGBA,
                        GL_UNSIGNED_BYTE,
                        NULL);
    content::ScopedFramebufferBinder<GL_FRAMEBUFFER> framebuffer_binder(
        context,
        framebuffer_);
    context->framebufferTexture2D(GL_FRAMEBUFFER,
                                  GL_COLOR_ATTACHMENT0,
                                  GL_TEXTURE_2D,
                                  texture_,
                                  0);
  }

  void Scale(WebKit::WebGLId src_texture) {
    scaler_->Scale(src_texture, texture_);
  }

  content::GLHelper::ScalerInterface* scaler() const { return scaler_.get(); }
  WebGLId texture() const { return texture_.id(); }
  WebGLId framebuffer() const { return framebuffer_.id(); }

 public:
  scoped_ptr<content::GLHelper::ScalerInterface> scaler_;
  content::ScopedTexture texture_;
  content::ScopedFramebuffer framebuffer_;

  DISALLOW_COPY_AND_ASSIGN(ScalerHolder);
};

}  // namespace

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

  // Reads back bytes from the currently bound frame buffer.
  // Note that dst_size is specified in bytes, not pixels.
  void ReadbackAsync(
      const gfx::Size& dst_size,
      int32 bytes_per_row,     // generally dst_size.width() * 4
      int32 row_stride_bytes,  // generally dst_size.width() * 4
      unsigned char* out,
      const base::Callback<void(bool)>& callback);

  WebKit::WebGLId CopyAndScaleTexture(WebGLId texture,
                                      const gfx::Size& src_size,
                                      const gfx::Size& dst_size,
                                      bool vertically_flip_texture,
                                      GLHelper::ScalerQuality quality);

  ReadbackYUVInterface* CreateReadbackPipelineYUV(
      GLHelper::ScalerQuality quality,
      const gfx::Size& src_size,
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      const gfx::Rect& dst_subrect,
      bool flip_vertically);

 private:
  // A single request to CropScaleReadbackAndCleanTexture.
  // The main thread can cancel the request, before it's handled by the helper
  // thread, by resetting the texture and pixels fields. Alternatively, the
  // thread marks that it handles the request by resetting the pixels field
  // (meaning it guarantees that the callback with be called).
  // In either case, the callback must be called exactly once, and the texture
  // must be deleted by the main thread context.
  struct Request {
    Request(const gfx::Size& size_,
            int32 bytes_per_row_,
            int32 row_stride_bytes_,
            unsigned char* pixels_,
            const base::Callback<void(bool)>& callback_)
        : size(size_),
          bytes_per_row(bytes_per_row_),
          row_stride_bytes(row_stride_bytes_),
          pixels(pixels_),
          callback(callback_),
          buffer(0) {
    }

    gfx::Size size;
    int bytes_per_row;
    int row_stride_bytes;
    unsigned char* pixels;
    base::Callback<void(bool)> callback;
    GLuint buffer;
  };

  // A readback pipeline that also converts the data to YUV before
  // reading it back.
  class ReadbackYUVImpl : public ReadbackYUVInterface {
   public:
    ReadbackYUVImpl(WebGraphicsContext3D* context,
                    CopyTextureToImpl* copy_impl,
                    GLHelperScaling* scaler_impl,
                    GLHelper::ScalerQuality quality,
                    const gfx::Size& src_size,
                    const gfx::Rect& src_subrect,
                    const gfx::Size& dst_size,
                    const gfx::Rect& dst_subrect,
                    bool flip_vertically);

    virtual void ReadbackYUV(
        WebKit::WebGLId src_texture,
        media::VideoFrame* target,
        const base::Callback<void(bool)>& callback) OVERRIDE;

    virtual ScalerInterface* scaler() OVERRIDE {
      return scaler_.scaler();
    }

   private:
    void ReadbackPlane(ScalerHolder* scaler,
                       media::VideoFrame* target,
                       int plane,
                       int size_shift,
                       const base::Callback<void(bool)>& callback);

    WebGraphicsContext3D* context_;
    CopyTextureToImpl* copy_impl_;
    gfx::Size dst_size_;
    gfx::Rect dst_subrect_;
    ScalerHolder scaler_;
    ScalerHolder y_;
    ScalerHolder u_;
    ScalerHolder v_;
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

  static void nullcallback(bool success) {}
  void ReadbackDone(Request* request);
  void FinishRequest(Request* request, bool result);
  void CancelRequests();

  static const float kRGBtoYColorWeights[];
  static const float kRGBtoUColorWeights[];
  static const float kRGBtoVColorWeights[];

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

void GLHelper::CopyTextureToImpl::ReadbackAsync(
    const gfx::Size& dst_size,
    int32 bytes_per_row,
    int32 row_stride_bytes,
    unsigned char* out,
    const base::Callback<void(bool)>& callback) {
  Request* request = new Request(dst_size,
                                 bytes_per_row,
                                 row_stride_bytes,
                                 out,
                                 callback);
  request_queue_.push(request);
  request->buffer = context_->createBuffer();
  context_->bindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM,
                       request->buffer);
  context_->bufferData(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM,
                       4 * dst_size.GetArea(),
                       NULL,
                       GL_STREAM_READ);

  context_->readPixels(0, 0, dst_size.width(), dst_size.height(),
                       GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  context_->bindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, 0);
  cc::SyncPointHelper::SignalSyncPoint(
      context_,
      context_->insertSyncPoint(),
      base::Bind(&CopyTextureToImpl::ReadbackDone, AsWeakPtr(), request));
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
  ScopedFramebuffer dst_framebuffer(context_, context_->createFramebuffer());
  ScopedFramebufferBinder<GL_FRAMEBUFFER> framebuffer_binder(context_,
                                                             dst_framebuffer);
  ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(context_, texture);
  context_->framebufferTexture2D(GL_FRAMEBUFFER,
                                 GL_COLOR_ATTACHMENT0,
                                 GL_TEXTURE_2D,
                                 texture,
                                 0);
  ReadbackAsync(dst_size,
                dst_size.width() * 4,
                dst_size.width() * 4,
                out,
                callback);
  context_->deleteTexture(texture);
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
               "GLHelper::CopyTextureToImpl::CheckReadbackFramebufferComplete");
  DCHECK(request == request_queue_.front());

  bool result = false;
  if (request->buffer != 0) {
    context_->bindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM,
                         request->buffer);
    unsigned char* data = static_cast<unsigned char *>(
        context_->mapBufferCHROMIUM(
            GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, GL_READ_ONLY));
    if (data) {
      result = true;
      if (request->bytes_per_row == request->size.width() * 4 &&
          request->bytes_per_row == request->row_stride_bytes) {
        memcpy(request->pixels, data, request->size.GetArea() * 4);
      } else {
        unsigned char* out = request->pixels;
        for (int y = 0; y < request->size.height(); y++) {
          memcpy(out, data, request->bytes_per_row);
          out += request->row_stride_bytes;
          data += request->size.width() * 4;
        }
      }
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

const float GLHelper::CopyTextureToImpl::kRGBtoYColorWeights[] = {
  0.257f, 0.504f, 0.098f, 0.0625f
};
const float GLHelper::CopyTextureToImpl::kRGBtoUColorWeights[] = {
  -0.148f, -0.291f, 0.439f, 0.5f
};
const float GLHelper::CopyTextureToImpl::kRGBtoVColorWeights[] = {
  0.439f, -0.368f, -0.071f, 0.5f
};

// YUV readback constructors. Initiates the main scaler pipeline and
// one planar scaler for each of the Y, U and V planes.
GLHelper::CopyTextureToImpl::ReadbackYUVImpl::ReadbackYUVImpl(
    WebGraphicsContext3D* context,
    CopyTextureToImpl* copy_impl,
    GLHelperScaling* scaler_impl,
    GLHelper::ScalerQuality quality,
    const gfx::Size& src_size,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    const gfx::Rect& dst_subrect,
    bool flip_vertically)
    : context_(context),
      copy_impl_(copy_impl),
      dst_size_(dst_size),
      dst_subrect_(dst_subrect),
      scaler_(context, scaler_impl->CreateScaler(
          quality,
          src_size,
          src_subrect,
          dst_subrect.size(),
          flip_vertically,
          false)),
      y_(context, scaler_impl->CreatePlanarScaler(
          dst_subrect.size(),
          gfx::Rect(0, 0,
                    (dst_subrect.width() + 3) & ~3,
                    dst_subrect.height()),
          gfx::Size((dst_subrect.width() + 3) / 4,
                    dst_subrect.height()),
          false,
          kRGBtoYColorWeights)),
      u_(context, scaler_impl->CreatePlanarScaler(
          dst_subrect.size(),
          gfx::Rect(0, 0,
                    (dst_subrect.width() + 7) & ~7,
                    (dst_subrect.height() + 1) & ~1),
          gfx::Size((dst_subrect.width() + 7) / 8,
                    (dst_subrect.height() + 1) / 2),
          false,
          kRGBtoUColorWeights)),
      v_(context, scaler_impl->CreatePlanarScaler(
          dst_subrect.size(),
          gfx::Rect(0, 0,
                    (dst_subrect.width() + 7) & ~7,
                    (dst_subrect.height() + 1) & ~1),
          gfx::Size((dst_subrect.width() + 7) / 8,
                    (dst_subrect.height() + 1) / 2),
          false,
          kRGBtoVColorWeights)) {
  DCHECK(!(dst_size.width() & 1));
  DCHECK(!(dst_size.height() & 1));
  DCHECK(!(dst_subrect.width() & 1));
  DCHECK(!(dst_subrect.height() & 1));
  DCHECK(!(dst_subrect.x() & 1));
  DCHECK(!(dst_subrect.y() & 1));
}

void GLHelper::CopyTextureToImpl::ReadbackYUVImpl::ReadbackYUV(
    WebKit::WebGLId src_texture,
    media::VideoFrame *target,
    const base::Callback<void(bool)>& callback) {
  // Scale texture to right size.
  scaler_.Scale(src_texture);

  // Convert the scaled texture in to Y, U and V planes.
  y_.Scale(scaler_.texture_);
  u_.Scale(scaler_.texture_);
  v_.Scale(scaler_.texture_);

  if (target->coded_size() != dst_size_) {
    DCHECK(target->coded_size() == dst_size_);
    LOG(ERROR) << "ReadbackYUV size error!";
    callback.Run(false);
    return;
  }

  // Read back planes, one at a time.
  ReadbackPlane(&y_,
                target,
                media::VideoFrame::kYPlane,
                0,
                base::Bind(&nullcallback));
  ReadbackPlane(&u_,
                target,
                media::VideoFrame::kUPlane,
                1,
                base::Bind(&nullcallback));
  ReadbackPlane(&v_,
                target,
                media::VideoFrame::kVPlane,
                1,
                callback);
  context_->bindFramebuffer(GL_FRAMEBUFFER, 0);
  media::LetterboxYUV(target, dst_subrect_);
}

void GLHelper::CopyTextureToImpl::ReadbackYUVImpl::ReadbackPlane(
    ScalerHolder *scaler,
    media::VideoFrame *target,
    int plane,
    int size_shift,
    const base::Callback<void(bool)>& callback) {
  context_->bindFramebuffer(GL_FRAMEBUFFER, scaler->framebuffer());
  copy_impl_->ReadbackAsync(
      scaler->scaler()->DstSize(),
      dst_subrect_.width() >> size_shift,
      target->stride(plane),
      target->data(plane) +
      target->stride(plane) * (dst_subrect_.y() >> size_shift) +
      (dst_subrect_.x() >> size_shift),
      callback);
}


ReadbackYUVInterface*
GLHelper::CopyTextureToImpl::CreateReadbackPipelineYUV(
    GLHelper::ScalerQuality quality,
    const gfx::Size& src_size,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    const gfx::Rect& dst_subrect,
    bool flip_vertically) {
  helper_->InitScalerImpl();
  return new ReadbackYUVImpl(
      context_,
      this,
      helper_->scaler_impl_.get(),
      quality,
      src_size,
      src_subrect,
      dst_size,
      dst_subrect,
      flip_vertically);
}

ReadbackYUVInterface*
GLHelper::CreateReadbackPipelineYUV(
    ScalerQuality quality,
    const gfx::Size& src_size,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    const gfx::Rect& dst_subrect,
    bool flip_vertically) {
  InitCopyTextToImpl();
  return copy_texture_to_impl_->CreateReadbackPipelineYUV(
      quality,
      src_size,
      src_subrect,
      dst_size,
      dst_subrect,
      flip_vertically);
}

}  // namespace content
