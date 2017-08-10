// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/media_codec_video_decoder.h"

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "media/base/android/media_codec_bridge_impl.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "media/gpu/android_video_surface_chooser.h"
#include "media/gpu/avda_codec_allocator.h"
#include "media/gpu/content_video_view_overlay.h"

namespace media {
namespace {

// Don't use MediaCodec's internal software decoders when we have more secure
// and up to date versions in the renderer process.
bool IsMediaCodecSoftwareDecodingForbidden(const VideoDecoderConfig& config) {
  return !config.is_encrypted() &&
         (config.codec() == kCodecVP8 || config.codec() == kCodecVP9);
}

bool ConfigSupported(const VideoDecoderConfig& config,
                     DeviceInfo* device_info) {
  // Don't support larger than 4k because it won't perform well on many devices.
  const auto size = config.coded_size();
  if (size.width() > 3840 || size.height() > 2160)
    return false;

  // Only use MediaCodec for VP8 or VP9 if it's likely backed by hardware or if
  // the stream is encrypted.
  const auto codec = config.codec();
  if (IsMediaCodecSoftwareDecodingForbidden(config) &&
      device_info->IsDecoderKnownUnaccelerated(codec)) {
    DVLOG(2) << "Config not supported: " << GetCodecName(codec)
             << " is not hardware accelerated";
    return false;
  }

  switch (codec) {
    case kCodecVP8:
    case kCodecVP9: {
      if ((codec == kCodecVP8 && !device_info->IsVp8DecoderAvailable()) ||
          (codec == kCodecVP9 && !device_info->IsVp9DecoderAvailable())) {
        return false;
      }

      // There's no fallback for encrypted content so we support all sizes.
      if (config.is_encrypted())
        return true;

      // Below 360p there's little to no power benefit to using MediaCodec over
      // libvpx so we prefer to fall back to that.
      if (size.width() < 480 || size.height() < 360)
        return false;

      return true;
    }
    case kCodecH264:
      return true;
#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
    case kCodecHEVC:
      return true;
#endif
    default:
      return false;
  }
}

}  // namespace

CodecAllocatorAdapter::CodecAllocatorAdapter() = default;
CodecAllocatorAdapter::~CodecAllocatorAdapter() = default;

void CodecAllocatorAdapter::OnCodecConfigured(
    std::unique_ptr<MediaCodecBridge> media_codec) {
  codec_created_cb.Run(std::move(media_codec));
}

// static
PendingDecode PendingDecode::CreateEos() {
  auto nop = [](DecodeStatus s) {};
  return {DecoderBuffer::CreateEOSBuffer(), base::Bind(nop)};
}

PendingDecode::PendingDecode(scoped_refptr<DecoderBuffer> buffer,
                             VideoDecoder::DecodeCB decode_cb)
    : buffer(buffer), decode_cb(decode_cb) {}
PendingDecode::PendingDecode(PendingDecode&& other) = default;
PendingDecode::~PendingDecode() = default;

MediaCodecVideoDecoder::MediaCodecVideoDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    base::Callback<gpu::GpuCommandBufferStub*()> get_stub_cb,
    VideoFrameFactory::OutputWithReleaseMailboxCB output_cb,
    DeviceInfo* device_info,
    AVDACodecAllocator* codec_allocator,
    std::unique_ptr<AndroidVideoSurfaceChooser> surface_chooser,
    std::unique_ptr<VideoFrameFactory> video_frame_factory)
    : state_(State::kBeforeSurfaceInit),
      lazy_init_pending_(true),
      output_cb_(output_cb),
      gpu_task_runner_(gpu_task_runner),
      get_stub_cb_(get_stub_cb),
      codec_allocator_(codec_allocator),
      surface_chooser_(std::move(surface_chooser)),
      video_frame_factory_(std::move(video_frame_factory)),
      device_info_(device_info),
      weak_factory_(this) {
  DVLOG(2) << __func__;
}

