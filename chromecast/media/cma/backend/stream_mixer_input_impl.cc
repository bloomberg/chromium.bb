// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/stream_mixer_input_impl.h"

#include <algorithm>
#include <limits>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/media/cma/backend/stream_mixer.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "media/base/audio_bus.h"
#include "media/base/multi_channel_resampler.h"

#define RUN_ON_MIXER_THREAD(callback, ...)                                     \
  if (!mixer_task_runner_->BelongsToCurrentThread()) {                         \
    POST_TASK_TO_MIXER_THREAD(&StreamMixerInputImpl::callback, ##__VA_ARGS__); \
    return;                                                                    \
  }

#define POST_TASK_TO_MIXER_THREAD(task, ...) \
  mixer_task_runner_->PostTask(              \
      FROM_HERE, base::Bind(task, base::Unretained(this), ##__VA_ARGS__));

#define RUN_ON_CALLER_THREAD(callback, ...)                     \
  if (!caller_task_runner_->BelongsToCurrentThread()) {         \
    POST_TASK_TO_CALLER_THREAD(&StreamMixerInputImpl::callback, \
                               ##__VA_ARGS__);                  \
    return;                                                     \
  }

#define POST_TASK_TO_CALLER_THREAD(task, ...) \
  caller_task_runner_->PostTask(FROM_HERE,    \
                                base::Bind(task, weak_this_, ##__VA_ARGS__));

namespace chromecast {
namespace media {

namespace {

const int kNumOutputChannels = 2;
const int64_t kMaxInputQueueUs = 90000;
const int64_t kMinFadeMs = 15;
// Number of samples to report as readable when paused. When paused, the mixer
// will still pull this many frames each time it tries to write frames, but we
// fill the frames with silence.
const int kPausedReadSamples = 512;
const int kDefaultReadSize = ::media::SincResampler::kDefaultRequestSize;
const int64_t kNoTimestamp = std::numeric_limits<int64_t>::min();

const int kDefaultSlewTimeMs = 15;

std::string AudioContentTypeToString(media::AudioContentType type) {
  switch (type) {
    case media::AudioContentType::kAlarm:
      return "alarm";
    case media::AudioContentType::kCommunication:
      return "communication";
    default:
      return "media";
  }
}

int RoundUpMultiple(int value, int multiple) {
  return multiple * ((value + (multiple - 1)) / multiple);
}

}  // namespace

StreamMixerInputImpl::StreamMixerInputImpl(StreamMixerInput::Delegate* delegate,
                                           int input_samples_per_second,
                                           bool primary,
                                           const std::string& device_id,
                                           AudioContentType content_type,
                                           StreamMixer* mixer)
    : delegate_(delegate),
      input_samples_per_second_(input_samples_per_second),
      primary_(primary),
      device_id_(device_id),
      content_type_(content_type),
      mixer_(mixer),
      filter_group_(nullptr),
      mixer_task_runner_(mixer_->task_runner()),
      caller_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      resample_ratio_(1.0),
      state_(kStateUninitialized),
      stream_volume_multiplier_(1.0f),
      type_volume_multiplier_(1.0f),
      mute_volume_multiplier_(1.0f),
      slew_volume_(kDefaultSlewTimeMs),
      queued_frames_(0),
      queued_frames_including_resampler_(0),
      current_buffer_offset_(0),
      max_queued_frames_(kMaxInputQueueUs * input_samples_per_second /
                         base::Time::kMicrosecondsPerSecond),
      fade_frames_remaining_(0),
      fade_out_frames_total_(0),
      zeroed_frames_(0),
      is_underflowing_(false),
      weak_factory_(this) {
  LOG(INFO) << "Create " << device_id_ << " (" << this
            << "), content type = " << AudioContentTypeToString(content_type_);
  DCHECK(delegate_);
  DCHECK(mixer_);
  weak_this_ = weak_factory_.GetWeakPtr();
}

StreamMixerInputImpl::~StreamMixerInputImpl() {
  LOG(INFO) << "Destroy " << device_id_ << " (" << this << ")";
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
}

int StreamMixerInputImpl::input_samples_per_second() const {
  return input_samples_per_second_;
}

bool StreamMixerInputImpl::primary() const {
  return primary_;
}

std::string StreamMixerInputImpl::device_id() const {
  return device_id_;
}

AudioContentType StreamMixerInputImpl::content_type() const {
  return content_type_;
}

bool StreamMixerInputImpl::IsDeleting() const {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  return (state_ == kStateFinalFade || state_ == kStateDeleted);
}

void StreamMixerInputImpl::Initialize(
    const MediaPipelineBackend::AudioDecoder::RenderingDelay&
        mixer_rendering_delay) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  DCHECK(!IsDeleting());
  if (mixer_->output_samples_per_second() != input_samples_per_second_) {
    resample_ratio_ = static_cast<double>(input_samples_per_second_) /
                      mixer_->output_samples_per_second();
    resampler_.reset(new ::media::MultiChannelResampler(
        kNumOutputChannels, resample_ratio_, kDefaultReadSize,
        base::Bind(&StreamMixerInputImpl::ReadCB, base::Unretained(this))));
    resampler_->PrimeWithSilence();
  }
  slew_volume_.SetSampleRate(mixer_->output_samples_per_second());
  mixer_rendering_delay_ = mixer_rendering_delay;
  fade_out_frames_total_ = NormalFadeFrames();
  fade_frames_remaining_ = NormalFadeFrames();
}

void StreamMixerInputImpl::set_filter_group(FilterGroup* filter_group) {
  filter_group_ = filter_group;
}

FilterGroup* StreamMixerInputImpl::filter_group() {
  return filter_group_;
}

void StreamMixerInputImpl::PreventDelegateCalls() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  weak_factory_.InvalidateWeakPtrs();

  base::AutoLock lock(queue_lock_);
  pending_data_ = nullptr;
}

void StreamMixerInputImpl::PrepareToDelete(const OnReadyToDeleteCb& delete_cb) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  DCHECK(!delete_cb.is_null());
  DCHECK(!IsDeleting());
  delete_cb_ = delete_cb;
  if (state_ != kStateNormalPlayback && state_ != kStateFadingOut &&
      state_ != kStateGotEos) {
    DeleteThis();
    return;
  }

  {
    base::AutoLock lock(queue_lock_);
    if (state_ == kStateGotEos) {
      fade_out_frames_total_ =
          RoundUpMultiple(queued_frames_including_resampler_ / resample_ratio_,
                          mixer_->filter_frame_alignment());
      fade_frames_remaining_ = fade_out_frames_total_;
    } else if (state_ == kStateNormalPlayback) {
      fade_out_frames_total_ = std::min(
          RoundUpMultiple(static_cast<int>(queued_frames_including_resampler_ /
                                           resample_ratio_),
                          mixer_->filter_frame_alignment()),
          NormalFadeFrames());
      fade_frames_remaining_ = fade_out_frames_total_;
    }
  }

  state_ = kStateFinalFade;
  if (fade_frames_remaining_ == 0) {
    DeleteThis();
  } else {
    // Tell the mixer that some more data might be available (since when fading
    // out, we can drain the queue completely).
    mixer_->OnFramesQueued();
  }
}

void StreamMixerInputImpl::DeleteThis() {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  state_ = kStateDeleted;
  if (!delete_cb_.is_null())
    base::ResetAndReturn(&delete_cb_).Run(this);
}

void StreamMixerInputImpl::WritePcm(
    const scoped_refptr<DecoderBufferBase>& data) {
  MediaPipelineBackend::AudioDecoder::RenderingDelay delay;
  {
    base::AutoLock lock(queue_lock_);
    if (queued_frames_ >= max_queued_frames_) {
      DCHECK(!pending_data_);
      pending_data_ = data;
      return;
    }
    delay = QueueData(data);
  }
  // Alert the |mixer_| on the mixer thread.
  DidQueueData(data->end_of_stream());
  PostPcmCallback(delay);
}

// Must only be called when queue_lock_ is locked.
MediaPipelineBackend::AudioDecoder::RenderingDelay
StreamMixerInputImpl::QueueData(const scoped_refptr<DecoderBufferBase>& data) {
  queue_lock_.AssertAcquired();
  if (!data->end_of_stream()) {
    int frames = data->data_size() / (kNumOutputChannels * sizeof(float));
    queue_.push_back(data);
    queued_frames_ += frames;
    queued_frames_including_resampler_ += frames;
  }

  if (is_underflowing_) {
    return MediaPipelineBackend::AudioDecoder::RenderingDelay();
  }

  MediaPipelineBackend::AudioDecoder::RenderingDelay delay =
      mixer_rendering_delay_;
  if (delay.timestamp_microseconds != kNoTimestamp) {
    delay.delay_microseconds += static_cast<int64_t>(
        queued_frames_including_resampler_ *
        base::Time::kMicrosecondsPerSecond / input_samples_per_second_);
  }
  return delay;
}

void StreamMixerInputImpl::PostPcmCallback(
    const MediaPipelineBackend::AudioDecoder::RenderingDelay& delay) {
  RUN_ON_CALLER_THREAD(PostPcmCallback, delay);
  delegate_->OnWritePcmCompletion(MediaPipelineBackend::kBufferSuccess, delay);
}

void StreamMixerInputImpl::DidQueueData(bool end_of_stream) {
  RUN_ON_MIXER_THREAD(DidQueueData, end_of_stream);
  DCHECK(!IsDeleting());
  if (end_of_stream) {
    LOG(INFO) << "End of stream for " << this;
    state_ = kStateGotEos;
  } else if (state_ == kStateUninitialized) {
    state_ = kStateNormalPlayback;
  }
  mixer_->OnFramesQueued();
}

void StreamMixerInputImpl::OnSkipped() {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  if (!is_underflowing_) {
    LOG(WARNING) << "Underflow for " << this;
    is_underflowing_ = true;
  }
  if (state_ == kStateNormalPlayback) {
    // Fade in once this input starts providing data again.
    fade_frames_remaining_ = NormalFadeFrames();
  }
  slew_volume_.Interrupted();
}

void StreamMixerInputImpl::AfterWriteFrames(
    const MediaPipelineBackend::AudioDecoder::RenderingDelay&
        mixer_rendering_delay) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  double resampler_queued_frames =
      (resampler_ ? resampler_->BufferedFrames() : 0);

  bool queued_more_data = false;
  MediaPipelineBackend::AudioDecoder::RenderingDelay total_delay;
  {
    base::AutoLock lock(queue_lock_);
    mixer_rendering_delay_ = mixer_rendering_delay;
    queued_frames_ = 0;
    for (const auto& data : queue_) {
      queued_frames_ +=
          data->data_size() / (kNumOutputChannels * sizeof(float));
    }
    queued_frames_ -= current_buffer_offset_;
    DCHECK_GE(queued_frames_, 0);
    queued_frames_including_resampler_ =
        queued_frames_ + resampler_queued_frames;

    if (pending_data_ && queued_frames_ < max_queued_frames_) {
      scoped_refptr<DecoderBufferBase> data = pending_data_;
      pending_data_ = nullptr;
      total_delay = QueueData(data);
      queued_more_data = true;
      if (data->end_of_stream()) {
        LOG(INFO) << "End of stream for " << this;
        state_ = kStateGotEos;
      }
    }
  }

  if (queued_more_data)
    PostPcmCallback(total_delay);
}

int StreamMixerInputImpl::MaxReadSize() {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  if (state_ == kStatePaused || state_ == kStateDeleted)
    return kPausedReadSamples;
  if (state_ == kStateFinalFade)
    return fade_frames_remaining_;

  int queued_frames;
  {
    base::AutoLock lock(queue_lock_);
    if (state_ == kStateGotEos) {
      return RoundUpMultiple(
          std::max(static_cast<int>(queued_frames_including_resampler_ /
                                    resample_ratio_),
                   kDefaultReadSize),
          mixer_->filter_frame_alignment());
    }
    queued_frames = queued_frames_;
  }

  int available_frames = 0;
  if (resampler_) {
    int num_chunks = queued_frames / kDefaultReadSize;
    available_frames = resampler_->ChunkSize() * num_chunks;
  } else {
    available_frames = queued_frames;
  }

  if (state_ == kStateFadingOut) {
    return std::min(available_frames, fade_frames_remaining_);
  }
  return std::max(0, available_frames - NormalFadeFrames());
}

void StreamMixerInputImpl::GetResampledData(::media::AudioBus* dest,
                                            int frames) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  DCHECK(dest);
  DCHECK_EQ(kNumOutputChannels, dest->channels());
  DCHECK_GE(dest->frames(), frames);
  is_underflowing_ = false;

  if (state_ == kStatePaused || state_ == kStateDeleted) {
    dest->ZeroFramesPartial(0, frames);
    return;
  }

  if (resampler_) {
    resampler_->Resample(frames, dest);
  } else {
    FillFrames(0, dest, frames);
  }

  if (state_ == kStateFadingOut || state_ == kStateFinalFade) {
    FadeOut(dest, frames);
  } else if (fade_frames_remaining_) {
    FadeIn(dest, frames);
  }
}

// This is the signature expected by the resampler. |output| should be filled
// completely by FillFrames.
void StreamMixerInputImpl::ReadCB(int frame_delay, ::media::AudioBus* output) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  DCHECK(output);
  FillFrames(frame_delay, output, output->frames());
}

void StreamMixerInputImpl::FillFrames(int frame_delay,
                                      ::media::AudioBus* output,
                                      int frames) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  DCHECK(output);
  DCHECK_GE(output->frames(), frames);
  int frames_left = frames;
  int frames_filled = 0;
  while (frames_left) {
    scoped_refptr<DecoderBufferBase> buffer;
    int buffer_frames;
    int frames_to_copy;
    int buffer_offset = current_buffer_offset_;

    {
      base::AutoLock lock(queue_lock_);
      if (!queue_.empty()) {
        buffer = queue_.front();
        buffer_frames =
            buffer->data_size() / (kNumOutputChannels * sizeof(float));
        frames_to_copy =
            std::min(frames_left, buffer_frames - current_buffer_offset_);
        // Note that queued_frames_ is not updated until AfterWriteFrames().
        // This is done so that the rendering delay is always correct from the
        // perspective of the calling thread.
        current_buffer_offset_ += frames_to_copy;
        if (current_buffer_offset_ == buffer_frames) {
          queue_.pop_front();
          current_buffer_offset_ = 0;
        }
      }
    }

    if (buffer) {
      const float* buffer_samples =
          reinterpret_cast<const float*>(buffer->data());
      for (int i = 0; i < kNumOutputChannels; ++i) {
        const float* buffer_channel = buffer_samples + (buffer_frames * i);
        memcpy(output->channel(i) + frames_filled,
               buffer_channel + buffer_offset, frames_to_copy * sizeof(float));
      }
      frames_left -= frames_to_copy;
      frames_filled += frames_to_copy;
      LOG_IF(WARNING, state_ != kStateFinalFade && zeroed_frames_ > 0)
          << "Filled a total of " << zeroed_frames_ << " frames with 0";
      zeroed_frames_ = 0;
    } else {
      // No data left in queue; fill remaining frames with zeros.
      LOG_IF(WARNING, state_ != kStateFinalFade && zeroed_frames_ == 0)
          << "Starting to fill frames with 0";
      zeroed_frames_ += frames_left;
      output->ZeroFramesPartial(frames_filled, frames_left);
      frames_filled += frames_left;
      frames_left = 0;
      break;
    }
  }

  DCHECK_EQ(0, frames_left);
  DCHECK_EQ(frames, frames_filled);
}

int StreamMixerInputImpl::NormalFadeFrames() {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  int frames = (mixer_->output_samples_per_second() * kMinFadeMs /
                base::Time::kMillisecondsPerSecond) -
               1;
  return frames <= 0
             ? 0
             : RoundUpMultiple(frames, mixer_->filter_frame_alignment());
}

void StreamMixerInputImpl::FadeIn(::media::AudioBus* dest, int frames) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  float fade_in_frames = mixer_->output_samples_per_second() * kMinFadeMs /
                         base::Time::kMillisecondsPerSecond;
  for (int f = 0; f < frames && fade_frames_remaining_; ++f) {
    float fade_multiplier = 1.0 - fade_frames_remaining_ / fade_in_frames;
    for (int c = 0; c < kNumOutputChannels; ++c)
      dest->channel(c)[f] *= fade_multiplier;
    --fade_frames_remaining_;
  }
}

