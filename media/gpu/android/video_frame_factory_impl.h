// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_VIDEO_FRAME_FACTORY_IMPL_
#define MEDIA_GPU_ANDROID_VIDEO_FRAME_FACTORY_IMPL_

#include <memory>

#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "gpu/command_buffer/service/abstract_texture.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "media/base/video_frame.h"
#include "media/gpu/android/codec_image.h"
#include "media/gpu/android/codec_wrapper.h"
#include "media/gpu/android/shared_image_video_provider.h"
#include "media/gpu/android/surface_texture_gl_owner.h"
#include "media/gpu/android/video_frame_factory.h"
#include "media/gpu/gles2_decoder_helper.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gl/gl_bindings.h"

namespace media {
class CodecImageGroup;

// VideoFrameFactoryImpl creates CodecOutputBuffer backed VideoFrames and tries
// to eagerly render them to their surface to release the buffers back to the
// decoder as soon as possible. It's not thread safe; it should be created, used
// and destructed on a single sequence. It's implemented by proxying calls
// to a helper class hosted on the gpu thread.
class MEDIA_GPU_EXPORT VideoFrameFactoryImpl : public VideoFrameFactory {
 public:
  // Callback used to return a mailbox and release callback for an image. The
  // release callback may be dropped without being run, and the image will be
  // cleaned up properly. The release callback may be called from any thread.
  using ImageReadyCB =
      base::OnceCallback<void(gpu::Mailbox mailbox,
                              VideoFrame::ReleaseMailboxCB release_cb)>;

  // |get_stub_cb| will be run on |gpu_task_runner|.
  VideoFrameFactoryImpl(
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      GetStubCb get_stub_cb,
      const gpu::GpuPreferences& gpu_preferences);
  ~VideoFrameFactoryImpl() override;

  void Initialize(OverlayMode overlay_mode, InitCb init_cb) override;
  void SetSurfaceBundle(
      scoped_refptr<CodecSurfaceBundle> surface_bundle) override;
  void CreateVideoFrame(
      std::unique_ptr<CodecOutputBuffer> output_buffer,
      base::TimeDelta timestamp,
      gfx::Size natural_size,
      PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb,
      OnceOutputCb output_cb) override;
  void RunAfterPendingVideoFrames(base::OnceClosure closure) override;

 private:
  // ImageReadyCB that will construct a VideoFrame, and forward it to
  // |output_cb| if construction succeeds.  This is static for two reasons.
  // First, we want to snapshot the state of the world when the request is made,
  // in case things like the texture owner change before it's returned.  While
  // it's unclear that MCVD would actually do this (it drains output buffers
  // before switching anything, which guarantees that the VideoFrame has been
  // created and sent to the renderer), it's still much simpler to think about
  // if this uses the same state as the CreateVideoFrame call.
  //
  // Second, this way we don't care about the lifetime of |this|; |output_cb|
  // can worry about it.
  static void OnImageReady(OnceOutputCb output_cb,
                           base::TimeDelta timestamp,
                           gfx::Size coded_size,
                           gfx::Size natural_size,
                           scoped_refptr<TextureOwner> texture_owner,
                           VideoPixelFormat pixel_format,
                           OverlayMode overlay_mode,
                           bool enable_threaded_texture_mailboxes,
                           SharedImageVideoProvider::ImageRecord record);

  std::unique_ptr<SharedImageVideoProvider> image_provider_;
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
  GetStubCb get_stub_cb_;

  // The texture owner that video frames should use, or nullptr.
  scoped_refptr<TextureOwner> texture_owner_;

  OverlayMode overlay_mode_ = OverlayMode::kDontRequestPromotionHints;

  // Is the sync mailbox manager enabled?
  bool enable_threaded_texture_mailboxes_ = false;

  // Current group that new CodecImages should belong to.  Do not use this on
  // our thread; everything must be posted to the gpu main thread, including
  // destruction of it.
  scoped_refptr<CodecImageGroup> image_group_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(VideoFrameFactoryImpl);
};

// GpuSharedImageVideoFactory creates SharedImageVideo objects.  It must be run
// on the gpu main thread.
//
// GpuSharedImageVideoFactory is an implementation detail of
// DirectSharedImageVideoProvider.  It really should be split out into its own
// file from here, but in the interest of making CL diffs more readable, that
// is left for later.
class GpuSharedImageVideoFactory
    : public gpu::CommandBufferStub::DestructionObserver {
 public:
  GpuSharedImageVideoFactory();
  ~GpuSharedImageVideoFactory() override;

  // TODO(liberato): Now that this is used as part of an image provider, it
  // doesn't really make sense for it to call back with a TextureOwner.  That
  // should be handled by VideoFrameFactoryImpl if it wants.
  void Initialize(VideoFrameFactory::OverlayMode overlay_mode,
                  VideoFrameFactory::GetStubCb get_stub_cb,
                  VideoFrameFactory::InitCb init_cb);

  // Similar to SharedImageVideoProvider::ImageReadyCB, but provides additional
  // details for the provider that's using us.
  using FactoryImageReadyCB =
      base::OnceCallback<void(SharedImageVideoProvider::ImageRecord record,
                              scoped_refptr<CodecImage> codec_image)>;

  // Creates a SharedImage for |spec|, and returns it via the callback.
  // TODO(liberato): |texture_owner| is only needed to get the service id, to
  // create the per-frame texture.  All of that is only needed for legacy
  // mailbox support, where we have to have one texture per CodecImage.
  void CreateImage(FactoryImageReadyCB cb,
                   const SharedImageVideoProvider::ImageSpec& spec,
                   scoped_refptr<TextureOwner> texture_owner);

 private:
  // Creates a SharedImage for |mailbox|, and returns success or failure.
  bool CreateImageInternal(const SharedImageVideoProvider::ImageSpec& spec,
                           scoped_refptr<TextureOwner> texture_owner,
                           gpu::Mailbox mailbox,
                           scoped_refptr<CodecImage> image);

  void OnWillDestroyStub(bool have_context) override;

  // Removes |image| from |images_|.
  void OnImageDestructed(CodecImage* image);

  // Set our image group.  Must be called before the first call to
  // CreateVideoFrame occurs.
  void SetImageGroup(scoped_refptr<CodecImageGroup> image_group);

  // Outstanding images that should be considered for early rendering.
  std::vector<CodecImage*> images_;

  gpu::CommandBufferStub* stub_ = nullptr;

  // A helper for creating textures. Only valid while |stub_| is valid.
  std::unique_ptr<GLES2DecoderHelper> decoder_helper_;

  // Current image group to which new images (frames) will be added.  We'll
  // replace this when SetImageGroup() is called.
  scoped_refptr<CodecImageGroup> image_group_;

  // Sampler conversion information which is used in vulkan context. This is
  // constant for all the frames in a video and hence we cache it.
  base::Optional<gpu::VulkanYCbCrInfo> ycbcr_info_;

  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<GpuSharedImageVideoFactory> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(GpuSharedImageVideoFactory);
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
      images[back_buffer_index]->is_texture_owner_backed()) {
    images[back_buffer_index]->RenderToTextureOwnerBackBuffer();
  }
}

}  // namespace internal

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_VIDEO_FRAME_FACTORY_IMPL_
