// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/video_frame_factory_impl.h"

#include <memory>

#include "base/android/android_image_reader_compat.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "base/threading/sequence_bound.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "media/gpu/android/codec_image.h"
#include "media/gpu/android/codec_image_group.h"
#include "media/gpu/android/codec_wrapper.h"
#include "media/gpu/android/shared_image_video.h"
#include "media/gpu/command_buffer_helper.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/scoped_make_current.h"

namespace media {
namespace {

bool MakeContextCurrent(gpu::CommandBufferStub* stub) {
  return stub && stub->decoder_context()->MakeCurrent();
}

TextureOwner::Mode GetTextureOwnerMode(
    VideoFrameFactory::OverlayMode overlay_mode) {
  const bool a_image_reader_supported =
      base::android::AndroidImageReader::GetInstance().IsSupported();

  switch (overlay_mode) {
    case VideoFrameFactory::OverlayMode::kDontRequestPromotionHints:
    case VideoFrameFactory::OverlayMode::kRequestPromotionHints:
      return a_image_reader_supported && base::FeatureList::IsEnabled(
                                             media::kAImageReaderVideoOutput)
                 ? TextureOwner::Mode::kAImageReaderInsecure
                 : TextureOwner::Mode::kSurfaceTextureInsecure;
    case VideoFrameFactory::OverlayMode::kSurfaceControlSecure:
      DCHECK(a_image_reader_supported);
      return TextureOwner::Mode::kAImageReaderSecure;
    case VideoFrameFactory::OverlayMode::kSurfaceControlInsecure:
      DCHECK(a_image_reader_supported);
      return TextureOwner::Mode::kAImageReaderInsecure;
  }

  NOTREACHED();
  return TextureOwner::Mode::kSurfaceTextureInsecure;
}

scoped_refptr<gpu::SharedContextState> GetSharedContext(
    gpu::CommandBufferStub* stub,
    gpu::ContextResult* result) {
  auto shared_context =
      stub->channel()->gpu_channel_manager()->GetSharedContextState(result);
  if (*result != gpu::ContextResult::kSuccess)
    return nullptr;
  return shared_context;
}

void ContextStateResultUMA(gpu::ContextResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "Media.GpuSharedImageVideoFactory.SharedContextStateResult", result);
}

}  // namespace

using gpu::gles2::AbstractTexture;

// TODO(liberato): This should be in its own file.  However, for now, it's here
// so that we don't have to move GpuSharedImageFactory with it, since the latter
// is an implementation detail.  This makes the diffs much easier to review.
// Moving it can be done separately with no other code changes.
class DirectSharedImageVideoProvider : public SharedImageVideoProvider {
 public:
  DirectSharedImageVideoProvider(
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      VideoFrameFactory::OverlayMode overlay_mode,
      VideoFrameFactory::GetStubCb get_stub_cb,
      VideoFrameFactory::InitCb init_cb)
      : gpu_factory_(gpu_task_runner), gpu_task_runner_(gpu_task_runner) {
    gpu_factory_.Post(FROM_HERE, &GpuSharedImageVideoFactory::Initialize,
                      overlay_mode, std::move(get_stub_cb),
                      BindToCurrentLoop(std::move(init_cb)));
  }

  // TODO(liberato): add a thread hop to create the default texture owner, but
  // not as part of this class.  just post something from VideoFrameFactory.
  void Initialize(VideoFrameFactory::OverlayMode overlay_mode,
                  VideoFrameFactory::GetStubCb get_stub_cb) {}

