// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_VIDEO_FRAME_FACTORY_IMPL_
#define MEDIA_GPU_ANDROID_VIDEO_FRAME_FACTORY_IMPL_

#include "base/optional.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/ipc/service/gpu_command_buffer_stub.h"
#include "media/base/video_frame.h"
#include "media/gpu/android/codec_image.h"
#include "media/gpu/android/codec_wrapper.h"
#include "media/gpu/android/video_frame_factory.h"
#include "media/gpu/ipc/service/media_gpu_channel_manager.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/surface_texture_gl_owner.h"
#include "ui/gl/gl_bindings.h"

namespace media {
class GpuVideoFrameFactory;

// VideoFrameFactoryImpl creates CodecOutputBuffer backed VideoFrames and tries
// to eagerly render them to their surface to release the buffers back to the
// decoder as soon as possible. It's not thread safe, but it internally posts
// calls to GpuVideoFrameFactory on the gpu thread.
class MEDIA_GPU_EXPORT VideoFrameFactoryImpl : public VideoFrameFactory {
 public:
  VideoFrameFactoryImpl();
  ~VideoFrameFactoryImpl() override;

  void Initialize(scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
                  GetStubCb get_stub_cb,
                  InitCb init_cb) override;
  void CreateVideoFrame(std::unique_ptr<CodecOutputBuffer> output_buffer,
                        scoped_refptr<SurfaceTextureGLOwner> surface_texture,
                        base::TimeDelta timestamp,
                        gfx::Size natural_size,
                        FrameCreatedCb frame_created_cb) override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
  std::unique_ptr<GpuVideoFrameFactory> gpu_video_frame_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameFactoryImpl);
};

// GpuVideoFrameFactory is an implementation detail of VideoFrameFactoryImpl. It
// may be created on any thread but only accessed on the gpu thread thereafter.
class GpuVideoFrameFactory {
 public:
  GpuVideoFrameFactory();
  ~GpuVideoFrameFactory();

  scoped_refptr<SurfaceTextureGLOwner> Initialize(
      VideoFrameFactory::GetStubCb get_stub_cb);
  scoped_refptr<VideoFrame> CreateVideoFrame(
      std::unique_ptr<CodecOutputBuffer> output_buffer,
      scoped_refptr<SurfaceTextureGLOwner> surface_texture,
      base::TimeDelta timestamp,
      gfx::Size natural_size);

 private:
  // Removes |image| from |images_|.
  void OnImageDestructed(CodecImage* image);

  // Tries to render the first renderable image from |images_|.
  void MaybeRenderEarly();

  // Outstanding images that should be considered for early rendering.
  std::vector<CodecImage*> images_;
  base::WeakPtr<gpu::GpuCommandBufferStub> stub_;
  base::WeakPtrFactory<GpuVideoFrameFactory> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuVideoFrameFactory);
};

namespace internal {

// Tries to render CodecImages to their backing surfaces when it's valid to do
// so. This lets us release codec buffers back to their codecs as soon as
// possible so that decoding can progress smoothly.
// Templated on the image type for testing.
template <typename Image>
void MEDIA_GPU_EXPORT MaybeRenderEarly(std::vector<Image*>* image_vector_ptr) {
  auto& images = *image_vector_ptr;
  if (images.empty())
    return;

  // Find the latest image rendered to the front buffer (if any).
  base::Optional<size_t> front_buffer_index;
  for (int i = images.size() - 1; i >= 0; --i) {
    if (images[i]->was_rendered_to_front_buffer()) {
      front_buffer_index = i;
      break;
    }
  }

  // If there's no image in the front buffer we can safely render one.
  if (!front_buffer_index) {
    // Iterate until we successfully render one to skip over invalidated images.
    for (size_t i = 0; i < images.size(); ++i) {
      if (images[i]->RenderToFrontBuffer()) {
        front_buffer_index = i;
        break;
      }
    }
    // If we couldn't render anything there's nothing more to do.
    if (!front_buffer_index)
      return;
  }

  // Try to render the image following the front buffer to the back buffer.
  size_t back_buffer_index = *front_buffer_index + 1;
  if (back_buffer_index < images.size() &&
      images[back_buffer_index]->is_surface_texture_backed()) {
    images[back_buffer_index]->RenderToSurfaceTextureBackBuffer();
  }
}

}  // namespace internal

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_VIDEO_FRAME_FACTORY_IMPL_
