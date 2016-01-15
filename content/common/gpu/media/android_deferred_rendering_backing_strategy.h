// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_ANDROID_DEFERRED_RENDERING_BACKING_STRATEGY_H_
#define CONTENT_COMMON_GPU_MEDIA_ANDROID_DEFERRED_RENDERING_BACKING_STRATEGY_H_

#include <stdint.h>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/common/gpu/media/android_video_decode_accelerator.h"

namespace gl {
class GLImage;
}

namespace gpu {
namespace gles2 {
class TextureRef;
}
}

namespace content {

class AVDACodecImage;
class AVDASharedState;

// A BackingStrategy implementation that defers releasing codec buffers until
// a PictureBuffer's texture is used to draw, then draws using the surface
// texture's front buffer rather than a copy.  To do this, it uses a GLImage
// implementation to talk to MediaCodec.
class CONTENT_EXPORT AndroidDeferredRenderingBackingStrategy
    : public AndroidVideoDecodeAccelerator::BackingStrategy {
 public:
  AndroidDeferredRenderingBackingStrategy();
  ~AndroidDeferredRenderingBackingStrategy() override;

  // AndroidVideoDecodeAccelerator::BackingStrategy
  void Initialize(AVDAStateProvider*) override;
  void Cleanup(bool have_context,
               const AndroidVideoDecodeAccelerator::OutputBufferMap&) override;
  uint32_t GetTextureTarget() const override;
  scoped_refptr<gfx::SurfaceTexture> CreateSurfaceTexture() override;
  void UseCodecBufferForPictureBuffer(int32_t codec_buffer_index,
                                      const media::PictureBuffer&) override;
  void AssignOnePictureBuffer(const media::PictureBuffer&) override;
  void ReuseOnePictureBuffer(const media::PictureBuffer&) override;
  void DismissOnePictureBuffer(const media::PictureBuffer&) override;
  void CodecChanged(
      media::VideoCodecBridge*,
      const AndroidVideoDecodeAccelerator::OutputBufferMap&) override;
  void OnFrameAvailable() override;

 private:
  // Release any codec buffer that is associated with the given picture buffer
  // back to the codec.  It is okay if there is no such buffer.
  void ReleaseCodecBufferForPicture(const media::PictureBuffer& picture_buffer);

  // Return the TextureRef for a given PictureBuffer's texture.
  gpu::gles2::TextureRef* GetTextureForPicture(const media::PictureBuffer&);

  // Return the AVDACodecImage for a given PictureBuffer's texture.
  AVDACodecImage* GetImageForPicture(const media::PictureBuffer&);
  void SetImageForPicture(const media::PictureBuffer& picture_buffer,
                          const scoped_refptr<gl::GLImage>& image);

  scoped_refptr<AVDASharedState> shared_state_;

  AVDAStateProvider* state_provider_;

  scoped_refptr<gfx::SurfaceTexture> surface_texture_;

  media::VideoCodecBridge* media_codec_;

  DISALLOW_COPY_AND_ASSIGN(AndroidDeferredRenderingBackingStrategy);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_ANDROID_DEFERRED_RENDERING_BACKING_STRATEGY_H_
