// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/video_frame_factory_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"
#include "media/gpu//android/codec_image.h"
#include "media/gpu//android/codec_image_group.h"
#include "media/gpu/android/codec_wrapper.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"

namespace media {
namespace {

bool MakeContextCurrent(gpu::CommandBufferStub* stub) {
  return stub && stub->decoder_context()->MakeCurrent();
}

}  // namespace

VideoFrameFactoryImpl::VideoFrameFactoryImpl(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    GetStubCb get_stub_cb)
    : gpu_task_runner_(std::move(gpu_task_runner)),
      get_stub_cb_(std::move(get_stub_cb)) {}

VideoFrameFactoryImpl::~VideoFrameFactoryImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (gpu_video_frame_factory_)
    gpu_task_runner_->DeleteSoon(FROM_HERE, gpu_video_frame_factory_.release());
}

void VideoFrameFactoryImpl::Initialize(bool wants_promotion_hint,
                                       InitCb init_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!gpu_video_frame_factory_);
  gpu_video_frame_factory_ = std::make_unique<GpuVideoFrameFactory>();
  base::PostTaskAndReplyWithResult(
      gpu_task_runner_.get(), FROM_HERE,
      base::Bind(&GpuVideoFrameFactory::Initialize,
                 base::Unretained(gpu_video_frame_factory_.get()),
                 wants_promotion_hint, get_stub_cb_),
      std::move(init_cb));
}

void VideoFrameFactoryImpl::SetSurfaceBundle(
    scoped_refptr<AVDASurfaceBundle> surface_bundle) {
  scoped_refptr<CodecImageGroup> image_group;
  if (!surface_bundle) {
    // Clear everything, just so we're not holding a reference.
    surface_texture_ = nullptr;
  } else {
    // If |surface_bundle| is using a SurfaceTexture, then get it.
    surface_texture_ =
        surface_bundle->overlay ? nullptr : surface_bundle->surface_texture;

    // Start a new image group.  Note that there's no reason that we can't have
    // more than one group per surface bundle; it's okay if we're called
    // mulitiple times with the same surface bundle.  It just helps to combine
    // the callbacks if we don't, especially since AndroidOverlay doesn't know
    // how to remove destruction callbacks.  That's one reason why we don't just
    // make the CodecImage register itself.  The other is that the threading is
    // easier if we do it this way, since the image group is constructed on the
    // proper thread to talk to the overlay.
    image_group =
        base::MakeRefCounted<CodecImageGroup>(gpu_task_runner_, surface_bundle);
  }

  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuVideoFrameFactory::SetImageGroup,
                     base::Unretained(gpu_video_frame_factory_.get()),
                     std::move(image_group)));
}

void VideoFrameFactoryImpl::CreateVideoFrame(
    std::unique_ptr<CodecOutputBuffer> output_buffer,
    base::TimeDelta timestamp,
    gfx::Size natural_size,
    PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb,
    VideoDecoder::OutputCB output_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GpuVideoFrameFactory::CreateVideoFrame,
                 base::Unretained(gpu_video_frame_factory_.get()),
                 base::Passed(&output_buffer), surface_texture_, timestamp,
                 natural_size, std::move(promotion_hint_cb),
                 std::move(output_cb), base::ThreadTaskRunnerHandle::Get()));
}

void VideoFrameFactoryImpl::RunAfterPendingVideoFrames(
    base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Hop through |gpu_task_runner_| to ensure it comes after pending frames.
  gpu_task_runner_->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&base::DoNothing), std::move(closure));
}

GpuVideoFrameFactory::GpuVideoFrameFactory() : weak_factory_(this) {
  DETACH_FROM_THREAD(thread_checker_);
}

GpuVideoFrameFactory::~GpuVideoFrameFactory() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (stub_)
    stub_->RemoveDestructionObserver(this);
  ClearTextureRefs();
}

scoped_refptr<SurfaceTextureGLOwner> GpuVideoFrameFactory::Initialize(
    bool wants_promotion_hint,
    VideoFrameFactoryImpl::GetStubCb get_stub_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  wants_promotion_hint_ = wants_promotion_hint;
  stub_ = get_stub_cb.Run();
  if (!MakeContextCurrent(stub_))
    return nullptr;
  stub_->AddDestructionObserver(this);
  decoder_helper_ = GLES2DecoderHelper::Create(stub_->decoder_context());
  return SurfaceTextureGLOwnerImpl::Create();
}

