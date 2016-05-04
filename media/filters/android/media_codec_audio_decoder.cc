// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/android/media_codec_audio_decoder.h"

#include "base/android/build_info.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/thread_task_runner_handle.h"
#include "media/base/android/sdk_media_codec_bridge.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/timestamp_constants.h"

namespace media {

namespace {

// Android MediaCodec can only output 16bit PCM audio.
const int kBytesPerOutputSample = 2;

// Valid MediaCodec buffer indexes are zero or positive.
const int kInvalidBufferIndex = -1;

inline const base::TimeDelta DecodePollDelay() {
  return base::TimeDelta::FromMilliseconds(10);
}

inline const base::TimeDelta NoWaitTimeout() {
  return base::TimeDelta::FromMicroseconds(0);
}

inline const base::TimeDelta IdleTimerTimeout() {
  return base::TimeDelta::FromSeconds(1);
}

inline int GetChannelCount(const AudioDecoderConfig& config) {
  return ChannelLayoutToChannelCount(config.channel_layout());
}

std::unique_ptr<MediaCodecBridge> CreateMediaCodec(
    const AudioDecoderConfig& config,
    jobject media_crypto) {
  DVLOG(1) << __FUNCTION__ << ": config:" << config.AsHumanReadableString();

  std::unique_ptr<AudioCodecBridge> audio_codec_bridge(
      AudioCodecBridge::Create(config.codec()));
  if (!audio_codec_bridge) {
    DVLOG(0) << __FUNCTION__ << " failed: cannot create AudioCodecBridge";
    return nullptr;
  }

  const bool do_play = false;  // Do not create AudioTrack object.

  if (!audio_codec_bridge->ConfigureAndStart(config, do_play, media_crypto)) {
    DVLOG(0) << __FUNCTION__ << " failed: cannot configure audio codec for "
             << config.AsHumanReadableString();
    return nullptr;
  }

  return std::move(audio_codec_bridge);
}

// Converts interleaved data into planar data and writes it to |planes|.
// The planes are populated in the order of channels in the interleaved frame.
// If |channel_count| is less than the number of available planes the extra
// destination planes will not be touched.
void SeparatePlanes(const uint8_t* interleaved_data,
                    size_t frame_count,
                    size_t bytes_per_frame,
                    size_t channel_count,
                    const std::vector<uint8_t*>& planes) {
  DCHECK(interleaved_data);
  DCHECK_LE(channel_count, planes.size());

  const uint8_t* src_frame = interleaved_data;
  for (size_t i = 0; i < frame_count; ++i, src_frame += bytes_per_frame) {
    for (size_t ch = 0; ch < channel_count; ++ch) {
      const int16_t* src_sample =
          reinterpret_cast<const int16_t*>(src_frame) + ch;
      int16_t* dst_sample = reinterpret_cast<int16_t*>(planes[ch]) + i;
      *dst_sample = *src_sample;
    }
  }
}

}  // namespace (anonymous)

MediaCodecAudioDecoder::MediaCodecAudioDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner),
      state_(STATE_UNINITIALIZED),
      channel_count_(0),
      pending_input_buf_index_(kInvalidBufferIndex),
      media_drm_bridge_cdm_context_(nullptr),
      cdm_registration_id_(0),
      weak_factory_(this) {
  DVLOG(1) << __FUNCTION__;
}

MediaCodecAudioDecoder::~MediaCodecAudioDecoder() {
  DVLOG(1) << __FUNCTION__;

  media_codec_.reset();

  if (media_drm_bridge_cdm_context_) {
    DCHECK(cdm_registration_id_);
    media_drm_bridge_cdm_context_->UnregisterPlayer(cdm_registration_id_);
  }

  ClearInputQueue(DecodeStatus::ABORTED);
}

std::string MediaCodecAudioDecoder::GetDisplayName() const {
  return "MediaCodecAudioDecoder";
}

