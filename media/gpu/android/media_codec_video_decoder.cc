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

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/base/android/extract_sps_and_pps.h"
#endif

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

CodecAllocatorAdapter::CodecAllocatorAdapter() : weak_factory(this) {}
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
    std::unique_ptr<VideoFrameFactory> video_frame_factory,
    std::unique_ptr<service_manager::ServiceContextRef> context_ref)
    : state_(State::kBeforeSurfaceInit),
      lazy_init_pending_(true),
      waiting_for_key_(false),
      reset_generation_(0),
      output_cb_(output_cb),
      gpu_task_runner_(gpu_task_runner),
      get_stub_cb_(get_stub_cb),
      codec_allocator_(codec_allocator),
      surface_chooser_(std::move(surface_chooser)),
      video_frame_factory_(std::move(video_frame_factory)),
      device_info_(device_info),
      context_ref_(std::move(context_ref)),
      weak_factory_(this) {
  DVLOG(2) << __func__;
}

MediaCodecVideoDecoder::~MediaCodecVideoDecoder() {
  DVLOG(2) << __func__;
  ReleaseCodec();
  codec_allocator_->StopThread(&codec_allocator_adapter_);
}

void MediaCodecVideoDecoder::Destroy() {
  DVLOG(2) << __func__;
  // Mojo callbacks require that they're run before destruction.
  if (reset_cb_)
    reset_cb_.Run();
  ClearPendingDecodes(DecodeStatus::ABORTED);
  StartDrainingCodec(DrainType::kForDestroy);
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
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  // We pass the SPS and PPS because it makes MediaCodec initialization
  // more reliable (http://crbug.com/649185).
  if (config.codec() == kCodecH264) {
    ExtractSpsAndPps(config.extra_data(), &codec_config_->csd0,
                     &codec_config_->csd1);
  }
#endif

  // We defer initialization of the Surface and MediaCodec until we
  // receive a Decode() call to avoid consuming those resources in cases where
  // we'll be destructed before getting a Decode(). Failure to initialize those
  // resources will be reported as a decode error on the first decode.
  // TODO(watk): Initialize the CDM before calling init_cb.
  init_cb.Run(true);
}

void MediaCodecVideoDecoder::OnKeyAdded() {
  DVLOG(2) << __func__;
  waiting_for_key_ = false;
  ManageTimer(true);
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
    EnterTerminalState(State::kError);
    return;
  }
  surface_texture_bundle_ = new AVDASurfaceBundle(std::move(surface_texture));
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
                : surface_texture_bundle_;
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
    incoming_surface_ = nullptr;
    return;
  }

  incoming_surface_ = overlay ? new AVDASurfaceBundle(std::move(overlay))
                              : surface_texture_bundle_;
}

void MediaCodecVideoDecoder::OnSurfaceDestroyed(AndroidOverlay* overlay) {
  DVLOG(2) << __func__;

  // If there is a pending transition to the overlay, cancel it.
  if (incoming_surface_ && incoming_surface_->overlay.get() == overlay) {
    incoming_surface_ = nullptr;
    return;
  }

  // If we've already stopped using the overlay, ignore it.
  if (!codec_config_->surface_bundle ||
      codec_config_->surface_bundle->overlay.get() != overlay) {
    return;
  }

  // If we can't do the transition, enter state kSurfaceDestroyed.
  // TODO(watk): If setOutputSurface() is available we're supposed to
  // transparently handle surface transitions, however we don't handle them
  // while codec creation is in progress. It should be handled gracefully
  // by allocating a new codec.
  if (!device_info_->IsSetOutputSurfaceSupported() ||
      state_ == State::kWaitingForCodec) {
    EnterTerminalState(State::kSurfaceDestroyed);
    return;
  }

  // We're using the destroyed overlay so try to transition away from it
  // (defaulting to a SurfaceTexture).
  if (!incoming_surface_)
    incoming_surface_ = surface_texture_bundle_;
  TransitionToIncomingSurface();
}