  void RequestImage(ImageReadyCB cb,
                    const ImageSpec& spec,
                    std::unique_ptr<CodecOutputBuffer> output_buffer,
                    scoped_refptr<TextureOwner> texture_owner,
                    PromotionHintAggregator::NotifyPromotionHintCB
                        promotion_hint_cb) override {
    auto image_ready_cb = BindToCurrentLoop(
        base::BindOnce(&DirectSharedImageVideoProvider::OnImageReady,
                       std::move(cb), std::move(output_buffer), texture_owner,
                       std::move(promotion_hint_cb), gpu_task_runner_));

    // TODO(liberato): If the image group is different, then tell |gpu_factory_|
    // about it, so that it can register a destruction callback on it.  This is
    // a complete hack for MaybeRenderEarly.  We shouldn't care about that at
    // all.  Instead, VideoFrameFactoryImpl should register a callback on the
    // image group, and it should MaybeRenderEarly instead.  The only reason we
    // don't do that now is that VideoFrameFactoryImpl can't really maintain
    // |images_|, since it never sees the CodecImage.  The output buffer should
    // be wrapped up in something that knows how to RenderToFrontBuffer, etc.,
    // rather than all of that state logic that's in CodecImage.  CodecImage
    // should just be a GL implementation detail that might own one of these new
    // things.  The image group would be a "new thing" group (the codec image
    // would drop it on destruction, so same effect.  if we're recycling buffers
    // then we'd ask it to clear its "new thing" ptr.).
    //
    // In any case, VideoFrameFactoryImpl would track these owners like we track
    // CodecImage*'s now in |images_|, and it could handle MaybeRenderEarly.

    gpu_factory_.Post(FROM_HERE, &GpuSharedImageVideoFactory::CreateImage,
                      std::move(image_ready_cb), spec, texture_owner);
  }

  static void OnImageReady(
      ImageReadyCB cb,
      std::unique_ptr<CodecOutputBuffer> output_buffer,
      scoped_refptr<TextureOwner> texture_owner,
      PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb,
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      ImageRecord record,
      scoped_refptr<CodecImage> codec_image) {
    codec_image->Initialize(std::move(output_buffer), texture_owner,
                            std::move(promotion_hint_cb));
    // Note that we could probably drop |codec_image| before calling |cb|, since
    // the codec image is also held on the gpu side, and will be until the
    // |record.release_cb| is run.  However, just to avoid potential weirness
    // during shutdown, we still post our reference.
    gpu_task_runner->ReleaseSoon(FROM_HERE, std::move(codec_image));

    std::move(cb).Run(std::move(record));
  }

  base::SequenceBound<GpuSharedImageVideoFactory> gpu_factory_;
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
};

VideoFrameFactoryImpl::VideoFrameFactoryImpl(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    GetStubCb get_stub_cb,
    const gpu::GpuPreferences& gpu_preferences)
    : gpu_task_runner_(std::move(gpu_task_runner)),
      get_stub_cb_(std::move(get_stub_cb)),
      enable_threaded_texture_mailboxes_(
          gpu_preferences.enable_threaded_texture_mailboxes) {}

VideoFrameFactoryImpl::~VideoFrameFactoryImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  gpu_task_runner_->ReleaseSoon(FROM_HERE, std::move(image_group_));
}

void VideoFrameFactoryImpl::Initialize(OverlayMode overlay_mode,
                                       InitCb init_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!image_provider_);
  overlay_mode_ = overlay_mode;
  image_provider_ = std::make_unique<DirectSharedImageVideoProvider>(
      gpu_task_runner_, overlay_mode, get_stub_cb_, std::move(init_cb));
}

void VideoFrameFactoryImpl::SetSurfaceBundle(
    scoped_refptr<CodecSurfaceBundle> surface_bundle) {
  scoped_refptr<CodecImageGroup> image_group;
  if (!surface_bundle) {
    // Clear everything, just so we're not holding a reference.
    texture_owner_ = nullptr;
  } else {
    // If |surface_bundle| is using a TextureOwner, then get it.
    texture_owner_ =
        surface_bundle->overlay() ? nullptr : surface_bundle->texture_owner();

    // Start a new image group.  Note that there's no reason that we can't have
    // more than one group per surface bundle; it's okay if we're called
    // mulitiple times with the same surface bundle.  It just helps to combine
    // the callbacks if we don't, especially since AndroidOverlay doesn't know
    // how to remove destruction callbacks.  That's one reason why we don't just
    // make the CodecImage register itself.  The other is that the threading is
    // easier if we do it this way, since the image group is constructed on the
    // proper thread to talk to the overlay.
    gpu_task_runner_->ReleaseSoon(FROM_HERE, std::move(image_group_));
    image_group_ =
        base::MakeRefCounted<CodecImageGroup>(gpu_task_runner_, surface_bundle);
  }
}

