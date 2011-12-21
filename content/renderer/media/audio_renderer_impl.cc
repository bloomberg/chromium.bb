// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_renderer_impl.h"

#include <math.h>

#include <algorithm>

#include "base/bind.h"
#include "content/common/child_process.h"
#include "content/common/media/audio_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "media/audio/audio_buffers_state.h"
#include "media/audio/audio_util.h"

// We define GetBufferSizeForSampleRate() instead of using
// GetAudioHardwareBufferSize() in audio_util because we're using
// the AUDIO_PCM_LINEAR flag, instead of AUDIO_PCM_LOW_LATENCY,
// which the audio_util functions assume.
//
// See: http://code.google.com/p/chromium/issues/detail?id=103627
// for a more detailed description of the subtleties.
static size_t GetBufferSizeForSampleRate(int sample_rate) {
  // kNominalBufferSize has been tested on Windows, Mac OS X, and Linux
  // using the low-latency audio codepath (SyncSocket implementation)
  // with the AUDIO_PCM_LINEAR flag.
  const size_t kNominalBufferSize = 2048;

  if (sample_rate <= 48000)
    return kNominalBufferSize;
  else if (sample_rate <= 96000)
    return kNominalBufferSize * 2;
  return kNominalBufferSize * 4;
}

AudioRendererImpl::AudioRendererImpl()
    : AudioRendererBase(),
      bytes_per_second_(0),
      stopped_(false) {
  // We create the AudioDevice here because it must be created in the
  // main thread.  But we don't yet know the audio format (sample-rate, etc.)
  // at this point.  Later, when OnInitialize() is called, we have
  // the audio format information and call the AudioDevice::Initialize()
  // method to fully initialize it.
  audio_device_ = new AudioDevice();
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
  audio_parameters_ = AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                                      channel_layout,
                                      sample_rate,
                                      bits_per_channel,
                                      0);

  bytes_per_second_ = audio_parameters_.GetBytesPerSecond();

  DCHECK(audio_device_.get());

  if (!audio_device_->IsInitialized()) {
    audio_device_->Initialize(
        GetBufferSizeForSampleRate(sample_rate),
        audio_parameters_.channels,
        audio_parameters_.sample_rate,
        audio_parameters_.format,
        this);

    audio_device_->Start();
  }

  return true;
}

void AudioRendererImpl::OnStop() {
  if (stopped_)
    return;

  DCHECK(audio_device_.get());
  audio_device_->Stop();

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
                             const media::FilterStatusCB& cb) {
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
  DCHECK(audio_device_.get());
  audio_device_->SetVolume(volume);
}

void AudioRendererImpl::DoPlay() {
  earliest_end_time_ = base::Time::Now();
  DCHECK(audio_device_.get());
  audio_device_->Play();
}

void AudioRendererImpl::DoPause() {
  DCHECK(audio_device_.get());
  audio_device_->Pause(false);
}

void AudioRendererImpl::DoSeek() {
  earliest_end_time_ = base::Time::Now();

  // Pause and flush the stream when we seek to a new location.
  DCHECK(audio_device_.get());
  audio_device_->Pause(true);
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

  uint32 bytes_per_frame =
      audio_parameters_.bits_per_sample * audio_parameters_.channels / 8;

  const size_t buf_size = number_of_frames * bytes_per_frame;
  scoped_array<uint8> buf(new uint8[buf_size]);

  base::Time time_now = base::Time::Now();
  uint32 filled = FillBuffer(buf.get(),
                             buf_size,
                             request_delay,
                             time_now >= earliest_end_time_);
  DCHECK_LE(filled, buf_size);
  UpdateEarliestEndTime(filled, request_delay, time_now);

  uint32 filled_frames = filled / bytes_per_frame;

  // Deinterleave each audio channel.
  int channels = audio_data.size();
  for (int channel_index = 0; channel_index < channels; ++channel_index) {
    media::DeinterleaveAudioChannel(buf.get(),
                                    audio_data[channel_index],
                                    channels,
                                    channel_index,
                                    bytes_per_frame / channels,
                                    filled_frames);

    // If FillBuffer() didn't give us enough data then zero out the remainder.
    if (filled_frames < number_of_frames) {
      int frames_to_zero = number_of_frames - filled_frames;
      memset(audio_data[channel_index] + filled_frames,
             0,
             sizeof(float) * frames_to_zero);
    }
  }
  return filled_frames;
}