MediaCodecVideoDecoder::~MediaCodecVideoDecoder() {
  ReleaseCodec();
  // Mojo callbacks require that they're run before destruction.
  if (reset_cb_)
    reset_cb_.Run();
  codec_allocator_->StopThread(&codec_allocator_adapter_);
}

void MediaCodecVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                        bool low_delay,
                                        CdmContext* cdm_context,
                                        const InitCB& init_cb,
                                        const OutputCB& output_cb) {
  const bool first_init = !decoder_config_.IsValidConfig();
  DVLOG(2) << (first_init ? "Initializing" : "Reinitializing")
           << " MCVD with config: " << config.AsHumanReadableString();

  InitCB bound_init_cb = BindToCurrentLoop(init_cb);
  if (!ConfigSupported(config, device_info_)) {
    bound_init_cb.Run(false);
    return;
  }

  // Disallow codec changes when reinitializing.
  if (!first_init && decoder_config_.codec() != config.codec()) {
    DVLOG(1) << "Codec changed: cannot reinitialize";
    bound_init_cb.Run(false);
    return;
  }

  decoder_config_ = config;

  if (first_init) {
    if (!codec_allocator_->StartThread(&codec_allocator_adapter_)) {
      LOG(ERROR) << "Unable to start thread";
      bound_init_cb.Run(false);
      return;
    }

    codec_config_ = new CodecConfig();
    codec_config_->codec = config.codec();
    // TODO(watk): Set |requires_secure_codec| correctly using
    // MediaDrmBridgeCdmContext::MediaCryptoReadyCB.
    codec_config_->requires_secure_codec = config.is_encrypted();
  }

  codec_config_->initial_expected_coded_size = config.coded_size();
  // TODO(watk): Parse config.extra_data().

  // We defer initialization of the Surface and MediaCodec until we
  // receive a Decode() call to avoid consuming those resources in cases where
  // we'll be destructed before getting a Decode(). Failure to initialize those
  // resources will be reported as a decode error on the first decode.
  // TODO(watk): Initialize the CDM before calling init_cb.
  init_cb.Run(true);
}

void MediaCodecVideoDecoder::StartLazyInit() {
  DVLOG(2) << __func__;
  video_frame_factory_->Initialize(
      gpu_task_runner_, get_stub_cb_,
      base::Bind(&MediaCodecVideoDecoder::OnVideoFrameFactoryInitialized,
                 weak_factory_.GetWeakPtr()));
}

void MediaCodecVideoDecoder::OnVideoFrameFactoryInitialized(
    scoped_refptr<SurfaceTextureGLOwner> surface_texture) {
  DVLOG(2) << __func__;
  if (!surface_texture) {
    HandleError();
    return;
  }
  surface_texture_ = std::move(surface_texture);
  InitializeSurfaceChooser();
}

void MediaCodecVideoDecoder::SetOverlayInfo(const OverlayInfo& overlay_info) {
  DVLOG(2) << __func__;
  bool overlay_changed = !overlay_info_.RefersToSameOverlayAs(overlay_info);
  overlay_info_ = overlay_info;
  // Only update surface chooser if it's initialized and the overlay changed.
  if (state_ != State::kBeforeSurfaceInit && overlay_changed)
    surface_chooser_->UpdateState(CreateOverlayFactoryCb(), chooser_state_);
}

void MediaCodecVideoDecoder::InitializeSurfaceChooser() {
  DVLOG(2) << __func__;
  DCHECK_EQ(state_, State::kBeforeSurfaceInit);
  // Initialize |surface_chooser_| and wait for its decision. Note: the
  // callback may be reentrant.
  surface_chooser_->Initialize(
      base::Bind(&MediaCodecVideoDecoder::OnSurfaceChosen,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&MediaCodecVideoDecoder::OnSurfaceChosen,
                 weak_factory_.GetWeakPtr(), nullptr),
      CreateOverlayFactoryCb(), chooser_state_);
}