void MediaCodecVideoDecoder::TransitionToIncomingSurface() {
  DVLOG(2) << __func__;
  DCHECK(incoming_surface_);
  DCHECK(codec_);
  auto surface_bundle = std::move(incoming_surface_);
  if (codec_->SetSurface(surface_bundle->GetJavaSurface()) == MEDIA_CODEC_OK)
    codec_config_->surface_bundle = std::move(surface_bundle);
  else
    EnterTerminalState(State::kError);
}

void MediaCodecVideoDecoder::CreateCodec() {
  DCHECK(!codec_);
  state_ = State::kWaitingForCodec;
  codec_allocator_adapter_.codec_created_cb = base::Bind(
      &MediaCodecVideoDecoder::OnCodecCreated, weak_factory_.GetWeakPtr());
  codec_allocator_->CreateMediaCodecAsync(
      codec_allocator_adapter_.weak_factory.GetWeakPtr(), codec_config_);
}

void MediaCodecVideoDecoder::OnCodecCreated(
    std::unique_ptr<MediaCodecBridge> codec) {
  DCHECK_EQ(state_, State::kWaitingForCodec);
  DCHECK(!codec_);

  if (!codec) {
    EnterTerminalState(State::kError);
    return;
  }

  state_ = State::kOk;
  codec_ = base::MakeUnique<CodecWrapper>(
      std::move(codec),
      BindToCurrentLoop(base::Bind(&MediaCodecVideoDecoder::ManageTimer,
                                   weak_factory_.GetWeakPtr(), true)));
  ManageTimer(true);
}

void MediaCodecVideoDecoder::Decode(const scoped_refptr<DecoderBuffer>& buffer,
                                    const DecodeCB& decode_cb) {
  DVLOG(2) << __func__ << buffer->AsHumanReadableString();
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
  if (!codec_ || codec_->IsFlushed() || state_ == State::kSurfaceDestroyed ||
      state_ == State::kError) {
    return;
  }

  if (codec_->SupportsFlush(device_info_)) {
    DVLOG(2) << "Flushing codec";
    if (!codec_->Flush()) {
      EnterTerminalState(State::kError);
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
    // Draining for destroy can no longer proceed if the timer is stopping,
    // because no more Decode() calls can be made, so complete it now to avoid
    // leaking |this|.
    if (drain_type_ == DrainType::kForDestroy)
      OnCodecDrained();
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
      state_ == State::kBeforeSurfaceInit ||
      state_ == State::kSurfaceDestroyed || waiting_for_key_) {
    return false;
  }

  // If the codec is drained, flush it when there is a pending decode and no
  // unreleased output buffers. This lets us avoid both unbacking frames when we
  // flush, and flushing unnecessarily, like at EOS.
  if (codec_->IsDrained()) {
    if (!codec_->HasUnreleasedOutputBuffers() && !pending_decodes_.empty()) {
      FlushCodec();
      return true;
    }
    return false;
  }

  if (pending_decodes_.empty())
    return false;

  PendingDecode& pending_decode = pending_decodes_.front();
  MediaCodecStatus status = codec_->QueueInputBuffer(
      *pending_decode.buffer, decoder_config_.encryption_scheme());
  if (status == MEDIA_CODEC_TRY_AGAIN_LATER)
    return false;

  DVLOG(2) << "QueueInput(" << pending_decode.buffer->AsHumanReadableString()
           << ") status=" << status;
  if (status == MEDIA_CODEC_ERROR) {
    EnterTerminalState(State::kError);
    return false;
  }
  if (status == MEDIA_CODEC_NO_KEY) {
    // Retry when a key is added.
    waiting_for_key_ = true;
    return false;
  }

  DCHECK_EQ(status, MEDIA_CODEC_OK);
  if (pending_decode.buffer->end_of_stream()) {
    // The VideoDecoder interface requires that the EOS DecodeCB is called after
    // all decodes before it are delivered, so we have to save it and call it
    // when the EOS is dequeued.
    DCHECK(!eos_decode_cb_);
    eos_decode_cb_ = std::move(pending_decode.decode_cb);
  } else {
    pending_decode.decode_cb.Run(DecodeStatus::OK);
  }
  pending_decodes_.pop_front();
  return true;
}

bool MediaCodecVideoDecoder::DequeueOutput() {
  DVLOG(4) << __func__;
  if (state_ == State::kError || state_ == State::kWaitingForCodec ||
      state_ == State::kBeforeSurfaceInit ||
      state_ == State::kSurfaceDestroyed || waiting_for_key_) {
    return false;
  }

  // If a surface transition is pending, wait for all outstanding buffers to be
  // released before doing the transition. This is necessary because the
  // VideoFrames corresponding to these buffers have metadata flags specific to
  // the surface type, and changing the surface before they're rendered would
  // invalidate them.
  if (incoming_surface_) {
    if (!codec_->HasUnreleasedOutputBuffers()) {
      TransitionToIncomingSurface();
      return true;
    }
    return false;
  }

  base::TimeDelta presentation_time;
  bool eos = false;
  std::unique_ptr<CodecOutputBuffer> output_buffer;
  MediaCodecStatus status =
      codec_->DequeueOutputBuffer(&presentation_time, &eos, &output_buffer);
  if (status == MEDIA_CODEC_ERROR) {
    DVLOG(1) << "DequeueOutputBuffer() error";
    EnterTerminalState(State::kError);
    return false;
  } else if (status == MEDIA_CODEC_TRY_AGAIN_LATER) {
    return false;
  }
  DCHECK_EQ(status, MEDIA_CODEC_OK);

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
      std::move(output_buffer), surface_texture_bundle_->surface_texture,
      presentation_time, decoder_config_.natural_size(),
      base::Bind(&MediaCodecVideoDecoder::ForwardVideoFrame,
                 weak_factory_.GetWeakPtr(), reset_generation_));
  return true;
}