void VideoFrameFactoryImpl::CreateVideoFrame(
    std::unique_ptr<CodecOutputBuffer> output_buffer,
    base::TimeDelta timestamp,
    gfx::Size natural_size,
    PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb,
    OnceOutputCb output_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  gfx::Size coded_size = output_buffer->size();
  gfx::Rect visible_rect(coded_size);
  // The pixel format doesn't matter as long as it's valid for texture frames.
  VideoPixelFormat pixel_format = PIXEL_FORMAT_ARGB;

  // Check that we can create a VideoFrame for this config before trying to
  // create the textures for it.
  if (!VideoFrame::IsValidConfig(pixel_format, VideoFrame::STORAGE_OPAQUE,
                                 coded_size, visible_rect, natural_size)) {
    LOG(ERROR) << __func__ << " unsupported video frame format";
    std::move(output_cb).Run(nullptr);
    return;
  }

  SharedImageVideoProvider::ImageSpec spec(coded_size, image_group_);

  auto image_ready_cb = base::BindOnce(
      &VideoFrameFactoryImpl::OnImageReady, std::move(output_cb), timestamp,
      coded_size, natural_size, texture_owner_, pixel_format, overlay_mode_,
      enable_threaded_texture_mailboxes_);

  image_provider_->RequestImage(std::move(image_ready_cb), spec,
                                std::move(output_buffer), texture_owner_,
                                std::move(promotion_hint_cb));
}

// static
void VideoFrameFactoryImpl::OnImageReady(
    OnceOutputCb output_cb,
    base::TimeDelta timestamp,
    gfx::Size coded_size,
    gfx::Size natural_size,
    scoped_refptr<TextureOwner> texture_owner,
    VideoPixelFormat pixel_format,
    OverlayMode overlay_mode,
    bool enable_threaded_texture_mailboxes,
    SharedImageVideoProvider::ImageRecord record) {
  TRACE_EVENT0("media", "VideoVideoFrameFactoryImpl::OnVideoFrameImageReady");

  gpu::MailboxHolder mailbox_holders[VideoFrame::kMaxPlanes];
  mailbox_holders[0] = gpu::MailboxHolder(record.mailbox, gpu::SyncToken(),
                                          GL_TEXTURE_EXTERNAL_OES);

  // TODO(liberato): We should set the promotion hint cb here on the image.  We
  // should also set the output buffer params; we shouldn't send the output
  // buffer to the gpu thread, since the codec image isn't in use anyway.  We
  // can access it on any thread.  We'll also need to get new images when we
  // switch texture owners.  That's left for future work.

  // TODO(liberato): When we switch to a pool, we need to provide some way to
  // call MaybeRenderEarly that doesn't depend on |release_cb|.  I suppose we
  // could get a RepeatingCallback that's a "reuse cb", that we'd attach to the
  // VideoFrame's release cb, since we have to wait for the sync token anyway.
  // That would run on the gpu thread, and could MaybeRenderEarly.

  gfx::Rect visible_rect(coded_size);

  auto frame = VideoFrame::WrapNativeTextures(
      pixel_format, mailbox_holders, VideoFrame::ReleaseMailboxCB(), coded_size,
      visible_rect, natural_size, timestamp);

  frame->set_ycbcr_info(record.ycbcr_info);
  // If, for some reason, we failed to create a frame, then fail.  Note that we
  // don't need to call |release_cb|; dropping it is okay since the api says so.
  if (!frame) {
    LOG(ERROR) << __func__ << " failed to create video frame";
    std::move(output_cb).Run(nullptr);
    return;
  }

  // The frames must be copied when threaded texture mailboxes are in use
  // (http://crbug.com/582170).
  if (enable_threaded_texture_mailboxes)
    frame->metadata()->SetBoolean(VideoFrameMetadata::COPY_REQUIRED, true);

  const bool is_surface_control =
      overlay_mode == OverlayMode::kSurfaceControlSecure ||
      overlay_mode == OverlayMode::kSurfaceControlInsecure;
  const bool wants_promotion_hints =
      overlay_mode == OverlayMode::kRequestPromotionHints;

  // Remember that we can't access |texture_owner|, but we can check if we have
  // one here.
  bool allow_overlay = false;
  if (is_surface_control) {
    DCHECK(texture_owner);
    allow_overlay = true;
  } else {
    // We unconditionally mark the picture as overlayable, even if
    // |!texture_owner|, if we want to get hints.  It's required, else we won't
    // get hints.
    allow_overlay = !texture_owner || wants_promotion_hints;
  }

  frame->metadata()->SetBoolean(VideoFrameMetadata::ALLOW_OVERLAY,
                                allow_overlay);
  frame->metadata()->SetBoolean(VideoFrameMetadata::WANTS_PROMOTION_HINT,
                                wants_promotion_hints);
  frame->metadata()->SetBoolean(VideoFrameMetadata::TEXTURE_OWNER,
                                !!texture_owner);

  frame->SetReleaseMailboxCB(std::move(record.release_cb));

  // Note that we don't want to handle the CodecImageGroup here.  It needs to be
  // accessed on the gpu thread.  Once we move to pooling, only the initial
  // create / destroy operations will affect it anyway, so it might as well stay
  // on the gpu thread.

  std::move(output_cb).Run(std::move(frame));
}

