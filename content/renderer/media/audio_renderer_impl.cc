// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_renderer_impl.h"

#include <math.h>

#include <algorithm>

#include "base/bind.h"
#include "content/common/child_process.h"
#include "content/common/media/audio_messages.h"
#include "content/renderer/media/audio_hardware.h"
#include "content/renderer/render_thread_impl.h"
#include "media/audio/audio_buffers_state.h"
#include "media/audio/audio_util.h"
#include "media/base/filter_host.h"

AudioRendererImpl::AudioRendererImpl(media::AudioRendererSink* sink)
    : AudioRendererBase(),
      bytes_per_second_(0),
      stopped_(false),
      sink_(sink),
      is_initialized_(false) {
}

AudioRendererImpl::~AudioRendererImpl() {
}

base::TimeDelta AudioRendererImpl::ConvertToDuration(int bytes) {
  if (bytes_per_second_) {
    return base::TimeDelta::FromMicroseconds(
        base::Time::kMicrosecondsPerSecond * bytes / bytes_per_second_);
  }
  return base::TimeDelta();
}

void AudioRendererImpl::UpdateEarliestEndTime(int bytes_filled,
                                              base::TimeDelta request_delay,
                                              base::Time time_now) {
  if (bytes_filled != 0) {
    base::TimeDelta predicted_play_time = ConvertToDuration(bytes_filled);
    float playback_rate = GetPlaybackRate();
    if (playback_rate != 1.0f) {
      predicted_play_time = base::TimeDelta::FromMicroseconds(
          static_cast<int64>(ceil(predicted_play_time.InMicroseconds() *
                                  playback_rate)));
    }
    earliest_end_time_ =
        std::max(earliest_end_time_,
                 time_now + request_delay + predicted_play_time);
  }
}

bool AudioRendererImpl::OnInitialize(int bits_per_channel,
                                     ChannelLayout channel_layout,
                                     int sample_rate) {
  // We use the AUDIO_PCM_LINEAR flag because AUDIO_PCM_LOW_LATENCY
  // does not currently support all the sample-rates that we require.
  // Please see: http://code.google.com/p/chromium/issues/detail?id=103627
  // for more details.
  audio_parameters_.Reset(
      media::AudioParameters::AUDIO_PCM_LINEAR,
      channel_layout, sample_rate, bits_per_channel,
      audio_hardware::GetHighLatencyOutputBufferSize(sample_rate));

  bytes_per_second_ = audio_parameters_.GetBytesPerSecond();

  DCHECK(sink_.get());

  if (!is_initialized_) {
    sink_->Initialize(audio_parameters_, this);

    sink_->Start();
    is_initialized_ = true;
    return true;
  }

  return false;
}

void AudioRendererImpl::OnStop() {
  if (stopped_)
    return;

  DCHECK(sink_.get());
  sink_->Stop();

  stopped_ = true;
}

void AudioRendererImpl::SetPlaybackRate(float rate) {
  DCHECK_LE(0.0f, rate);

  // Handle the case where we stopped due to IO message loop dying.
  if (stopped_) {
    AudioRendererBase::SetPlaybackRate(rate);
    return;
  }

  // We have two cases here:
  // Play: GetPlaybackRate() == 0.0 && rate != 0.0
  // Pause: GetPlaybackRate() != 0.0 && rate == 0.0
  if (GetPlaybackRate() == 0.0f && rate != 0.0f) {
    DoPlay();
  } else if (GetPlaybackRate() != 0.0f && rate == 0.0f) {
    // Pause is easy, we can always pause.
    DoPause();
  }
  AudioRendererBase::SetPlaybackRate(rate);
}

void AudioRendererImpl::Pause(const base::Closure& callback) {
  AudioRendererBase::Pause(callback);
  if (stopped_)
    return;

  DoPause();
}

void AudioRendererImpl::Seek(base::TimeDelta time,
                             const media::PipelineStatusCB& cb) {
  AudioRendererBase::Seek(time, cb);
  if (stopped_)
    return;

  DoSeek();
}

void AudioRendererImpl::Play(const base::Closure& callback) {
  AudioRendererBase::Play(callback);
  if (stopped_)
    return;

  if (GetPlaybackRate() != 0.0f) {
    DoPlay();
  } else {
    DoPause();
  }
}

void AudioRendererImpl::SetVolume(float volume) {
  if (stopped_)
    return;
  DCHECK(sink_.get());
  sink_->SetVolume(volume);
}

void AudioRendererImpl::DoPlay() {
  earliest_end_time_ = base::Time::Now();
  DCHECK(sink_.get());
  sink_->Play();
}

void AudioRendererImpl::DoPause() {
  DCHECK(sink_.get());
  sink_->Pause(false);
}

void AudioRendererImpl::DoSeek() {
  earliest_end_time_ = base::Time::Now();

  // Pause and flush the stream when we seek to a new location.
  DCHECK(sink_.get());
  sink_->Pause(true);
}

size_t AudioRendererImpl::Render(const std::vector<float*>& audio_data,
                                 size_t number_of_frames,
                                 size_t audio_delay_milliseconds) {
  if (stopped_ || GetPlaybackRate() == 0.0f) {
    // Output silence if stopped.
    for (size_t i = 0; i < audio_data.size(); ++i)
      memset(audio_data[i], 0, sizeof(float) * number_of_frames);
    return 0;
  }

  // Adjust the playback delay.
  base::TimeDelta request_delay =
      base::TimeDelta::FromMilliseconds(audio_delay_milliseconds);

  // Finally we need to adjust the delay according to playback rate.
  if (GetPlaybackRate() != 1.0f) {
    request_delay = base::TimeDelta::FromMicroseconds(
        static_cast<int64>(ceil(request_delay.InMicroseconds() *
                                GetPlaybackRate())));
  }

  int bytes_per_frame  = audio_parameters_.GetBytesPerFrame();

  const size_t buf_size = number_of_frames * bytes_per_frame;
  scoped_array<uint8> buf(new uint8[buf_size]);

  uint32 frames_filled = FillBuffer(buf.get(), number_of_frames, request_delay);
  uint32 bytes_filled = frames_filled * bytes_per_frame;
  DCHECK_LE(bytes_filled, buf_size);
  UpdateEarliestEndTime(bytes_filled, request_delay, base::Time::Now());

  // Deinterleave each audio channel.
  int channels = audio_data.size();
  for (int channel_index = 0; channel_index < channels; ++channel_index) {
    media::DeinterleaveAudioChannel(buf.get(),
                                    audio_data[channel_index],
                                    channels,
                                    channel_index,
                                    bytes_per_frame / channels,
                                    frames_filled);

    // If FillBuffer() didn't give us enough data then zero out the remainder.
    if (frames_filled < number_of_frames) {
      int frames_to_zero = number_of_frames - frames_filled;
      memset(audio_data[channel_index] + frames_filled,
             0,
             sizeof(float) * frames_to_zero);
    }
  }
  return frames_filled;
}

void AudioRendererImpl::OnRenderError() {
  host()->DisableAudioRenderer();
}

void AudioRendererImpl::OnRenderEndOfStream() {
  // TODO(enal): schedule callback instead of polling.
  if (base::Time::Now() >= earliest_end_time_)
    SignalEndOfStream();
}
