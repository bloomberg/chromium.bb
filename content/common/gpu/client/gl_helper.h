// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GL_HELPER_H_
#define CONTENT_COMMON_GPU_CLIENT_GL_HELPER_H_

#include "base/atomicops.h"
#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"

namespace gfx {
class Rect;
class Size;
}

namespace gpu {
class ContextSupport;
struct Mailbox;
}

namespace media {
class VideoFrame;
};

class SkRegion;

namespace content {

class GLHelperScaling;

class ScopedWebGLId {
 public:
  typedef void (blink::WebGraphicsContext3D::*DeleteFunc)(WebGLId);
  ScopedWebGLId(blink::WebGraphicsContext3D* context,
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

  ~ScopedWebGLId() {
    if (id_ != 0) {
      (context_->*delete_func_)(id_);
    }
  }

 private:
  blink::WebGraphicsContext3D* context_;
  WebGLId id_;
  DeleteFunc delete_func_;

  DISALLOW_COPY_AND_ASSIGN(ScopedWebGLId);
};

class ScopedBuffer : public ScopedWebGLId {
 public:
  ScopedBuffer(blink::WebGraphicsContext3D* context,
               WebGLId id)
      : ScopedWebGLId(context,
                      id,
                      &blink::WebGraphicsContext3D::deleteBuffer) {}
};

class ScopedFramebuffer : public ScopedWebGLId {
 public:
  ScopedFramebuffer(blink::WebGraphicsContext3D* context,
                    WebGLId id)
      : ScopedWebGLId(context,
                      id,
                      &blink::WebGraphicsContext3D::deleteFramebuffer) {}
};

class ScopedProgram : public ScopedWebGLId {
 public:
  ScopedProgram(blink::WebGraphicsContext3D* context,
                WebGLId id)
      : ScopedWebGLId(context,
                      id,
                      &blink::WebGraphicsContext3D::deleteProgram) {}
};

class ScopedShader : public ScopedWebGLId {
 public:
  ScopedShader(blink::WebGraphicsContext3D* context,
               WebGLId id)
      : ScopedWebGLId(context,
                      id,
                      &blink::WebGraphicsContext3D::deleteShader) {}
};

class ScopedTexture : public ScopedWebGLId {
 public:
  ScopedTexture(blink::WebGraphicsContext3D* context,
                WebGLId id)
      : ScopedWebGLId(context,
                      id,
                      &blink::WebGraphicsContext3D::deleteTexture) {}
};

template <blink::WGC3Denum target>
class ScopedBinder {
 public:
  typedef void (blink::WebGraphicsContext3D::*BindFunc)(blink::WGC3Denum,
                                                         WebGLId);
  ScopedBinder(blink::WebGraphicsContext3D* context,
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
  blink::WebGraphicsContext3D* context_;
  BindFunc bind_func_;

  DISALLOW_COPY_AND_ASSIGN(ScopedBinder);
};

template <blink::WGC3Denum target>
class ScopedBufferBinder : ScopedBinder<target> {
 public:
  ScopedBufferBinder(blink::WebGraphicsContext3D* context,
                     WebGLId id)
      : ScopedBinder<target>(
          context,
          id,
          &blink::WebGraphicsContext3D::bindBuffer) {}
};

template <blink::WGC3Denum target>
class ScopedFramebufferBinder : ScopedBinder<target> {
 public:
  ScopedFramebufferBinder(blink::WebGraphicsContext3D* context,
                          WebGLId id)
      : ScopedBinder<target>(
          context,
          id,
          &blink::WebGraphicsContext3D::bindFramebuffer) {}
};

template <blink::WGC3Denum target>
class ScopedTextureBinder : ScopedBinder<target> {
 public:
  ScopedTextureBinder(blink::WebGraphicsContext3D* context,
                      WebGLId id)
      : ScopedBinder<target>(
          context,
          id,
          &blink::WebGraphicsContext3D::bindTexture) {}
};

class ScopedFlush {
 public:
  explicit ScopedFlush(blink::WebGraphicsContext3D* context)
      : context_(context) {
  }

  ~ScopedFlush() {
    context_->flush();
  }

 private:
  blink::WebGraphicsContext3D* context_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFlush);
};


class ReadbackYUVInterface;

// Provides higher level operations on top of the blink::WebGraphicsContext3D
// interfaces.
class CONTENT_EXPORT GLHelper {
 public:
  GLHelper(blink::WebGraphicsContext3D* context,
           gpu::ContextSupport* context_support);
  ~GLHelper();