void VideoFrameFactoryImpl::RunAfterPendingVideoFrames(
    base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Hop through |gpu_task_runner_| to ensure it comes after pending frames.
  gpu_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                     std::move(closure));
}

GpuSharedImageVideoFactory::GpuSharedImageVideoFactory() : weak_factory_(this) {
  DETACH_FROM_THREAD(thread_checker_);
}

GpuSharedImageVideoFactory::~GpuSharedImageVideoFactory() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (stub_)
    stub_->RemoveDestructionObserver(this);
}

void GpuSharedImageVideoFactory::Initialize(
    VideoFrameFactoryImpl::OverlayMode overlay_mode,
    VideoFrameFactoryImpl::GetStubCb get_stub_cb,
    VideoFrameFactory::InitCb init_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  stub_ = get_stub_cb.Run();
  if (!MakeContextCurrent(stub_)) {
    std::move(init_cb).Run(nullptr);
    return;
  }
  stub_->AddDestructionObserver(this);

  decoder_helper_ = GLES2DecoderHelper::Create(stub_->decoder_context());

  gpu::ContextResult result;
  auto shared_context = GetSharedContext(stub_, &result);
  if (!shared_context) {
    LOG(ERROR) << "GpuSharedImageVideoFactory: Unable to get a shared context.";
    ContextStateResultUMA(result);
    std::move(init_cb).Run(nullptr);
    return;
  }

  // Make the shared context current.
  auto scoped_current = std::make_unique<ui::ScopedMakeCurrent>(
      shared_context->context(), shared_context->surface());
  if (!shared_context->IsCurrent(nullptr)) {
    result = gpu::ContextResult::kTransientFailure;
    LOG(ERROR)
        << "GpuSharedImageVideoFactory: Unable to make shared context current.";
    ContextStateResultUMA(result);
    std::move(init_cb).Run(nullptr);
    return;
  }
  std::move(init_cb).Run(
      TextureOwner::Create(TextureOwner::CreateTexture(shared_context.get()),
                           GetTextureOwnerMode(overlay_mode)));
}

void GpuSharedImageVideoFactory::CreateImage(
    FactoryImageReadyCB image_ready_cb,
    const SharedImageVideoProvider::ImageSpec& spec,
    scoped_refptr<TextureOwner> texture_owner) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Very, very hackily register to MaybeRenderEarly on this group, if needed.
  // We should not be responsible for this.  See elsewhere.
  if (spec.image_group != image_group_)
    SetImageGroup(spec.image_group);

  // Generate a shared image mailbox.
  auto mailbox = gpu::Mailbox::GenerateForSharedImage();
  auto codec_image = base::MakeRefCounted<CodecImage>();

  bool success =
      CreateImageInternal(spec, std::move(texture_owner), mailbox, codec_image);
  TRACE_EVENT0("media", "GpuSharedImageVideoFactory::CreateVideoFrame");
  if (!success)
    return;

  // Try to render this frame if possible.
  // TODO(liberato): This should not be here.
  internal::MaybeRenderEarly(&images_);

  // This callback destroys the shared image when video frame is
  // released/destroyed. This callback has a weak pointer to the shared image
  // stub because shared image stub could be destroyed before video frame. In
  // those cases there is no need to destroy the shared image as the shared
  // image stub destruction will cause all the shared images to be destroyed.
  auto destroy_shared_image =
      stub_->channel()->shared_image_stub()->GetSharedImageDestructionCallback(
          mailbox);

  // Guarantee that the SharedImage is destroyed even if the VideoFrame is
  // dropped. Otherwise we could keep shared images we don't need alive.
  auto release_cb = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      BindToCurrentLoop(std::move(destroy_shared_image)), gpu::SyncToken());

  SharedImageVideoProvider::ImageRecord record;
  record.mailbox = mailbox;
  record.release_cb = std::move(release_cb);
  record.ycbcr_info = ycbcr_info_;

  std::move(image_ready_cb).Run(std::move(record), std::move(codec_image));
}