void MediaCodecVideoDecoder::ForwardVideoFrame(
    int reset_generation,
    VideoFrameFactory::ReleaseMailboxCB release_cb,
    const scoped_refptr<VideoFrame>& frame) {
  if (reset_generation_ == reset_generation)
    output_cb_.Run(std::move(release_cb), frame);
}

void MediaCodecVideoDecoder::Reset(const base::Closure& closure) {
  DVLOG(2) << __func__;
  reset_generation_++;
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
  // TODO(watk): Strongly consider blacklisting VP8 (or specific MediaCodecs)
  // instead. Draining is responsible for a lot of complexity.
  if (decoder_config_.codec() != kCodecVP8 || !codec_ || codec_->IsFlushed() ||
      codec_->IsDrained()) {
    OnCodecDrained();
    return;
  }

  // Queue EOS if the codec isn't already processing one.
  if (!codec_->IsDraining())
    pending_decodes_.push_back(PendingDecode::CreateEos());

  // We can safely invalidate outstanding buffers for both types of drain, and
  // doing so can only make the drain complete quicker.
  codec_->DiscardOutputBuffers();
  PumpCodec(true);
}

void MediaCodecVideoDecoder::OnCodecDrained() {
  DVLOG(2) << __func__;
  DrainType drain_type = *drain_type_;
  drain_type_.reset();

  if (drain_type == DrainType::kForDestroy) {
    // Post the delete in case the caller uses |this| after we return.
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
    return;
  }

  base::ResetAndReturn(&reset_cb_).Run();
  FlushCodec();
}

void MediaCodecVideoDecoder::EnterTerminalState(State state) {
  DVLOG(2) << __func__ << static_cast<int>(state);
  DCHECK(state == State::kError || state == State::kSurfaceDestroyed);
  state_ = state;

  // Cancel pending codec creation.
  codec_allocator_adapter_.weak_factory.InvalidateWeakPtrs();
  pump_codec_timer_.Stop();
  ReleaseCodecAndBundle();
  if (state == State::kError)
    ClearPendingDecodes(DecodeStatus::DECODE_ERROR);
  if (drain_type_)
    OnCodecDrained();
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
  DCHECK(codec_config_);
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
