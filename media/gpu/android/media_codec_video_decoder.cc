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
#include "media/gpu/android/android_video_surface_chooser.h"
#include "media/gpu/android/avda_codec_allocator.h"

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
    const gpu::GpuPreferences& gpu_preferences,
    VideoFrameFactory::OutputWithReleaseMailboxCB output_cb,
    DeviceInfo* device_info,
    AVDACodecAllocator* codec_allocator,
    std::unique_ptr<AndroidVideoSurfaceChooser> surface_chooser,
    AndroidOverlayMojoFactoryCB overlay_factory_cb,
    RequestOverlayInfoCB request_overlay_info_cb,
    std::unique_ptr<VideoFrameFactory> video_frame_factory,
    std::unique_ptr<service_manager::ServiceContextRef> context_ref)
    : output_cb_(output_cb),
      codec_allocator_(codec_allocator),
      request_overlay_info_cb_(std::move(request_overlay_info_cb)),
      surface_chooser_(std::move(surface_chooser)),
      video_frame_factory_(std::move(video_frame_factory)),
      overlay_factory_cb_(std::move(overlay_factory_cb)),
      device_info_(device_info),
      enable_threaded_texture_mailboxes_(
          gpu_preferences.enable_threaded_texture_mailboxes),
      context_ref_(std::move(context_ref)),
      weak_factory_(this),
      codec_allocator_weak_factory_(this) {
  DVLOG(2) << __func__;
  surface_chooser_->SetClientCallbacks(
      base::Bind(&MediaCodecVideoDecoder::OnSurfaceChosen,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&MediaCodecVideoDecoder::OnSurfaceChosen,
                 weak_factory_.GetWeakPtr(), nullptr));
}

MediaCodecVideoDecoder::~MediaCodecVideoDecoder() {
  DVLOG(2) << __func__;
  ReleaseCodec();
  codec_allocator_->StopThread(this);
}

void MediaCodecVideoDecoder::Destroy() {
  DVLOG(2) << __func__;
  // Mojo callbacks require that they're run before destruction.
  if (reset_cb_)
    std::move(reset_cb_).Run();
  // Cancel callbacks we no longer want.
  codec_allocator_weak_factory_.InvalidateWeakPtrs();
  CancelPendingDecodes(DecodeStatus::ABORTED);
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

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (config.codec() == kCodecH264)
    ExtractSpsAndPps(config.extra_data(), &csd0_, &csd1_);
#endif

  // Do the rest of the initialization lazily on the first decode.
  // TODO(watk): Add CDM Support.
  DCHECK(!cdm_context);
  init_cb.Run(true);
}

void MediaCodecVideoDecoder::OnKeyAdded() {
  DVLOG(2) << __func__;
  waiting_for_key_ = false;
  StartTimer();
}