void MediaCodecVideoDecoder::OnSurfaceChosen(
    std::unique_ptr<AndroidOverlay> overlay) {
  DVLOG(2) << __func__;
  if (overlay) {
    overlay->AddSurfaceDestroyedCallback(
        base::Bind(&MediaCodecVideoDecoder::OnSurfaceDestroyed,
                   weak_factory_.GetWeakPtr()));
  }

  // If we were waiting for our first surface during initialization, then
  // proceed to create a codec with the chosen surface.
  if (state_ == State::kBeforeSurfaceInit) {
    codec_config_->surface_bundle =
        overlay ? new AVDASurfaceBundle(std::move(overlay))
                : new AVDASurfaceBundle(surface_texture_);
    CreateCodec();
    return;
  }

  // If setOutputSurface() is not supported we can't do the transition.
  if (!device_info_->IsSetOutputSurfaceSupported())
    return;

  // If we're told to switch to a SurfaceTexture but we're already using a
  // SurfaceTexture, we just cancel any pending transition.
  // (It's not possible for this to choose the overlay we're already using.)
  if (!overlay && codec_config_->surface_bundle &&
      !codec_config_->surface_bundle->overlay) {
    incoming_surface_.reset();
    return;
  }

  incoming_surface_.emplace(overlay ? new AVDASurfaceBundle(std::move(overlay))
                                    : new AVDASurfaceBundle(surface_texture_));
}

void MediaCodecVideoDecoder::OnSurfaceDestroyed(AndroidOverlay* overlay) {
  DVLOG(2) << __func__;

  // If there is a pending transition to the overlay, cancel it.
  if (incoming_surface_ && (*incoming_surface_)->overlay.get() == overlay) {
    incoming_surface_.reset();
    return;
  }

  // If we've already stopped using the overlay, ignore it.
  if (!codec_config_->surface_bundle ||
      codec_config_->surface_bundle->overlay.get() != overlay) {
    return;
  }

  // TODO(watk): If setOutputSurface() is available we're supposed to
  // transparently handle surface transitions, however we don't handle them
  // while codec creation is in progress. It should be handled gracefully
  // by allocating a new codec.
  if (state_ == State::kWaitingForCodec) {
    state_ = State::kSurfaceDestroyed;
    return;
  }

  if (!device_info_->IsSetOutputSurfaceSupported()) {
    state_ = State::kSurfaceDestroyed;
    ReleaseCodecAndBundle();
    if (drain_type_)
      OnCodecDrained();
    return;
  }

  // If there isn't a pending overlay then transition to a SurfaceTexture.
  if (!incoming_surface_)
    incoming_surface_.emplace(new AVDASurfaceBundle(surface_texture_));
  TransitionToIncomingSurface();
}

void MediaCodecVideoDecoder::TransitionToIncomingSurface() {
  DVLOG(2) << __func__;
  DCHECK(incoming_surface_);
  DCHECK(codec_);
  auto surface_bundle = std::move(*incoming_surface_);
  incoming_surface_.reset();
  if (codec_->SetSurface(surface_bundle->GetJavaSurface()) == MEDIA_CODEC_OK) {
    codec_config_->surface_bundle = std::move(surface_bundle);
  } else {
    ReleaseCodecAndBundle();
    HandleError();
  }
}

void MediaCodecVideoDecoder::CreateCodec() {
  DCHECK(!codec_);
  state_ = State::kWaitingForCodec;
  codec_allocator_adapter_.codec_created_cb = base::Bind(
      &MediaCodecVideoDecoder::OnCodecCreated, weak_factory_.GetWeakPtr());
  codec_allocator_->CreateMediaCodecAsync(codec_allocator_adapter_.AsWeakPtr(),
                                          codec_config_);
}

void MediaCodecVideoDecoder::OnCodecCreated(
    std::unique_ptr<MediaCodecBridge> codec) {
  DCHECK(state_ == State::kWaitingForCodec ||
         state_ == State::kSurfaceDestroyed);
  DCHECK(!codec_);
  if (codec) {
    codec_ = base::MakeUnique<CodecWrapper>(
        std::move(codec),
        BindToCurrentLoop(base::Bind(&MediaCodecVideoDecoder::ManageTimer,
                                     weak_factory_.GetWeakPtr(), true)));
  }

  // If we entered state kSurfaceDestroyed while we were waiting for
  // the codec, then it's already invalid and we have to drop it.
  if (state_ == State::kSurfaceDestroyed) {
    ReleaseCodecAndBundle();
    return;
  }

  // Handle the failure case after the kSurfaceDestroyed case to avoid
  // transitioning from kSurfaceDestroyed to kError.
  if (!codec_) {
    HandleError();
    return;
  }

  state_ = State::kOk;
  ManageTimer(true);
}