void MediaCodecAudioDecoder::Initialize(const AudioDecoderConfig& config,
                                        CdmContext* cdm_context,
                                        const InitCB& init_cb,
                                        const OutputCB& output_cb) {
  DVLOG(1) << __FUNCTION__ << ": " << config.AsHumanReadableString();
  DCHECK_EQ(state_, STATE_UNINITIALIZED);

  InitCB bound_init_cb = BindToCurrentLoop(init_cb);

  // We can support only the codecs that AudioCodecBridge can decode.
  const bool is_codec_supported = config.codec() == kCodecVorbis ||
                                  config.codec() == kCodecAAC ||
                                  config.codec() == kCodecOpus;
  if (!is_codec_supported) {
    DVLOG(1) << "Unsuported codec " << GetCodecName(config.codec());
    bound_init_cb.Run(false);
    return;
  }

  if (config.is_encrypted() && !cdm_context) {
    NOTREACHED() << "The stream is encrypted but there is no CDM context";
    bound_init_cb.Run(false);
    return;
  }

  config_ = config;
  timestamp_helper_.reset(
      new AudioTimestampHelper(config_.samples_per_second()));
  output_cb_ = BindToCurrentLoop(output_cb);

  if (config_.is_encrypted()) {
    // Postpone initialization after MediaCrypto is available.
    // SetCdm uses init_cb in a method that's already bound to the current loop.
    SetCdm(cdm_context, init_cb);
    SetState(STATE_WAITING_FOR_MEDIA_CRYPTO);
    return;
  }

  media_codec_ = CreateMediaCodec(config_, nullptr);
  if (!media_codec_) {
    bound_init_cb.Run(false);
    return;
  }

  SetState(STATE_READY);
  bound_init_cb.Run(true);
}