void StreamMixerInputImpl::FadeOut(::media::AudioBus* dest, int frames) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  int f = 0;
  for (; f < frames && fade_frames_remaining_; ++f) {
    float fade_multiplier =
        fade_frames_remaining_ / static_cast<float>(fade_out_frames_total_);
    for (int c = 0; c < kNumOutputChannels; ++c)
      dest->channel(c)[f] *= fade_multiplier;
    --fade_frames_remaining_;
  }
  // Zero remaining frames
  for (; f < frames; ++f) {
    for (int c = 0; c < kNumOutputChannels; ++c)
      dest->channel(c)[f] = 0.0f;
  }

  if (fade_frames_remaining_ == 0) {
    if (state_ == kStateFinalFade) {
      DeleteThis();
    } else {
      state_ = kStatePaused;
    }
  }
}

void StreamMixerInputImpl::SignalError(StreamMixerInput::MixerError error) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  if (state_ == kStateFinalFade) {
    DeleteThis();
    return;
  }
  state_ = kStateError;
  PostError(error);
}

void StreamMixerInputImpl::PostError(StreamMixerInput::MixerError error) {
  RUN_ON_CALLER_THREAD(PostError, error);
  delegate_->OnMixerError(error);
}

void StreamMixerInputImpl::SetPaused(bool paused) {
  RUN_ON_MIXER_THREAD(SetPaused, paused);
  DCHECK(!IsDeleting());
  if (paused) {
    if (state_ == kStateUninitialized) {
      state_ = kStatePaused;
    } else if (state_ == kStateNormalPlayback) {
      fade_frames_remaining_ = NormalFadeFrames() - fade_frames_remaining_;
      state_ = (fade_frames_remaining_ ? kStateFadingOut : kStatePaused);
    } else {
      return;
    }
    LOG(INFO) << "Pausing " << this;
  } else {
    if (state_ == kStateFadingOut) {
      fade_frames_remaining_ = NormalFadeFrames() - fade_frames_remaining_;
    } else if (state_ == kStatePaused) {
      fade_frames_remaining_ = NormalFadeFrames();
    } else {
      return;
    }
    LOG(INFO) << "Unpausing " << this;
    state_ = kStateNormalPlayback;
  }
  DCHECK_GE(fade_frames_remaining_, 0);

  if (state_ == kStateFadingOut) {
    // Tell the mixer that some more data might be available (since when fading
    // out, we can drain the queue completely).
    mixer_->OnFramesQueued();
  }
}