void MediaCodecVideoDecoder::Decode(const scoped_refptr<DecoderBuffer>& buffer,
                                    const DecodeCB& decode_cb) {
  DVLOG(2) << __func__;

  if (state_ == State::kError) {
    decode_cb.Run(DecodeStatus::DECODE_ERROR);
    return;
  }
  pending_decodes_.emplace_back(buffer, std::move(decode_cb));

  if (lazy_init_pending_) {
    lazy_init_pending_ = false;
    StartLazyInit();
    return;
  }

  PumpCodec(true);
}

void MediaCodecVideoDecoder::FlushCodec() {
  DVLOG(2) << __func__;
  if (!codec_ || codec_->IsFlushed())
    return;
  if (codec_->SupportsFlush(device_info_)) {
    DVLOG(2) << "Flushing codec";
    if (!codec_->Flush()) {
      HandleError();
      return;
    }
  } else {
    DVLOG(2) << "flush() workaround: creating a new codec";
    // Release the codec and create a new one with the same surface bundle.
    // TODO(watk): We should guarantee that the new codec is created after the
    // current one is released so they aren't attached to the same surface at
    // the same time. This doesn't usually happen because the release and
    // creation usually post to the same thread, but it's not guaranteed.
    ReleaseCodec();
    CreateCodec();
  }
}

void MediaCodecVideoDecoder::PumpCodec(bool force_start_timer) {
  DVLOG(4) << __func__;
  bool did_work = false, did_input = false, did_output = false;
  do {
    did_input = QueueInput();
    did_output = DequeueOutput();
    if (did_input || did_output)
      did_work = true;
  } while (did_input || did_output);

  ManageTimer(did_work || force_start_timer);
}

void MediaCodecVideoDecoder::ManageTimer(bool start_timer) {
  const base::TimeDelta kPollingPeriod = base::TimeDelta::FromMilliseconds(10);
  const base::TimeDelta kIdleTimeout = base::TimeDelta::FromSeconds(1);

  if (!idle_timer_ || start_timer)
    idle_timer_ = base::ElapsedTimer();

  if (!start_timer && idle_timer_->Elapsed() > kIdleTimeout) {
    DVLOG(2) << __func__ << " Stopping timer; idle timeout hit";
    pump_codec_timer_.Stop();
  } else if (!pump_codec_timer_.IsRunning()) {
    pump_codec_timer_.Start(FROM_HERE, kPollingPeriod,
                            base::Bind(&MediaCodecVideoDecoder::PumpCodec,
                                       base::Unretained(this), false));
  }
}