bool GpuSharedImageVideoFactory::CreateImageInternal(
    const SharedImageVideoProvider::ImageSpec& spec,
    scoped_refptr<TextureOwner> texture_owner,
    gpu::Mailbox mailbox,
    scoped_refptr<CodecImage> image) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!MakeContextCurrent(stub_))
    return false;

  gpu::gles2::ContextGroup* group = stub_->decoder_context()->GetContextGroup();
  if (!group)
    return false;
  gpu::gles2::TextureManager* texture_manager = group->texture_manager();
  if (!texture_manager)
    return false;

  const auto& size = spec.size;

  // Create a Texture and a CodecImage to back it.
  // TODO(liberato): Once legacy mailbox support is removed, we don't need to
  // create this texture.  So, we won't need |texture_owner| either.
  std::unique_ptr<AbstractTexture> texture = decoder_helper_->CreateTexture(
      GL_TEXTURE_EXTERNAL_OES, GL_RGBA, size.width(), size.height(), GL_RGBA,
      GL_UNSIGNED_BYTE);
  images_.push_back(image.get());

  // Add |image| to our current image group.  This makes sure that any overlay
  // lasts as long as the images.  For TextureOwner, it doesn't do much.
  spec.image_group->AddCodecImage(image.get());

  // Attach the image to the texture.
  // Either way, we expect this to be UNBOUND (i.e., decoder-managed).  For
  // overlays, BindTexImage will return true, causing it to transition to the
  // BOUND state, and thus receive ScheduleOverlayPlane calls.  For TextureOwner
  // backed images, BindTexImage will return false, and CopyTexImage will be
  // tried next.
  // TODO(liberato): consider not binding this as a StreamTextureImage if we're
  // using an overlay.  There's no advantage.  We'd likely want to create (and
  // initialize to a 1x1 texture) a 2D texture above in that case, in case
  // somebody tries to sample from it.  Be sure that promotion hints still
  // work properly, though -- they might require a stream texture image.
  GLuint texture_owner_service_id =
      texture_owner ? texture_owner->GetTextureId() : 0;
  texture->BindStreamTextureImage(image.get(), texture_owner_service_id);

  gpu::ContextResult result;
  auto shared_context = GetSharedContext(stub_, &result);
  if (!shared_context) {
    LOG(ERROR) << "GpuSharedImageVideoFactory: Unable to get a shared context.";
    ContextStateResultUMA(result);
    return false;
  }

  // Create a shared image.
  // TODO(vikassoni): Hardcoding colorspace to SRGB. Figure how if media has a
  // colorspace and wire it here.
  // TODO(vikassoni): This shared image need to be thread safe eventually for
  // webview to work with shared images.
  auto shared_image = std::make_unique<SharedImageVideo>(
      mailbox, gfx::ColorSpace::CreateSRGB(), std::move(image),
      std::move(texture), std::move(shared_context),
      false /* is_thread_safe */);

  if (!ycbcr_info_)
    ycbcr_info_ = shared_image->GetYcbcrInfo();

  // Register it with shared image mailbox as well as legacy mailbox. This
  // keeps |shared_image| around until its destruction cb is called.
  // NOTE: Currently none of the video mailbox consumer uses shared image
  // mailbox.
  DCHECK(stub_->channel()->gpu_channel_manager()->shared_image_manager());
  stub_->channel()->shared_image_stub()->factory()->RegisterBacking(
      std::move(shared_image), /* legacy_mailbox */ true);

  return true;
}

void GpuSharedImageVideoFactory::OnWillDestroyStub(bool have_context) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(stub_);
  stub_ = nullptr;
  decoder_helper_ = nullptr;
}

void GpuSharedImageVideoFactory::OnImageDestructed(CodecImage* image) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::Erase(images_, image);
  internal::MaybeRenderEarly(&images_);
}

void GpuSharedImageVideoFactory::SetImageGroup(
    scoped_refptr<CodecImageGroup> image_group) {
  image_group_ = std::move(image_group);

  if (!image_group_)
    return;

  image_group_->SetDestructionCb(
      base::BindRepeating(&GpuSharedImageVideoFactory::OnImageDestructed,
                          weak_factory_.GetWeakPtr()));
}

}  // namespace media