  enum ScalerQuality {
    // Bilinear single pass, fastest possible.
    SCALER_QUALITY_FAST = 1,

    // Bilinear upscale + N * 50% bilinear downscales.
    // This is still fast enough for most purposes and
    // Image quality is nearly as good as the BEST option.
    SCALER_QUALITY_GOOD = 2,

    // Bicubic upscale + N * 50% bicubic downscales.
    // Produces very good quality scaled images, but it's
    // 2-8x slower than the "GOOD" quality, so it's not always
    // worth it.
    SCALER_QUALITY_BEST = 3,
  };


  // Copies the block of pixels specified with |src_subrect| from |src_texture|,
  // scales it to |dst_size|, and writes it into |out|.
  // |src_size| is the size of |src_texture|. The result is of format GL_BGRA
  // and is potentially flipped vertically to make it a correct image
  // representation.  |callback| is invoked with the copy result when the copy
  // operation has completed.
  // Note that the src_texture will have the min/mag filter set to GL_LINEAR
  // and wrap_s/t set to CLAMP_TO_EDGE in this call.
  void CropScaleReadbackAndCleanTexture(
      blink::WebGLId src_texture,
      const gfx::Size& src_size,
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      unsigned char* out,
      const base::Callback<void(bool)>& callback);

  // Copies the block of pixels specified with |src_subrect| from |src_mailbox|,
  // scales it to |dst_size|, and writes it into |out|.
  // |src_size| is the size of |src_mailbox|. The result is of format GL_BGRA
  // and is potentially flipped vertically to make it a correct image
  // representation.  |callback| is invoked with the copy result when the copy
  // operation has completed.
  // Note that the texture bound to src_mailbox will have the min/mag filter set
  // to GL_LINEAR and wrap_s/t set to CLAMP_TO_EDGE in this call. src_mailbox is
  // assumed to be GL_TEXTURE_2D.
  void CropScaleReadbackAndCleanMailbox(
      const gpu::Mailbox& src_mailbox,
      uint32 sync_point,
      const gfx::Size& src_size,
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      unsigned char* out,
      const base::Callback<void(bool)>& callback);

  // Copies the texture data out of |texture| into |out|.  |size| is the
  // size of the texture.  No post processing is applied to the pixels.  The
  // texture is assumed to have a format of GL_RGBA with a pixel type of
  // GL_UNSIGNED_BYTE.  This is a blocking call that calls glReadPixels on this
  // current context.
  void ReadbackTextureSync(blink::WebGLId texture,
                           const gfx::Rect& src_rect,
                           unsigned char* out);

  // Creates a copy of the specified texture. |size| is the size of the texture.
  // Note that the src_texture will have the min/mag filter set to GL_LINEAR
  // and wrap_s/t set to CLAMP_TO_EDGE in this call.
  blink::WebGLId CopyTexture(blink::WebGLId texture,
                              const gfx::Size& size);

  // Creates a scaled copy of the specified texture. |src_size| is the size of
  // the texture and |dst_size| is the size of the resulting copy.
  // Note that the src_texture will have the min/mag filter set to GL_LINEAR
  // and wrap_s/t set to CLAMP_TO_EDGE in this call.
  blink::WebGLId CopyAndScaleTexture(
      blink::WebGLId texture,
      const gfx::Size& src_size,
      const gfx::Size& dst_size,
      bool vertically_flip_texture,
      ScalerQuality quality);

  // Returns the shader compiled from the source.
  blink::WebGLId CompileShaderFromSource(const blink::WGC3Dchar* source,
                                          blink::WGC3Denum type);

  // Copies all pixels from |previous_texture| into |texture| that are
  // inside the region covered by |old_damage| but not part of |new_damage|.
  void CopySubBufferDamage(blink::WebGLId texture,
                           blink::WebGLId previous_texture,
                           const SkRegion& new_damage,
                           const SkRegion& old_damage);

  // Simply creates a texture.
  blink::WebGLId CreateTexture();
  // Deletes a texture.
  void DeleteTexture(blink::WebGLId texture_id);

  // Insert a sync point into the GL command buffer.
  uint32 InsertSyncPoint();
  // Wait for the sync point before executing further GL commands.
  void WaitSyncPoint(uint32 sync_point);