void GpuVideoFrameFactory::CreateVideoFrame(
    std::unique_ptr<CodecOutputBuffer> output_buffer,
    scoped_refptr<SurfaceTextureGLOwner> surface_texture,
    base::TimeDelta timestamp,
    gfx::Size natural_size,
    PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb,
    VideoDecoder::OutputCB output_cb,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  scoped_refptr<VideoFrame> frame;
  scoped_refptr<gpu::gles2::TextureRef> texture_ref;
  CreateVideoFrameInternal(std::move(output_buffer), std::move(surface_texture),
                           timestamp, natural_size,
                           std::move(promotion_hint_cb), &frame, &texture_ref);
  if (!frame || !texture_ref)
    return;

  // Try to render this frame if possible.
  internal::MaybeRenderEarly(&images_);

  // TODO(sandersd, watk): The VideoFrame release callback will not be called
  // after MojoVideoDecoderService is destructed, so we have to release all
  // our TextureRefs when |this| is destructed. This can unback outstanding
  // VideoFrames (e.g., the current frame when the player is suspended). The
  // release callback lifetime should be separate from MCVD or
  // MojoVideoDecoderService (http://crbug.com/737220).
  texture_refs_[texture_ref.get()] = texture_ref;
  auto drop_texture_ref = base::BindOnce(&GpuVideoFrameFactory::DropTextureRef,
                                         weak_factory_.GetWeakPtr(),
                                         base::Unretained(texture_ref.get()));

  // Guarantee that the TextureRef is released even if the VideoFrame is
  // dropped. Otherwise we could keep TextureRefs we don't need alive.
  auto release_cb = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      BindToCurrentLoop(std::move(drop_texture_ref)), gpu::SyncToken());
  frame->SetReleaseMailboxCB(std::move(release_cb));
  task_runner->PostTask(FROM_HERE, base::BindOnce(output_cb, std::move(frame)));
}

