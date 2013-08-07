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
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "cc/resources/sync_point_helper.h"
#include "content/common/gpu/client/gl_helper_scaling.h"
#include "gpu/command_buffer/common/mailbox.h"
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

// Helper class for allocating and holding an RGBA texture of a given
// size and an associated framebuffer.
class TextureFrameBufferPair {
 public:
  TextureFrameBufferPair(WebGraphicsContext3D* context,
                         gfx::Size size)
      : texture_(context, context->createTexture()),
        framebuffer_(context, context->createFramebuffer()),
        size_(size) {
    content::ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(context,
                                                               texture_);
    context->texImage2D(GL_TEXTURE_2D,
                        0,
                        GL_RGBA,
                        size.width(),
                        size.height(),
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

  WebGLId texture() const { return texture_.id(); }
  WebGLId framebuffer() const { return framebuffer_.id(); }
  gfx::Size size() const { return size_; }

 private:
  content::ScopedTexture texture_;
  content::ScopedFramebuffer framebuffer_;
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(TextureFrameBufferPair);
};

// Helper class for holding a scaler, a texture for the output of that
// scaler and an associated frame buffer. This is inteded to be used
// when the output of a scaler is to be sent to a readback.
class ScalerHolder {
 public:
  ScalerHolder(WebGraphicsContext3D* context,
               content::GLHelper::ScalerInterface *scaler)
      : texture_and_framebuffer_(context, scaler->DstSize()),
        scaler_(scaler) {
  }

  void Scale(WebKit::WebGLId src_texture) {
    scaler_->Scale(src_texture, texture_and_framebuffer_.texture());
  }

  content::GLHelper::ScalerInterface* scaler() const { return scaler_.get(); }
  TextureFrameBufferPair *texture_and_framebuffer() {
    return &texture_and_framebuffer_;
  }
  WebGLId texture() const { return texture_and_framebuffer_.texture(); }

 private:
  TextureFrameBufferPair texture_and_framebuffer_;
  scoped_ptr<content::GLHelper::ScalerInterface> scaler_;

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
        max_draw_buffers_(0) {
    std::string extensions_string = " " +
        UTF16ToASCII(context_->getString(GL_EXTENSIONS)) + " ";
    if (extensions_string.find(" GL_EXT_draw_buffers ") != std::string::npos) {
      context_->getIntegerv(GL_MAX_DRAW_BUFFERS, &max_draw_buffers_);
    }
  }
  ~CopyTextureToImpl() {
    CancelRequests();
  }

  WebGLId ConsumeMailboxToTexture(const gpu::Mailbox& mailbox,
                                  uint32 sync_point) {
    return helper_->ConsumeMailboxToTexture(mailbox, sync_point);
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

  void ReadbackPlane(TextureFrameBufferPair* source,
                     const scoped_refptr<media::VideoFrame>& target,
                     int plane,
                     int size_shift,
                     const gfx::Rect& dst_subrect,
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
      bool flip_vertically,
      bool use_mrt);

  // Returns the maximum number of draw buffers available,
  // 0 if GL_EXT_draw_buffers is not available.
  WebKit::WGC3Dint MaxDrawBuffers() const {
    return max_draw_buffers_;
  }

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
        : done(false),
          size(size_),
          bytes_per_row(bytes_per_row_),
          row_stride_bytes(row_stride_bytes_),
          pixels(pixels_),
          callback(callback_),
          buffer(0),
          query(0) {
    }

    bool done;
    gfx::Size size;
    int bytes_per_row;
    int row_stride_bytes;
    unsigned char* pixels;
    base::Callback<void(bool)> callback;
    GLuint buffer;
    WebKit::WebGLId query;
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
        const gpu::Mailbox& mailbox,
        uint32 sync_point,
        const scoped_refptr<media::VideoFrame>& target,
        const base::Callback<void(bool)>& callback) OVERRIDE;

    virtual ScalerInterface* scaler() OVERRIDE {
      return scaler_.scaler();
    }

   private:
    WebGraphicsContext3D* context_;
    CopyTextureToImpl* copy_impl_;
    gfx::Size dst_size_;
    gfx::Rect dst_subrect_;
    ScalerHolder scaler_;
    ScalerHolder y_;
    ScalerHolder u_;
    ScalerHolder v_;

    DISALLOW_COPY_AND_ASSIGN(ReadbackYUVImpl);
  };

  // A readback pipeline that also converts the data to YUV before
  // reading it back. This one uses Multiple Render Targets, which
  // may not be supported on all platforms.
  class ReadbackYUV_MRT : public ReadbackYUVInterface {
   public:
    ReadbackYUV_MRT(WebGraphicsContext3D* context,
                    CopyTextureToImpl* copy_impl,
                    GLHelperScaling* scaler_impl,
                    GLHelper::ScalerQuality quality,
                    const gfx::Size& src_size,
                    const gfx::Rect& src_subrect,
                    const gfx::Size& dst_size,
                    const gfx::Rect& dst_subrect,
                    bool flip_vertically);

    virtual void ReadbackYUV(
        const gpu::Mailbox& mailbox,
        uint32 sync_point,
        const scoped_refptr<media::VideoFrame>& target,
        const base::Callback<void(bool)>& callback) OVERRIDE;

    virtual ScalerInterface* scaler() OVERRIDE {
      return scaler_.scaler();
    }

   private:
    WebGraphicsContext3D* context_;
    CopyTextureToImpl* copy_impl_;
    gfx::Size dst_size_;
    gfx::Rect dst_subrect_;
    ScalerHolder scaler_;
    scoped_ptr<content::GLHelperScaling::ShaderInterface> pass1_shader_;
    scoped_ptr<content::GLHelperScaling::ShaderInterface> pass2_shader_;
    TextureFrameBufferPair y_;
    ScopedTexture uv_;
    TextureFrameBufferPair u_;
    TextureFrameBufferPair v_;

    DISALLOW_COPY_AND_ASSIGN(ReadbackYUV_MRT);
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
  void ReadbackDone(Request *request);
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

  std::queue<Request*> request_queue_;
  WebKit::WGC3Dint max_draw_buffers_;
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

  request->query = context_->createQueryEXT();
  context_->beginQueryEXT(GL_ASYNC_READ_PIXELS_COMPLETED_CHROMIUM,
                          request->query);
  context_->readPixels(0, 0, dst_size.width(), dst_size.height(),
                       GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  context_->endQueryEXT(GL_ASYNC_READ_PIXELS_COMPLETED_CHROMIUM);
  context_->bindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, 0);
  cc::SyncPointHelper::SignalQuery(
      context_,
      request->query,
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

void GLHelper::CopyTextureToImpl::ReadbackDone(Request* finished_request) {
  TRACE_EVENT0("mirror",
               "GLHelper::CopyTextureToImpl::CheckReadbackFramebufferComplete");
  finished_request->done = true;

  // We process transfer requests in the order they were received, regardless
  // of the order we get the callbacks in.
  while (!request_queue_.empty()) {
    Request* request = request_queue_.front();
    if (!request->done) {
      break;
    }

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
}

void GLHelper::CopyTextureToImpl::FinishRequest(Request* request,
                                                bool result) {
  TRACE_EVENT0("mirror", "GLHelper::CopyTextureToImpl::FinishRequest");
  DCHECK(request_queue_.front() == request);
  request_queue_.pop();
  request->callback.Run(result);
  ScopedFlush flush(context_);
  if (request->query != 0) {
    context_->deleteQueryEXT(request->query);
    request->query = 0;
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

void GLHelper::CropScaleReadbackAndCleanMailbox(
    const gpu::Mailbox& src_mailbox,
    uint32 sync_point,
    const gfx::Size& src_size,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    unsigned char* out,
    const base::Callback<void(bool)>& callback) {
  WebGLId mailbox_texture = ConsumeMailboxToTexture(src_mailbox, sync_point);
  CropScaleReadbackAndCleanTexture(
      mailbox_texture, src_size, src_subrect, dst_size, out, callback);
  context_->deleteTexture(mailbox_texture);
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

WebKit::WGC3Dint GLHelper::MaxDrawBuffers() {
  InitCopyTextToImpl();
  return copy_texture_to_impl_->MaxDrawBuffers();
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

WebKit::WebGLId GLHelper::CreateTexture() {
  WebKit::WebGLId texture = context_->createTexture();
  content::ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(context_,
                                                             texture);
  context_->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  context_->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  context_->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  context_->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return texture;
}

WebKit::WebGLId GLHelper::ConsumeMailboxToTexture(const gpu::Mailbox& mailbox,
                                                  uint32 sync_point) {
  if (mailbox.IsZero())
    return 0;
  if (sync_point)
    context_->waitSyncPoint(sync_point);
  WebKit::WebGLId texture = CreateTexture();
  content::ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(context_,
                                                             texture);
  context_->consumeTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
  return texture;
}

void GLHelper::ResizeTexture(WebKit::WebGLId texture, const gfx::Size& size) {
  content::ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(context_, texture);
  context_->texImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                       size.width(), size.height(), 0,
                       GL_RGB, GL_UNSIGNED_BYTE, NULL);
}

void GLHelper::CopyTextureSubImage(WebKit::WebGLId texture,
                                   const gfx::Rect& rect) {
  content::ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(context_, texture);
  context_->copyTexSubImage2D(GL_TEXTURE_2D, 0,
                              rect.x(), rect.y(),
                              rect.x(), rect.y(), rect.width(), rect.height());
}

void GLHelper::CopyTextureFullImage(WebKit::WebGLId texture,
                                    const gfx::Size& size) {
  content::ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(context_, texture);
  context_->copyTexImage2D(GL_TEXTURE_2D, 0,
                           GL_RGB,
                           0, 0,
                           size.width(), size.height(), 0);
}

void GLHelper::CopyTextureToImpl::ReadbackPlane(
    TextureFrameBufferPair* source,
    const scoped_refptr<media::VideoFrame>& target,
    int plane,
    int size_shift,
    const gfx::Rect& dst_subrect,
    const base::Callback<void(bool)>& callback) {
  context_->bindFramebuffer(GL_FRAMEBUFFER, source->framebuffer());
  size_t offset = target->stride(plane) * (dst_subrect.y() >> size_shift) +
      (dst_subrect.x() >> size_shift);
  ReadbackAsync(
      source->size(),
      dst_subrect.width() >> size_shift,
      target->stride(plane),
      target->data(plane) + offset,
      callback);
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

static void CallbackKeepingVideoFrameAlive(
    scoped_refptr<media::VideoFrame> video_frame,
    const base::Callback<void(bool)>& callback,
    bool success) {
  callback.Run(success);
}

void GLHelper::CopyTextureToImpl::ReadbackYUVImpl::ReadbackYUV(
    const gpu::Mailbox& mailbox,
    uint32 sync_point,
    const scoped_refptr<media::VideoFrame>& target,
    const base::Callback<void(bool)>& callback) {
  WebGLId mailbox_texture =
      copy_impl_->ConsumeMailboxToTexture(mailbox, sync_point);

  // Scale texture to right size.
  scaler_.Scale(mailbox_texture);
  context_->deleteTexture(mailbox_texture);

  // Convert the scaled texture in to Y, U and V planes.
  y_.Scale(scaler_.texture());
  u_.Scale(scaler_.texture());
  v_.Scale(scaler_.texture());

  if (target->coded_size() != dst_size_) {
    DCHECK(target->coded_size() == dst_size_);
    LOG(ERROR) << "ReadbackYUV size error!";
    callback.Run(false);
    return;
  }

  // Read back planes, one at a time. Keep the video frame alive while doing the
  // readback.
  copy_impl_->ReadbackPlane(y_.texture_and_framebuffer(),
                            target,
                            media::VideoFrame::kYPlane,
                            0,
                            dst_subrect_,
                            base::Bind(&nullcallback));
  copy_impl_->ReadbackPlane(u_.texture_and_framebuffer(),
                            target,
                            media::VideoFrame::kUPlane,
                            1,
                            dst_subrect_,
                            base::Bind(&nullcallback));
  copy_impl_->ReadbackPlane(v_.texture_and_framebuffer(),
                            target,
                            media::VideoFrame::kVPlane,
                            1,
                            dst_subrect_,
                            base::Bind(&CallbackKeepingVideoFrameAlive,
                                       target,
                                       callback));
  context_->bindFramebuffer(GL_FRAMEBUFFER, 0);
  media::LetterboxYUV(target, dst_subrect_);
}

// YUV readback constructors. Initiates the main scaler pipeline and
// one planar scaler for each of the Y, U and V planes.
GLHelper::CopyTextureToImpl::ReadbackYUV_MRT::ReadbackYUV_MRT(
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
      pass1_shader_(scaler_impl->CreateYuvMrtShader(
          dst_subrect.size(),
          gfx::Rect(0, 0,
                    (dst_subrect.width() + 3) & ~3,
                    dst_subrect.height()),
          gfx::Size((dst_subrect.width() + 3) / 4,
                    dst_subrect.height()),
          false,
          GLHelperScaling::SHADER_YUV_MRT_PASS1)),
      pass2_shader_(scaler_impl->CreateYuvMrtShader(
          gfx::Size((dst_subrect.width() + 3) / 4,
                    dst_subrect.height()),
          gfx::Rect(0, 0,
                    (dst_subrect.width() + 7) / 8 * 2,
                    dst_subrect.height()),
          gfx::Size((dst_subrect.width() + 7) / 8,
                    (dst_subrect.height() + 1) / 2),
          false,
          GLHelperScaling::SHADER_YUV_MRT_PASS2)),
      y_(context, gfx::Size((dst_subrect.width() + 3) / 4,
                            dst_subrect.height())),
      uv_(context, context->createTexture()),
      u_(context, gfx::Size((dst_subrect.width() + 7) / 8,
                            (dst_subrect.height() + 1) / 2)),
      v_(context, gfx::Size((dst_subrect.width() + 7) / 8,
                            (dst_subrect.height() + 1) / 2)) {

  content::ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(context, uv_);
  context->texImage2D(GL_TEXTURE_2D,
                      0,
                      GL_RGBA,
                      (dst_subrect.width() + 3) / 4,
                      dst_subrect.height(),
                      0,
                      GL_RGBA,
                      GL_UNSIGNED_BYTE,
                      NULL);

  DCHECK(!(dst_size.width() & 1));
  DCHECK(!(dst_size.height() & 1));
  DCHECK(!(dst_subrect.width() & 1));
  DCHECK(!(dst_subrect.height() & 1));
  DCHECK(!(dst_subrect.x() & 1));
  DCHECK(!(dst_subrect.y() & 1));
}

void GLHelper::CopyTextureToImpl::ReadbackYUV_MRT::ReadbackYUV(
    const gpu::Mailbox& mailbox,
    uint32 sync_point,
    const scoped_refptr<media::VideoFrame>& target,
    const base::Callback<void(bool)>& callback) {
  WebGLId mailbox_texture =
      copy_impl_->ConsumeMailboxToTexture(mailbox, sync_point);

  // Scale texture to right size.
  scaler_.Scale(mailbox_texture);
  context_->deleteTexture(mailbox_texture);

  std::vector<WebKit::WebGLId> outputs(2);
  // Convert the scaled texture in to Y, U and V planes.
  outputs[0] = y_.texture();
  outputs[1] = uv_;
  pass1_shader_->Execute(scaler_.texture(), outputs);
  outputs[0] = u_.texture();
  outputs[1] = v_.texture();
  pass2_shader_->Execute(uv_, outputs);

  if (target->coded_size() != dst_size_) {
    DCHECK(target->coded_size() == dst_size_);
    LOG(ERROR) << "ReadbackYUV size error!";
    callback.Run(false);
    return;
  }

  // Read back planes, one at a time.
  copy_impl_->ReadbackPlane(&y_,
                            target,
                            media::VideoFrame::kYPlane,
                            0,
                            dst_subrect_,
                            base::Bind(&nullcallback));
  copy_impl_->ReadbackPlane(&u_,
                            target,
                            media::VideoFrame::kUPlane,
                            1,
                            dst_subrect_,
                            base::Bind(&nullcallback));
  copy_impl_->ReadbackPlane(&v_,
                            target,
                            media::VideoFrame::kVPlane,
                            1,
                            dst_subrect_,
                            base::Bind(&CallbackKeepingVideoFrameAlive,
                                       target,
                                       callback));
  context_->bindFramebuffer(GL_FRAMEBUFFER, 0);
  media::LetterboxYUV(target, dst_subrect_);
}

ReadbackYUVInterface*
GLHelper::CopyTextureToImpl::CreateReadbackPipelineYUV(
    GLHelper::ScalerQuality quality,
    const gfx::Size& src_size,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    const gfx::Rect& dst_subrect,
    bool flip_vertically,
    bool use_mrt) {
  helper_->InitScalerImpl();
  if (max_draw_buffers_ >= 2 && use_mrt) {
    return new ReadbackYUV_MRT(
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
    bool flip_vertically,
    bool use_mrt) {
  InitCopyTextToImpl();
  return copy_texture_to_impl_->CreateReadbackPipelineYUV(
      quality,
      src_size,
      src_subrect,
      dst_size,
      dst_subrect,
      flip_vertically,
      use_mrt);
}

}  // namespace content