bool MediaCodecVideoDecoder::QueueInput() {
  DVLOG(4) << __func__;
  if (state_ == State::kError || state_ == State::kWaitingForCodec ||
      state_ == State::kWaitingForKey || state_ == State::kBeforeSurfaceInit ||
      state_ == State::kSurfaceDestroyed) {
    return false;
  }

  // Flush the codec when there are no unrendered codec buffers, but decodes
  // pending. This lets us avoid unbacking any frames when we flush, but only
  // flush when we have more frames to decode. Without waiting for pending
  // decodes we would create a new codec at the end of playback on devices that
  // need the flush workaround.
  if (codec_->IsDrained()) {
    if (!codec_->HasValidCodecOutputBuffers() && !pending_decodes_.empty())
      FlushCodec();
    return false;
  }

  if (pending_decodes_.empty())
    return false;

  int input_buffer = -1;
  MediaCodecStatus status = codec_->DequeueInputBuffer(&input_buffer);
  if (status == MEDIA_CODEC_ERROR) {
    DVLOG(1) << "DequeueInputBuffer() error";
    HandleError();
    return false;
  } else if (status == MEDIA_CODEC_TRY_AGAIN_LATER) {
    return false;
  }
  DCHECK(status == MEDIA_CODEC_OK);
  DCHECK_GE(input_buffer, 0);

  PendingDecode pending_decode = std::move(pending_decodes_.front());
  pending_decodes_.pop_front();

  if (pending_decode.buffer->end_of_stream()) {
    DVLOG(2) << ": QueueEOS()";
    codec_->QueueEOS(input_buffer);
    eos_decode_cb_ = std::move(pending_decode.decode_cb);
    return true;
  }

  MediaCodecStatus queue_status = codec_->QueueInputBuffer(
      input_buffer, pending_decode.buffer->data(),
      pending_decode.buffer->data_size(), pending_decode.buffer->timestamp());
  DVLOG(2) << ": QueueInputBuffer(pts="
           << pending_decode.buffer->timestamp().InMilliseconds()
           << ") status=" << queue_status;

  DCHECK_NE(queue_status, MEDIA_CODEC_NO_KEY)
      << "Encrypted support not yet implemented";
  if (queue_status != MEDIA_CODEC_OK) {
    pending_decode.decode_cb.Run(DecodeStatus::DECODE_ERROR);
    HandleError();
    return false;
  }

  pending_decode.decode_cb.Run(DecodeStatus::OK);
  return true;
}

bool MediaCodecVideoDecoder::DequeueOutput() {
  DVLOG(4) << __func__;
  if (state_ == State::kError || state_ == State::kWaitingForCodec ||
      state_ == State::kWaitingForKey || state_ == State::kBeforeSurfaceInit ||
      state_ == State::kSurfaceDestroyed) {
    return false;
  }

  // Transition to the incoming surface when there are no unrendered codec
  // buffers. This is so we can ensure we create the right kind of VideoFrame
  // for the current surface.
  if (incoming_surface_) {
    if (codec_->HasValidCodecOutputBuffers())
      return false;
    TransitionToIncomingSurface();
    return true;
  }

  base::TimeDelta presentation_time;
  bool eos = false;
  std::unique_ptr<CodecOutputBuffer> output_buffer;
  MediaCodecStatus status =
      codec_->DequeueOutputBuffer(&presentation_time, &eos, &output_buffer);
  if (status == MEDIA_CODEC_ERROR) {
    DVLOG(1) << "DequeueOutputBuffer() error";
    HandleError();
    if (drain_type_)
      OnCodecDrained();
    return false;
  } else if (status == MEDIA_CODEC_TRY_AGAIN_LATER) {
    return false;
  }
  DCHECK(status == MEDIA_CODEC_OK);

  if (eos) {
    DVLOG(2) << "DequeueOutputBuffer(): EOS";
    if (eos_decode_cb_) {
      // Note: It's important to post |eos_decode_cb_| through the gpu task
      // runner to ensure it follows all previous outputs.
      gpu_task_runner_->PostTaskAndReply(
          FROM_HERE, base::Bind(&base::DoNothing),
          base::Bind(base::ResetAndReturn(&eos_decode_cb_), DecodeStatus::OK));
    }
    if (drain_type_)
      OnCodecDrained();
    // We don't want to flush the drained codec immediately if !|drain_type_|
    // because it might be backing unrendered frames near EOS. Instead we'll
    // flush it after all outstanding buffers are released.
    return false;
  }

  DVLOG(2) << "DequeueOutputBuffer(): pts="
           << presentation_time.InMilliseconds();

  // If we're draining for reset or destroy we can discard |output_buffer|
  // without rendering it.
  if (drain_type_)
    return true;

  video_frame_factory_->CreateVideoFrame(
      std::move(output_buffer), surface_texture_, presentation_time,
      decoder_config_.natural_size(), output_cb_);
  return true;
}