void GpuVideoFrameFactory::CreateVideoFrameInternal(
    std::unique_ptr<CodecOutputBuffer> output_buffer,
    scoped_refptr<SurfaceTextureGLOwner> surface_texture,
    base::TimeDelta timestamp,
    gfx::Size natural_size,
    PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb,
    scoped_refptr<VideoFrame>* video_frame_out,
    scoped_refptr<gpu::gles2::TextureRef>* texture_ref_out) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!MakeContextCurrent(stub_))
    return;

  gpu::gles2::ContextGroup* group = stub_->decoder_context()->GetContextGroup();
  if (!group)
    return;
  gpu::gles2::TextureManager* texture_manager = group->texture_manager();
  if (!texture_manager)
    return;

  gfx::Size size = output_buffer->size();
  gfx::Rect visible_rect(size);
  // The pixel format doesn't matter as long as it's valid for texture frames.
  VideoPixelFormat pixel_format = PIXEL_FORMAT_ARGB;

  // Check that we can create a VideoFrame for this config before creating the
  // TextureRef so that we don't have to clean up the TextureRef if creating the
  // frame fails.
  if (!VideoFrame::IsValidConfig(pixel_format, VideoFrame::STORAGE_OPAQUE, size,
                                 visible_rect, natural_size)) {
    return;
  }

  // Create a Texture and a CodecImage to back it.
  scoped_refptr<gpu::gles2::TextureRef> texture_ref =
      decoder_helper_->CreateTexture(GL_TEXTURE_EXTERNAL_OES, GL_RGBA,
                                     size.width(), size.height(), GL_RGBA,
                                     GL_UNSIGNED_BYTE);
  auto image = base::MakeRefCounted<CodecImage>(
      std::move(output_buffer), surface_texture, std::move(promotion_hint_cb));
  images_.push_back(image.get());

  // Add |image| to our current image group.  This makes suer that any overlay
  // lasts as long as the images.  For SurfaceTexture, it doesn't do much.
  image_group_->AddCodecImage(image.get());

  // Attach the image to the texture.
  // If we're attaching a SurfaceTexture backed image, we set the state to
  // UNBOUND. This ensures that the implementation will call CopyTexImage()
  // which lets us update the surface texture at the right time.
  // For overlays we set the state to BOUND because it's required for
  // ScheduleOverlayPlane() to be called. If something tries to sample from an
  // overlay texture it won't work, but there's no way to make that work.
  auto image_state = surface_texture ? gpu::gles2::Texture::UNBOUND
                                     : gpu::gles2::Texture::BOUND;
  GLuint surface_texture_service_id =
      surface_texture ? surface_texture->GetTextureId() : 0;
  texture_manager->SetLevelStreamTextureImage(
      texture_ref.get(), GL_TEXTURE_EXTERNAL_OES, 0, image.get(), image_state,
      surface_texture_service_id);
  texture_manager->SetLevelCleared(texture_ref.get(), GL_TEXTURE_EXTERNAL_OES,
                                   0, true);

  gpu::Mailbox mailbox = decoder_helper_->CreateMailbox(texture_ref.get());
  gpu::MailboxHolder mailbox_holders[VideoFrame::kMaxPlanes];
  mailbox_holders[0] =
      gpu::MailboxHolder(mailbox, gpu::SyncToken(), GL_TEXTURE_EXTERNAL_OES);

  auto frame = VideoFrame::WrapNativeTextures(
      pixel_format, mailbox_holders, VideoFrame::ReleaseMailboxCB(), size,
      visible_rect, natural_size, timestamp);

  // The frames must be copied when threaded texture mailboxes are in use
  // (http://crbug.com/582170).
  if (group->gpu_preferences().enable_threaded_texture_mailboxes)
    frame->metadata()->SetBoolean(VideoFrameMetadata::COPY_REQUIRED, true);

  // We unconditionally mark the picture as overlayable, even if
  // |!surface_texture|, if we want to get hints.  It's required, else we won't
  // get hints.
  const bool allow_overlay = !surface_texture || wants_promotion_hint_;

  frame->metadata()->SetBoolean(VideoFrameMetadata::ALLOW_OVERLAY,
                                allow_overlay);
  frame->metadata()->SetBoolean(VideoFrameMetadata::WANTS_PROMOTION_HINT,
                                wants_promotion_hint_);
  frame->metadata()->SetBoolean(VideoFrameMetadata::SURFACE_TEXTURE,
                                !!surface_texture);

  *video_frame_out = std::move(frame);
  *texture_ref_out = std::move(texture_ref);
}

void GpuVideoFrameFactory::OnWillDestroyStub() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(stub_);
  ClearTextureRefs();
  stub_ = nullptr;
  decoder_helper_ = nullptr;
}

void GpuVideoFrameFactory::ClearTextureRefs() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(stub_ || texture_refs_.empty());
  // If we fail to make the context current, we have to notify the TextureRefs
  // so they don't try to delete textures without a context.
  if (!MakeContextCurrent(stub_)) {
    for (const auto& kv : texture_refs_)
      kv.first->ForceContextLost();
  }
  texture_refs_.clear();
}

void GpuVideoFrameFactory::DropTextureRef(gpu::gles2::TextureRef* ref,
                                          const gpu::SyncToken& token) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = texture_refs_.find(ref);
  if (it == texture_refs_.end())
    return;
  // If we fail to make the context current, we have to notify the TextureRef
  // so it doesn't try to delete a texture without a context.
  if (!MakeContextCurrent(stub_))
    ref->ForceContextLost();
  texture_refs_.erase(it);
}

void GpuVideoFrameFactory::OnImageDestructed(CodecImage* image) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::Erase(images_, image);
  internal::MaybeRenderEarly(&images_);
}

void GpuVideoFrameFactory::SetImageGroup(
    scoped_refptr<CodecImageGroup> image_group) {
  image_group_ = std::move(image_group);

  if (!image_group_)
    return;

  image_group_->SetDestructionCb(base::BindRepeating(
      &GpuVideoFrameFactory::OnImageDestructed, weak_factory_.GetWeakPtr()));
}

}  // namespace media