void StreamMixerInputImpl::SetVolumeMultiplier(float multiplier) {
  RUN_ON_MIXER_THREAD(SetVolumeMultiplier, multiplier);
  DCHECK(!IsDeleting());
  stream_volume_multiplier_ = std::max(0.0f, multiplier);
  float effective_volume = EffectiveVolume();
  LOG(INFO) << device_id_ << "(" << this
            << "): stream volume = " << stream_volume_multiplier_
            << ", effective multiplier = " << effective_volume;
  slew_volume_.SetMaxSlewTimeMs(kDefaultSlewTimeMs);
  slew_volume_.SetVolume(effective_volume);
}

void StreamMixerInputImpl::SetContentTypeVolume(float volume, int fade_ms) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  type_volume_multiplier_ = std::max(0.0f, std::min(volume, 1.0f));
  float effective_volume = EffectiveVolume();
  LOG(INFO) << device_id_ << "(" << this
            << "): type volume = " << type_volume_multiplier_
            << ", effective multiplier = " << effective_volume;
  if (fade_ms < 0) {
    fade_ms = kDefaultSlewTimeMs;
  } else {
    LOG(INFO) << "Fade over " << fade_ms << " ms";
  }
  slew_volume_.SetMaxSlewTimeMs(fade_ms);
  slew_volume_.SetVolume(effective_volume);
}

void StreamMixerInputImpl::SetMuted(bool muted) {
  DCHECK(mixer_task_runner_->BelongsToCurrentThread());
  mute_volume_multiplier_ = muted ? 0.0f : 1.0f;
  float effective_volume = EffectiveVolume();
  LOG(INFO) << device_id_ << "(" << this
            << "): mute volume = " << mute_volume_multiplier_
            << ", effective multiplier = " << effective_volume;
  slew_volume_.SetMaxSlewTimeMs(kDefaultSlewTimeMs);
  slew_volume_.SetVolume(effective_volume);
}

float StreamMixerInputImpl::EffectiveVolume() {
  float volume = stream_volume_multiplier_ * type_volume_multiplier_ *
                 mute_volume_multiplier_;
  return std::max(0.0f, std::min(volume, 1.0f));
}

void StreamMixerInputImpl::VolumeScaleAccumulate(bool repeat_transition,
                                                 const float* src,
                                                 int frames,
                                                 float* dest) {
  slew_volume_.ProcessFMAC(repeat_transition, src, frames, 1, dest);
}

}  // namespace media
}  // namespace chromecast