void MediaCodecAudioDecoder::Decode(const scoped_refptr<DecoderBuffer>& buffer,
                                    const DecodeCB& decode_cb) {
  DecodeCB bound_decode_cb = BindToCurrentLoop(decode_cb);

  if (!buffer->end_of_stream() && buffer->timestamp() == kNoTimestamp()) {
    DVLOG(2) << __FUNCTION__ << " " << buffer->AsHumanReadableString()
             << ": no timestamp, skipping this buffer";
    bound_decode_cb.Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  if (state_ == STATE_ERROR) {
    // We get here if an error happens in DequeueOutput() or Reset().
    DVLOG(2) << __FUNCTION__ << " " << buffer->AsHumanReadableString()
             << ": Error state, returning decode error for all buffers";
    ClearInputQueue(DecodeStatus::DECODE_ERROR);
    bound_decode_cb.Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  DVLOG(2) << __FUNCTION__ << " " << buffer->AsHumanReadableString();

  DCHECK_EQ(state_, STATE_READY) << " unexpected state " << AsString(state_);

  // AudioDecoder requires that "Only one decode may be in flight at any given
  // time".
  DCHECK(input_queue_.empty());

  input_queue_.push_back(std::make_pair(buffer, bound_decode_cb));

  DoIOTask();
}

void MediaCodecAudioDecoder::Reset(const base::Closure& closure) {
  DVLOG(1) << __FUNCTION__;

  io_timer_.Stop();

  ClearInputQueue(DecodeStatus::ABORTED);

  // Flush if we can, otherwise completely recreate and reconfigure the codec.
  // Prior to JellyBean-MR2, flush() had several bugs (b/8125974, b/8347958) so
  // we have to completely destroy and recreate the codec there.
  bool success = false;
  if (state_ != STATE_ERROR && state_ != STATE_DRAINED &&
      base::android::BuildInfo::GetInstance()->sdk_int() >= 18) {
    // media_codec_->Reset() calls MediaCodec.flush().
    success = (media_codec_->Reset() == MEDIA_CODEC_OK);
  }

  if (!success) {
    media_codec_.reset();
    jobject media_crypto_obj = media_crypto_ ? media_crypto_->obj() : nullptr;
    media_codec_ = CreateMediaCodec(config_, media_crypto_obj);
    success = !!media_codec_;
  }

  // Reset AudioTimestampHelper.
  timestamp_helper_->SetBaseTimestamp(kNoTimestamp());

  SetState(success ? STATE_READY : STATE_ERROR);

  task_runner_->PostTask(FROM_HERE, closure);
}

bool MediaCodecAudioDecoder::NeedsBitstreamConversion() const {
  // An AAC stream needs to be converted as ADTS stream.
  DCHECK_NE(config_.codec(), kUnknownAudioCodec);
  return config_.codec() == kCodecAAC;
}

void MediaCodecAudioDecoder::SetCdm(CdmContext* cdm_context,
                                    const InitCB& init_cb) {
  DCHECK(cdm_context);

  // On Android platform the CdmContext must be a MediaDrmBridgeCdmContext.
  media_drm_bridge_cdm_context_ =
      static_cast<media::MediaDrmBridgeCdmContext*>(cdm_context);

  // Register CDM callbacks. The callbacks registered will be posted back to
  // this thread via BindToCurrentLoop.

  // Since |this| holds a reference to the |cdm_|, by the time the CDM is
  // destructed, UnregisterPlayer() must have been called and |this| has been
  // destructed as well. So the |cdm_unset_cb| will never have a chance to be
  // called.
  // TODO(xhwang): Remove |cdm_unset_cb| after it's not used on all platforms.
  cdm_registration_id_ = media_drm_bridge_cdm_context_->RegisterPlayer(
      media::BindToCurrentLoop(base::Bind(&MediaCodecAudioDecoder::OnKeyAdded,
                                          weak_factory_.GetWeakPtr())),
      base::Bind(&base::DoNothing));

  media_drm_bridge_cdm_context_->SetMediaCryptoReadyCB(media::BindToCurrentLoop(
      base::Bind(&MediaCodecAudioDecoder::OnMediaCryptoReady,
                 weak_factory_.GetWeakPtr(), init_cb)));
}

void MediaCodecAudioDecoder::OnMediaCryptoReady(
    const InitCB& init_cb,
    MediaDrmBridgeCdmContext::JavaObjectPtr media_crypto,
    bool /*needs_protected_surface*/) {
  DVLOG(1) << __FUNCTION__;

  DCHECK(state_ == STATE_WAITING_FOR_MEDIA_CRYPTO);

  if (!media_crypto) {
    LOG(ERROR) << "MediaCrypto is not available, can't play encrypted stream.";
    SetState(STATE_UNINITIALIZED);
    init_cb.Run(false);
    return;
  }

  DCHECK(!media_crypto->is_null());

  // We assume this is a part of the initialization process, thus MediaCodec
  // is not created yet.
  DCHECK(!media_codec_);

  // After receiving |media_crypto_| we can configure MediaCodec.

  media_codec_ = CreateMediaCodec(config_, media_crypto->obj());
  if (!media_codec_) {
    SetState(STATE_UNINITIALIZED);
    init_cb.Run(false);
    return;
  }

  media_crypto_ = std::move(media_crypto);

  SetState(STATE_READY);
  init_cb.Run(true);
}

void MediaCodecAudioDecoder::OnKeyAdded() {
  DVLOG(1) << __FUNCTION__;

  if (state_ == STATE_WAITING_FOR_KEY)
    SetState(STATE_READY);

  DoIOTask();
}

void MediaCodecAudioDecoder::DoIOTask() {
  if (state_ == STATE_ERROR)
    return;

  const bool did_input = QueueInput();
  const bool did_output = DequeueOutput();

  ManageTimer(did_input || did_output);
}

bool MediaCodecAudioDecoder::QueueInput() {
  DVLOG(2) << __FUNCTION__;

  bool did_work = false;
  while (QueueOneInputBuffer())
    did_work = true;

  return did_work;
}

bool MediaCodecAudioDecoder::QueueOneInputBuffer() {
  DVLOG(2) << __FUNCTION__;

  if (input_queue_.empty())
    return false;

  if (state_ == STATE_WAITING_FOR_KEY || state_ == STATE_DRAINING ||
      state_ == STATE_DRAINED)
    return false;

  // DequeueInputBuffer() may set STATE_ERROR.
  InputBufferInfo input_info = DequeueInputBuffer();

  if (input_info.buf_index == kInvalidBufferIndex) {
    if (state_ == STATE_ERROR)
      ClearInputQueue(DecodeStatus::DECODE_ERROR);

    return false;
  }

  // EnqueueInputBuffer() may set STATE_DRAINING, STATE_WAITING_FOR_KEY or
  // STATE_ERROR.
  bool did_work = EnqueueInputBuffer(input_info);

  switch (state_) {
    case STATE_READY: {
      const DecodeCB& decode_cb = input_queue_.front().second;
      decode_cb.Run(DecodeStatus::OK);
      input_queue_.pop_front();
    } break;

    case STATE_DRAINING:
      // After queueing EOS we need to flush decoder, i.e. receive this EOS at
      // the output, and then call corresponding decoder_cb.
      // Keep the EOS buffer in the input queue.
      break;

    case STATE_WAITING_FOR_KEY:
      // Keep trying to enqueue the same input buffer.
      // The buffer is owned by us (not the MediaCodec) and is filled with data.
      break;

    case STATE_ERROR:
      ClearInputQueue(DecodeStatus::DECODE_ERROR);
      break;

    default:
      NOTREACHED() << ": internal error, unexpected state " << AsString(state_);
      SetState(STATE_ERROR);
      ClearInputQueue(DecodeStatus::DECODE_ERROR);
      did_work = false;
      break;
  }

  return did_work;
}

MediaCodecAudioDecoder::InputBufferInfo
MediaCodecAudioDecoder::DequeueInputBuffer() {
  DVLOG(2) << __FUNCTION__;

  // Do not dequeue a new input buffer if we failed with MEDIA_CODEC_NO_KEY.
  // That status does not return the input buffer back to the pool of
  // available input buffers. We have to reuse it in QueueSecureInputBuffer().
  if (pending_input_buf_index_ != kInvalidBufferIndex) {
    InputBufferInfo result(pending_input_buf_index_, true);
    pending_input_buf_index_ = kInvalidBufferIndex;
    return result;
  }

  int input_buf_index = kInvalidBufferIndex;

  media::MediaCodecStatus status =
      media_codec_->DequeueInputBuffer(NoWaitTimeout(), &input_buf_index);
  switch (status) {
    case media::MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER:
      DVLOG(2) << __FUNCTION__ << ": MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER";
      break;

    case media::MEDIA_CODEC_ERROR:
      DVLOG(1) << __FUNCTION__ << ": MEDIA_CODEC_ERROR from DequeInputBuffer";
      SetState(STATE_ERROR);
      break;

    case media::MEDIA_CODEC_OK:
      break;

    default:
      NOTREACHED() << "Unknown DequeueInputBuffer status " << status;
      SetState(STATE_ERROR);
      break;
  }

  return InputBufferInfo(input_buf_index, false);
}

bool MediaCodecAudioDecoder::EnqueueInputBuffer(
    const InputBufferInfo& input_info) {
  DVLOG(2) << __FUNCTION__ << ": index:" << input_info.buf_index;

  DCHECK_NE(input_info.buf_index, kInvalidBufferIndex);

  scoped_refptr<DecoderBuffer> decoder_buffer = input_queue_.front().first;

  if (decoder_buffer->end_of_stream()) {
    media_codec_->QueueEOS(input_info.buf_index);

    SetState(STATE_DRAINING);
    return true;
  }

  media::MediaCodecStatus status = MEDIA_CODEC_OK;

  const DecryptConfig* decrypt_config = decoder_buffer->decrypt_config();
  if (decrypt_config && decrypt_config->is_encrypted()) {
    // A pending buffer is already filled with data, no need to copy it again.
    const uint8_t* memory =
        input_info.is_pending ? nullptr : decoder_buffer->data();

    DVLOG(2) << __FUNCTION__ << ": QueueSecureInputBuffer:"
             << " index:" << input_info.buf_index
             << " pts:" << decoder_buffer->timestamp()
             << " size:" << decoder_buffer->data_size();

    status = media_codec_->QueueSecureInputBuffer(
        input_info.buf_index, memory, decoder_buffer->data_size(),
        decrypt_config->key_id(), decrypt_config->iv(),
        decrypt_config->subsamples(), decoder_buffer->timestamp());

  } else {
    DVLOG(2) << __FUNCTION__ << ": QueueInputBuffer:"
             << " index:" << input_info.buf_index
             << " pts:" << decoder_buffer->timestamp()
             << " size:" << decoder_buffer->data_size();

    status = media_codec_->QueueInputBuffer(
        input_info.buf_index, decoder_buffer->data(),
        decoder_buffer->data_size(), decoder_buffer->timestamp());
  }

  bool did_work = false;
  switch (status) {
    case MEDIA_CODEC_ERROR:
      DVLOG(0) << __FUNCTION__ << ": MEDIA_CODEC_ERROR from QueueInputBuffer";
      SetState(STATE_ERROR);
      break;

    case MEDIA_CODEC_NO_KEY:
      DVLOG(1) << "QueueSecureInputBuffer failed: MEDIA_CODEC_NO_KEY";
      pending_input_buf_index_ = input_info.buf_index;
      SetState(STATE_WAITING_FOR_KEY);
      break;

    case MEDIA_CODEC_OK:
      did_work = true;
      break;

    default:
      NOTREACHED() << "Unknown Queue(Secure)InputBuffer status " << status;
      SetState(STATE_ERROR);
      break;
  }
  return did_work;
}

void MediaCodecAudioDecoder::ClearInputQueue(DecodeStatus decode_status) {
  DVLOG(2) << __FUNCTION__;

  for (const auto& entry : input_queue_)
    entry.second.Run(decode_status);

  input_queue_.clear();
}

bool MediaCodecAudioDecoder::DequeueOutput() {
  DVLOG(2) << __FUNCTION__;

  MediaCodecStatus status;
  OutputBufferInfo out;
  bool did_work = false;
  do {
    status = media_codec_->DequeueOutputBuffer(NoWaitTimeout(), &out.buf_index,
                                               &out.offset, &out.size, &out.pts,
                                               &out.is_eos, &out.is_key_frame);

    switch (status) {
      case MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED:
        // Output buffers are replaced in MediaCodecBridge, nothing to do.
        DVLOG(2) << __FUNCTION__ << " MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED";
        did_work = true;
        break;

      case MEDIA_CODEC_OUTPUT_FORMAT_CHANGED:
        DVLOG(2) << __FUNCTION__ << " MEDIA_CODEC_OUTPUT_FORMAT_CHANGED";
        OnOutputFormatChanged();
        did_work = true;
        break;

      case MEDIA_CODEC_OK:
        // We got the decoded frame.
        if (out.is_eos)
          OnDecodedEos(out);
        else
          OnDecodedFrame(out);

        did_work = true;
        break;

      case MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER:
        // Nothing to do.
        DVLOG(2) << __FUNCTION__ << " MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER";
        break;

      case MEDIA_CODEC_ERROR:
        DVLOG(0) << __FUNCTION__
                 << ": MEDIA_CODEC_ERROR from DequeueOutputBuffer";

        // Next Decode() will report the error to the pipeline.
        SetState(STATE_ERROR);
        break;

      default:
        NOTREACHED() << "Unknown DequeueOutputBuffer status " << status;
        // Next Decode() will report the error to the pipeline.
        SetState(STATE_ERROR);
        break;
    }
  } while (status != MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER &&
           status != MEDIA_CODEC_ERROR && !out.is_eos);

  return did_work;
}

void MediaCodecAudioDecoder::ManageTimer(bool did_work) {
  bool should_be_running = true;

  base::TimeTicks now = base::TimeTicks::Now();
  if (did_work || idle_time_begin_ == base::TimeTicks()) {
    idle_time_begin_ = now;
  } else {
    // Make sure that we have done work recently enough, else stop the timer.
    if (now - idle_time_begin_ > IdleTimerTimeout())
      should_be_running = false;
  }

  if (should_be_running && !io_timer_.IsRunning()) {
    io_timer_.Start(FROM_HERE, DecodePollDelay(), this,
                    &MediaCodecAudioDecoder::DoIOTask);
  } else if (!should_be_running && io_timer_.IsRunning()) {
    io_timer_.Stop();
  }
}

void MediaCodecAudioDecoder::SetState(State new_state) {
  DVLOG(1) << __FUNCTION__ << ": " << AsString(state_) << "->"
           << AsString(new_state);
  state_ = new_state;
}

void MediaCodecAudioDecoder::OnDecodedEos(const OutputBufferInfo& out) {
  DVLOG(2) << __FUNCTION__ << " pts:" << out.pts;

  DCHECK_NE(out.buf_index, kInvalidBufferIndex);
  DCHECK(media_codec_);

  DCHECK_EQ(state_, STATE_DRAINING);

  media_codec_->ReleaseOutputBuffer(out.buf_index, false);

  // Report the end of decoding for EOS DecoderBuffer.
  DCHECK(!input_queue_.empty());
  DCHECK(input_queue_.front().first->end_of_stream());

  const DecodeCB& decode_cb = input_queue_.front().second;
  decode_cb.Run(DecodeStatus::OK);

  // Remove the EOS buffer from the input queue.
  input_queue_.pop_front();

  // Set state STATE_DRAINED after we have received EOS frame at the output.
  // media_decoder_job.cc says: once output EOS has occurred, we should
  // not be asked to decode again.
  SetState(STATE_DRAINED);
}

void MediaCodecAudioDecoder::OnDecodedFrame(const OutputBufferInfo& out) {
  DVLOG(2) << __FUNCTION__ << " pts:" << out.pts;

  DCHECK_NE(out.size, 0U);
  DCHECK_NE(out.buf_index, kInvalidBufferIndex);
  DCHECK(media_codec_);

  // For proper |frame_count| calculation we need to use the actual number
  // of channels which can be different from |config_| value.
  const int bytes_per_frame = kBytesPerOutputSample * channel_count_;
  const size_t frame_count = out.size / bytes_per_frame;

  // Create AudioOutput buffer based on configuration.
  const int config_channel_count = GetChannelCount(config_);
  const SampleFormat sample_format = config_channel_count == channel_count_
                                         ? kSampleFormatS16         // can copy
                                         : kSampleFormatPlanarS16;  // upsample

  scoped_refptr<AudioBuffer> audio_buffer = AudioBuffer::CreateBuffer(
      sample_format, config_.channel_layout(), config_channel_count,
      config_.samples_per_second(), frame_count);

  if (config_channel_count == channel_count_) {
    // Copy data into AudioBuffer.
    CHECK_LE(out.size, audio_buffer->data_size());

    MediaCodecStatus status = media_codec_->CopyFromOutputBuffer(
        out.buf_index, out.offset, audio_buffer->channel_data()[0], out.size);

    // TODO(timav,watk): This CHECK maintains the behavior of this call before
    // we started catching CodecException and returning it as MEDIA_CODEC_ERROR.
    // It needs to be handled some other way. http://crbug.com/585978
    CHECK_EQ(status, MEDIA_CODEC_OK);
  } else {
    // Separate the planes while copying MediaCodec buffer into AudioBuffer.
    DCHECK_LT(channel_count_, config_channel_count);

    const uint8_t* interleaved_data = nullptr;
    size_t interleaved_capacity = 0;
    MediaCodecStatus status = media_codec_->GetOutputBufferAddress(
        out.buf_index, out.offset, &interleaved_data, &interleaved_capacity);

    // TODO(timav): Handle wrong status properly, http://crbug.com/585978.
    CHECK_EQ(status, MEDIA_CODEC_OK);

    DCHECK_LE(out.size, interleaved_capacity);

    memset(audio_buffer->channel_data()[0], 0, audio_buffer->data_size());
    SeparatePlanes(interleaved_data, frame_count, bytes_per_frame,
                   channel_count_, audio_buffer->channel_data());
  }

  // Release MediaCodec output buffer.
  media_codec_->ReleaseOutputBuffer(out.buf_index, false);

  // Calculate and set buffer timestamp.

  const bool first_buffer =
      timestamp_helper_->base_timestamp() == kNoTimestamp();
  if (first_buffer) {
    // Clamp the base timestamp to zero.
    timestamp_helper_->SetBaseTimestamp(std::max(base::TimeDelta(), out.pts));
  }

  audio_buffer->set_timestamp(timestamp_helper_->GetTimestamp());
  timestamp_helper_->AddFrames(frame_count);

  // Call the |output_cb_|.
  output_cb_.Run(audio_buffer);
}

void MediaCodecAudioDecoder::OnOutputFormatChanged() {
  DVLOG(2) << __FUNCTION__;

  int new_sampling_rate = 0;
  MediaCodecStatus status =
      media_codec_->GetOutputSamplingRate(&new_sampling_rate);
  if (status != MEDIA_CODEC_OK) {
    DLOG(ERROR) << "GetOutputSamplingRate failed.";
    SetState(STATE_ERROR);
    return;
  }
  if (new_sampling_rate != config_.samples_per_second()) {
    // We do not support the change of sampling rate on the fly
    DLOG(ERROR) << "Sampling rate change is not supported by "
                << GetDisplayName() << " (detected change "
                << config_.samples_per_second() << "->" << new_sampling_rate
                << ")";
    SetState(STATE_ERROR);
    return;
  }

  status = media_codec_->GetOutputChannelCount(&channel_count_);
  if (status != MEDIA_CODEC_OK) {
    DLOG(ERROR) << "GetOutputChannelCount failed.";
    SetState(STATE_ERROR);
    return;
  }

  const int config_channel_count = GetChannelCount(config_);
  DVLOG(1) << __FUNCTION__ << ": new channel count:" << channel_count_
           << " (configured for " << config_channel_count << ")";

  if (channel_count_ > config_channel_count) {
    DLOG(ERROR) << "Actual channel count " << channel_count_
                << " is greater than configured " << config_channel_count;
    SetState(STATE_ERROR);
    return;
  }
}

#undef RETURN_STRING
#define RETURN_STRING(x) \
  case x:                \
    return #x;

// static
const char* MediaCodecAudioDecoder::AsString(State state) {
  switch (state) {
    RETURN_STRING(STATE_UNINITIALIZED);
    RETURN_STRING(STATE_WAITING_FOR_MEDIA_CRYPTO);
    RETURN_STRING(STATE_READY);
    RETURN_STRING(STATE_WAITING_FOR_KEY);
    RETURN_STRING(STATE_DRAINING);
    RETURN_STRING(STATE_DRAINED);
    RETURN_STRING(STATE_ERROR);
  }
  NOTREACHED() << "Unknown state " << state;
  return nullptr;
}

#undef RETURN_STRING

}  // namespace media