void MediaCodecVideoDecoder::Reset(const base::Closure& closure) {
  DVLOG(2) << __func__;
  ClearPendingDecodes(DecodeStatus::ABORTED);
  if (!codec_) {
    closure.Run();
    return;
  }
  reset_cb_ = std::move(closure);
  StartDrainingCodec(DrainType::kForReset);
}

void MediaCodecVideoDecoder::StartDrainingCodec(DrainType drain_type) {
  DVLOG(2) << __func__;
  DCHECK(pending_decodes_.empty());
  // It's okay if there's already a drain ongoing. We'll only enqueue an EOS if
  // the codec isn't already draining.
  drain_type_ = drain_type;

  // Skip the drain if possible. Only VP8 codecs need draining because
  // they can hang in release() or flush() otherwise
  // (http://crbug.com/598963).
  if (!codec_ || decoder_config_.codec() != kCodecVP8 || codec_->IsFlushed() ||
      codec_->IsDrained()) {
    OnCodecDrained();
    return;
  }

  // Queue EOS if the codec isn't already processing one.
  if (!codec_->IsDraining())
    pending_decodes_.push_back(PendingDecode::CreateEos());
}

void MediaCodecVideoDecoder::OnCodecDrained() {
  DVLOG(2) << __func__;
  if (drain_type_ == DrainType::kForDestroy) {
    // TODO(watk): Delete |this|.
    return;
  }

  DCHECK(drain_type_ == DrainType::kForReset);
  base::ResetAndReturn(&reset_cb_).Run();
  if (state_ == State::kSurfaceDestroyed || state_ == State::kError)
    return;
  FlushCodec();
  drain_type_.reset();
}

void MediaCodecVideoDecoder::HandleError() {
  DVLOG(2) << __func__;
  state_ = State::kError;
  ClearPendingDecodes(DecodeStatus::DECODE_ERROR);
}

void MediaCodecVideoDecoder::ClearPendingDecodes(DecodeStatus status) {
  for (auto& pending_decode : pending_decodes_)
    pending_decode.decode_cb.Run(status);
  pending_decodes_.clear();
  if (eos_decode_cb_)
    base::ResetAndReturn(&eos_decode_cb_).Run(status);
}

void MediaCodecVideoDecoder::ReleaseCodec() {
  if (!codec_)
    return;
  codec_allocator_->ReleaseMediaCodec(codec_->TakeCodec(),
                                      codec_config_->surface_bundle);
  codec_ = nullptr;
}

void MediaCodecVideoDecoder::ReleaseCodecAndBundle() {
  ReleaseCodec();
  codec_config_->surface_bundle = nullptr;
}

AndroidOverlayFactoryCB MediaCodecVideoDecoder::CreateOverlayFactoryCb() {
  if (overlay_info_.HasValidSurfaceId()) {
    return base::Bind(&ContentVideoViewOverlay::Create,
                      overlay_info_.surface_id);
  } else if (overlay_info_.HasValidRoutingToken() && overlay_factory_cb_) {
    return base::Bind(overlay_factory_cb_, *overlay_info_.routing_token);
  }
  return AndroidOverlayFactoryCB();
}

std::string MediaCodecVideoDecoder::GetDisplayName() const {
  return "MediaCodecVideoDecoder";
}

bool MediaCodecVideoDecoder::NeedsBitstreamConversion() const {
  return true;
}

bool MediaCodecVideoDecoder::CanReadWithoutStalling() const {
  // MediaCodec gives us no indication that it will stop producing outputs
  // until we provide more inputs or release output buffers back to it, so
  // we have to always return false.
  // TODO(watk): This puts all MCVD playbacks into low delay mode (i.e., the
  // renderer won't try to preroll). Ideally we'd be smarter about
  // this and attempt preroll but be able to give up if we can't produce
  // enough frames.
  return false;
}

int MediaCodecVideoDecoder::GetMaxDecodeRequests() const {
  // We indicate that we're done decoding a frame as soon as we submit it to
  // MediaCodec so the number of parallel decode requests just sets the upper
  // limit of the size of our pending decode queue.
  return 2;
}

}  // namespace media