  // Creates a mailbox that is attached to the given texture id, and a sync
  // point to wait on before using the mailbox. Returns an empty mailbox on
  // failure.
  // Note the texture is assumed to be GL_TEXTURE_2D.
  gpu::Mailbox ProduceMailboxFromTexture(blink::WebGLId texture_id,
                                         uint32* sync_point);

  // Creates a texture and consumes a mailbox into it. Returns 0 on failure.
  // Note the mailbox is assumed to be GL_TEXTURE_2D.
  blink::WebGLId ConsumeMailboxToTexture(const gpu::Mailbox& mailbox,
                                          uint32 sync_point);

  // Resizes the texture's size to |size|.
  void ResizeTexture(blink::WebGLId texture, const gfx::Size& size);

  // Copies the framebuffer data given in |rect| to |texture|.
  void CopyTextureSubImage(blink::WebGLId texture, const gfx::Rect& rect);

  // Copies the all framebuffer data to |texture|. |size| specifies the
  // size of the framebuffer.
  void CopyTextureFullImage(blink::WebGLId texture, const gfx::Size& size);

  // A scaler will cache all intermediate textures and programs
  // needed to scale from a specified size to a destination size.
  // If the source or destination sizes changes, you must create
  // a new scaler.
  class CONTENT_EXPORT ScalerInterface {
   public:
    ScalerInterface() {}
    virtual ~ScalerInterface() {}

    // Note that the src_texture will have the min/mag filter set to GL_LINEAR
    // and wrap_s/t set to CLAMP_TO_EDGE in this call.
    virtual void Scale(blink::WebGLId source_texture,
                       blink::WebGLId dest_texture) = 0;
    virtual const gfx::Size& SrcSize() = 0;
    virtual const gfx::Rect& SrcSubrect() = 0;
    virtual const gfx::Size& DstSize() = 0;
  };

  // Note that the quality may be adjusted down if texture
  // allocations fail or hardware doesn't support the requtested
  // quality. Note that ScalerQuality enum is arranged in
  // numerical order for simplicity.
  ScalerInterface* CreateScaler(ScalerQuality quality,
                                const gfx::Size& src_size,
                                const gfx::Rect& src_subrect,
                                const gfx::Size& dst_size,
                                bool vertically_flip_texture,
                                bool swizzle);

  // Create a readback pipeline that will scale a subsection of the source
  // texture, then convert it to YUV422 planar form and then read back that.
  // This reduces the amount of memory read from GPU to CPU memory by a factor
  // 2.6, which can be quite handy since readbacks have very limited speed
  // on some platforms. All values in |dst_size| and |dst_subrect| must be
  // a multiple of two. If |use_mrt| is true, the pipeline will try to optimize
  // the YUV conversion using the multi-render-target extension. |use_mrt|
  // should only be set to false for testing.
  ReadbackYUVInterface* CreateReadbackPipelineYUV(
      ScalerQuality quality,
      const gfx::Size& src_size,
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      const gfx::Rect& dst_subrect,
      bool flip_vertically,
      bool use_mrt);

  // Returns the maximum number of draw buffers available,
  // 0 if GL_EXT_draw_buffers is not available.
  blink::WGC3Dint MaxDrawBuffers();

 protected:
  class CopyTextureToImpl;

  // Creates |copy_texture_to_impl_| if NULL.
  void InitCopyTextToImpl();
  // Creates |scaler_impl_| if NULL.
  void InitScalerImpl();

  blink::WebGraphicsContext3D* context_;
  gpu::ContextSupport* context_support_;
  scoped_ptr<CopyTextureToImpl> copy_texture_to_impl_;
  scoped_ptr<GLHelperScaling> scaler_impl_;

  DISALLOW_COPY_AND_ASSIGN(GLHelper);
};

// Similar to a ScalerInterface, a yuv readback pipeline will
// cache a scaler and all intermediate textures and frame buffers
// needed to scale, crop, letterbox and read back a texture from
// the GPU into CPU-accessible RAM. A single readback pipeline
// can handle multiple outstanding readbacks at the same time, but
// if the source or destination sizes change, you'll need to create
// a new readback pipeline.
class CONTENT_EXPORT ReadbackYUVInterface {
public:
  ReadbackYUVInterface() {}
  virtual ~ReadbackYUVInterface() {}

  // Note that |target| must use YV12 format.
  virtual void ReadbackYUV(
      const gpu::Mailbox& mailbox,
      uint32 sync_point,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback) = 0;
  virtual GLHelper::ScalerInterface* scaler() = 0;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GL_HELPER_H_