void MediaCodecVideoDecoder::StartLazyInit() {
  DVLOG(2) << __func__;
  lazy_init_pending_ = false;
  codec_allocator_->StartThread(this);
  video_frame_factory_->Initialize(
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

  // Overlays are disabled when |enable_threaded_texture_mailboxes| is true
  // (http://crbug.com/582170).
  if (enable_threaded_texture_mailboxes_ ||
      !device_info_->SupportsOverlaySurfaces()) {
    OnSurfaceChosen(nullptr);
    return;
  }

  // Request OverlayInfo updates. Initialization continues on the first one.
  bool restart_for_transitions = !device_info_->IsSetOutputSurfaceSupported();
  std::move(request_overlay_info_cb_)
      .Run(restart_for_transitions,
           base::Bind(&MediaCodecVideoDecoder::OnOverlayInfoChanged,
                      weak_factory_.GetWeakPtr()));
}

void MediaCodecVideoDecoder::OnOverlayInfoChanged(
    const OverlayInfo& overlay_info) {
  DVLOG(2) << __func__;
  DCHECK(device_info_->SupportsOverlaySurfaces());
  DCHECK(!enable_threaded_texture_mailboxes_);
  if (InTerminalState())
    return;

  // TODO(watk): Handle frame_hidden like AVDA. Maybe even if in a terminal
  // state.
  // TODO(watk): Incorporate the other chooser_state_ signals.

  bool overlay_changed = !overlay_info_.RefersToSameOverlayAs(overlay_info);
  overlay_info_ = overlay_info;
  chooser_state_.is_fullscreen = overlay_info_.is_fullscreen;
  chooser_state_.is_frame_hidden = overlay_info_.is_frame_hidden;
  surface_chooser_->UpdateState(
      overlay_changed ? base::make_optional(CreateOverlayFactoryCb())
                      : base::nullopt,
      chooser_state_);
}

void MediaCodecVideoDecoder::OnSurfaceChosen(
    std::unique_ptr<AndroidOverlay> overlay) {
  DVLOG(2) << __func__;
  DCHECK(state_ == State::kInitializing ||
         device_info_->IsSetOutputSurfaceSupported());

  if (overlay) {
    overlay->AddSurfaceDestroyedCallback(
        base::Bind(&MediaCodecVideoDecoder::OnSurfaceDestroyed,
                   weak_factory_.GetWeakPtr()));
    target_surface_bundle_ = new AVDASurfaceBundle(std::move(overlay));
  } else {
    target_surface_bundle_ = surface_texture_bundle_;
  }

  // If we were waiting for our first surface during initialization, then
  // proceed to create a codec.
  if (state_ == State::kInitializing) {
    state_ = State::kRunning;
    CreateCodec();
  }
}

void MediaCodecVideoDecoder::OnSurfaceDestroyed(AndroidOverlay* overlay) {
  DVLOG(2) << __func__;
  DCHECK_NE(state_, State::kInitializing);

  // If SetOutputSurface() is not supported we only ever observe destruction of
  // a single overlay so this must be the one we're using. In this case it's
  // the responsibility of our consumer to destroy us for surface transitions.
  // TODO(liberato): This might not be true for L1 / L3, since our caller has
  // no idea that this has happened.  We should unback the frames here.
  if (!device_info_->IsSetOutputSurfaceSupported()) {
    EnterTerminalState(State::kSurfaceDestroyed);
    return;
  }

  // Reset the target bundle if it is the one being destroyed.
  if (target_surface_bundle_->overlay.get() == overlay)
    target_surface_bundle_ = surface_texture_bundle_;

  // Transition the codec away from the overlay if necessary.
  if (SurfaceTransitionPending())
    TransitionToTargetSurface();
}

bool MediaCodecVideoDecoder::SurfaceTransitionPending() {
  return codec_ && codec_->SurfaceBundle() != target_surface_bundle_;
}

void MediaCodecVideoDecoder::TransitionToTargetSurface() {
  DVLOG(2) << __func__;
  DCHECK(SurfaceTransitionPending());
  DCHECK(device_info_->IsSetOutputSurfaceSupported());

  if (!codec_->SetSurface(target_surface_bundle_))
    EnterTerminalState(State::kError);
}

void MediaCodecVideoDecoder::CreateCodec() {
  DCHECK(!codec_);
  DCHECK(target_surface_bundle_);
  DCHECK_EQ(state_, State::kRunning);

  scoped_refptr<CodecConfig> config = new CodecConfig();
  config->codec = decoder_config_.codec();
  // TODO(watk): Set |requires_secure_codec| correctly using
  // MediaDrmBridgeCdmContext::MediaCryptoReadyCB.
  config->requires_secure_codec = decoder_config_.is_encrypted();
  config->initial_expected_coded_size = decoder_config_.coded_size();
  config->surface_bundle = target_surface_bundle_;
  codec_allocator_->CreateMediaCodecAsync(
      codec_allocator_weak_factory_.GetWeakPtr(), std::move(config));
}

void MediaCodecVideoDecoder::OnCodecConfigured(
    std::unique_ptr<MediaCodecBridge> codec,
    scoped_refptr<AVDASurfaceBundle> surface_bundle) {
  DCHECK(!codec_);
  DCHECK_EQ(state_, State::kRunning);

  if (!codec) {
    EnterTerminalState(State::kError);
    return;
  }
  codec_ = base::MakeUnique<CodecWrapper>(
      CodecSurfacePair(std::move(codec), std::move(surface_bundle)),
      BindToCurrentLoop(base::Bind(&MediaCodecVideoDecoder::StartTimer,
                                   weak_factory_.GetWeakPtr())));

  // If the target surface changed while codec creation was in progress,
  // transition to it immediately.
  // Note: this can only happen if we support SetOutputSurface() because if we
  // don't OnSurfaceDestroyed() cancels codec creations, and
  // |surface_chooser_| doesn't change the target surface.
  if (SurfaceTransitionPending())
    TransitionToTargetSurface();

  StartTimer();
}

void MediaCodecVideoDecoder::Decode(const scoped_refptr<DecoderBuffer>& buffer,
                                    const DecodeCB& decode_cb) {
  DVLOG(2) << __func__ << ": " << buffer->AsHumanReadableString();
  if (state_ == State::kError) {
    decode_cb.Run(DecodeStatus::DECODE_ERROR);
    return;
  }
  pending_decodes_.emplace_back(buffer, std::move(decode_cb));

  if (state_ == State::kInitializing) {
    if (lazy_init_pending_)
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
    if (!codec_->Flush())
      EnterTerminalState(State::kError);
  } else {
    DVLOG(2) << "flush() workaround: creating a new codec";
    // Release the codec and create a new one.
    // Note: we may end up with two codecs attached to the same surface if the
    // release hangs on one thread and create proceeds on another. This will
    // result in an error, letting the user retry the playback. The alternative
    // of waiting for the release risks hanging the playback forever.
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

  if (did_work || force_start_timer)
    StartTimer();
  else
    StopTimerIfIdle();
}

void MediaCodecVideoDecoder::StartTimer() {
  DVLOG(4) << __func__;
  if (state_ != State::kRunning)
    return;

  idle_timer_ = base::ElapsedTimer();

  // Poll at 10ms somewhat arbitrarily.
  // TODO: Don't poll on new devices; use the callback API.
  // TODO: Experiment with this number to save power. Since we already pump the
  // codec in response to receiving a decode and output buffer release, polling
  // at this frequency is likely overkill in the steady state.
  const auto kPollingPeriod = base::TimeDelta::FromMilliseconds(10);
  if (!pump_codec_timer_.IsRunning()) {
    pump_codec_timer_.Start(FROM_HERE, kPollingPeriod,
                            base::Bind(&MediaCodecVideoDecoder::PumpCodec,
                                       base::Unretained(this), false));
  }
}

void MediaCodecVideoDecoder::StopTimerIfIdle() {
  DVLOG(4) << __func__;
  // Stop the timer if we've been idle for one second. Chosen arbitrarily.
  const auto kTimeout = base::TimeDelta::FromSeconds(1);
  if (idle_timer_.Elapsed() > kTimeout) {
    DVLOG(2) << "Stopping timer; idle timeout hit";
    pump_codec_timer_.Stop();
    // Draining for destroy can no longer proceed if the timer is stopping,
    // because no more Decode() calls can be made, so complete it now to avoid
    // leaking |this|.
    if (drain_type_ == DrainType::kForDestroy)
      OnCodecDrained();
  }
}

bool MediaCodecVideoDecoder::QueueInput() {
  DVLOG(4) << __func__;
  if (!codec_ || waiting_for_key_)
    return false;

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
  auto status = codec_->QueueInputBuffer(*pending_decode.buffer,
                                         decoder_config_.encryption_scheme());
  DVLOG((status == CodecWrapper::QueueStatus::kTryAgainLater ? 3 : 2))
      << "QueueInput(" << pending_decode.buffer->AsHumanReadableString()
      << ") status=" << static_cast<int>(status);

  switch (status) {
    case CodecWrapper::QueueStatus::kOk:
      break;
    case CodecWrapper::QueueStatus::kTryAgainLater:
      return false;
    case CodecWrapper::QueueStatus::kNoKey:
      // Retry when a key is added.
      waiting_for_key_ = true;
      return false;
    case CodecWrapper::QueueStatus::kError:
      EnterTerminalState(State::kError);
      return false;
  }

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
  if (!codec_ || codec_->IsDrained() || waiting_for_key_)
    return false;

  // If a surface transition is pending, wait for all outstanding buffers to be
  // released before doing the transition. This is necessary because the
  // VideoFrames corresponding to these buffers have metadata flags specific to
  // the surface type, and changing the surface before they're rendered would
  // invalidate them.
  if (SurfaceTransitionPending()) {
    if (!codec_->HasUnreleasedOutputBuffers()) {
      TransitionToTargetSurface();
      return true;
    }
    return false;
  }

  base::TimeDelta presentation_time;
  bool eos = false;
  std::unique_ptr<CodecOutputBuffer> output_buffer;
  auto status =
      codec_->DequeueOutputBuffer(&presentation_time, &eos, &output_buffer);
  switch (status) {
    case CodecWrapper::DequeueStatus::kOk:
      break;
    case CodecWrapper::DequeueStatus::kTryAgainLater:
      return false;
    case CodecWrapper::DequeueStatus::kError:
      DVLOG(1) << "DequeueOutputBuffer() error";
      EnterTerminalState(State::kError);
      return false;
  }
  DVLOG(2) << "DequeueOutputBuffer(): pts="
           << (eos ? "EOS"
                   : std::to_string(presentation_time.InMilliseconds()));

  if (eos) {
    if (eos_decode_cb_) {
      // Schedule the EOS DecodeCB to run after all previous frames.
      video_frame_factory_->RunAfterPendingVideoFrames(
          base::Bind(&MediaCodecVideoDecoder::RunEosDecodeCb,
                     weak_factory_.GetWeakPtr(), reset_generation_));
    }
    if (drain_type_)
      OnCodecDrained();
    // We don't flush the drained codec immediately because it might be
    // backing unrendered frames near EOS. It's flushed lazily in QueueInput().
    return false;
  }

  // If we're draining for reset or destroy we can discard |output_buffer|
  // without rendering it.
  if (drain_type_)
    return true;

  video_frame_factory_->CreateVideoFrame(
      std::move(output_buffer),
      codec_->SurfaceBundle()->overlay
          ? nullptr
          : surface_texture_bundle_->surface_texture,
      presentation_time, decoder_config_.natural_size(),
      CreatePromotionHintCB(),
      base::Bind(&MediaCodecVideoDecoder::ForwardVideoFrame,
                 weak_factory_.GetWeakPtr(), reset_generation_));
  return true;
}

void MediaCodecVideoDecoder::RunEosDecodeCb(int reset_generation) {
  // Both of the following conditions are necessary because:
  //  * In an error state, the reset generations will match but |eos_decode_cb_|
  //    will be aborted.
  //  * After a Reset(), the reset generations won't match, but we might already
  //    have a new |eos_decode_cb_| for the new generation.
  if (reset_generation == reset_generation_ && eos_decode_cb_)
    std::move(eos_decode_cb_).Run(DecodeStatus::OK);
}

void MediaCodecVideoDecoder::ForwardVideoFrame(
    int reset_generation,
    VideoFrameFactory::ReleaseMailboxCB release_cb,
    const scoped_refptr<VideoFrame>& frame) {
  if (reset_generation == reset_generation_)
    output_cb_.Run(std::move(release_cb), frame);
}

// Our Reset() provides a slightly stronger guarantee than VideoDecoder does.
// After |closure| runs:
// 1) no VideoFrames from before the Reset() will be output, and
// 2) no DecodeCBs (including EOS) from before the Reset() will be run.
void MediaCodecVideoDecoder::Reset(const base::Closure& closure) {
  DVLOG(2) << __func__;
  DCHECK(!reset_cb_);
  reset_generation_++;
  reset_cb_ = std::move(closure);
  CancelPendingDecodes(DecodeStatus::ABORTED);
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

  std::move(reset_cb_).Run();
  FlushCodec();
}

void MediaCodecVideoDecoder::EnterTerminalState(State state) {
  DVLOG(2) << __func__ << " " << static_cast<int>(state);

  state_ = state;
  DCHECK(InTerminalState());

  // Cancel pending codec creation.
  codec_allocator_weak_factory_.InvalidateWeakPtrs();
  pump_codec_timer_.Stop();
  ReleaseCodec();
  target_surface_bundle_ = nullptr;
  surface_texture_bundle_ = nullptr;
  if (state == State::kError)
    CancelPendingDecodes(DecodeStatus::DECODE_ERROR);
  if (drain_type_)
    OnCodecDrained();
}

bool MediaCodecVideoDecoder::InTerminalState() {
  return state_ == State::kSurfaceDestroyed || state_ == State::kError;
}

void MediaCodecVideoDecoder::CancelPendingDecodes(DecodeStatus status) {
  for (auto& pending_decode : pending_decodes_)
    pending_decode.decode_cb.Run(status);
  pending_decodes_.clear();
  if (eos_decode_cb_)
    std::move(eos_decode_cb_).Run(status);
}

void MediaCodecVideoDecoder::ReleaseCodec() {
  if (!codec_)
    return;
  auto pair = codec_->TakeCodecSurfacePair();
  codec_ = nullptr;
  codec_allocator_->ReleaseMediaCodec(std::move(pair.first),
                                      std::move(pair.second));
}

AndroidOverlayFactoryCB MediaCodecVideoDecoder::CreateOverlayFactoryCb() {
  DCHECK(!overlay_info_.HasValidSurfaceId());
  if (!overlay_factory_cb_ || !overlay_info_.HasValidRoutingToken())
    return AndroidOverlayFactoryCB();

  // This wrapper forwards its arguments and clones a context ref on each call.
  auto wrapper = [](AndroidOverlayMojoFactoryCB overlay_factory_cb,
                    service_manager::ServiceContextRef* context_ref,
                    base::UnguessableToken routing_token,
                    AndroidOverlayConfig config) {
    return overlay_factory_cb.Run(context_ref->Clone(),
                                  std::move(routing_token), std::move(config));
  };

  // Pass ownership of a new context ref into the callback.
  return base::Bind(wrapper, overlay_factory_cb_,
                    base::Owned(context_ref_->Clone().release()),
                    *overlay_info_.routing_token);
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

PromotionHintAggregator::NotifyPromotionHintCB
MediaCodecVideoDecoder::CreatePromotionHintCB() const {
  // Right now, we don't request promotion hints.  This is only used by SOP.
  // While we could simplify it a bit, this is the general form that we'll use
  // when handling promotion hints.

  // TODO(liberato): Keeping the surface bundle around as long as the images
  // doesn't work so well if the surface is destroyed.  In that case, the right
  // thing to do is (a) wait for any codec to quit using the surface, and (b)
  // clear |overlay| out of the surface bundle.
  // Having the surface bundle register for destruction callbacks, instead of
  // us, makes sense.
  return BindToCurrentLoop(base::BindRepeating(
      [](scoped_refptr<AVDASurfaceBundle> surface_bundle,
         PromotionHintAggregator::Hint hint) {
        // If we're promotable, and we have a surface bundle, then also
        // position the overlay.  We could do this even if the overlay is
        // not promotable, but it wouldn't have any visible effect.
        if (hint.is_promotable && surface_bundle)
          surface_bundle->overlay->ScheduleLayout(hint.screen_rect);
      },
      codec_->SurfaceBundle()));
}

}  // namespace media
