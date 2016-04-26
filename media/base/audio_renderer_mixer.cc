// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_renderer_mixer.h"

#include <cmath>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"

namespace media {

enum { kPauseDelaySeconds = 10 };

AudioRendererMixer::AudioRendererMixer(
    const AudioParameters& output_params,
    const scoped_refptr<AudioRendererSink>& sink)
    : audio_sink_(sink),
      output_params_(output_params),
      master_converter_(output_params, output_params, true),
      pause_delay_(base::TimeDelta::FromSeconds(kPauseDelaySeconds)),
      last_play_time_(base::TimeTicks::Now()),
      // Initialize |playing_| to true since Start() results in an auto-play.
      playing_(true) {
  audio_sink_->Initialize(output_params, this);
  audio_sink_->Start();
}

AudioRendererMixer::~AudioRendererMixer() {
  // AudioRendererSinks must be stopped before being destructed.
  audio_sink_->Stop();

  // Ensure that all mixer inputs have removed themselves prior to destruction.
  DCHECK(master_converter_.empty());
  DCHECK(converters_.empty());
  DCHECK_EQ(error_callbacks_.size(), 0U);
}

void AudioRendererMixer::AddMixerInput(const AudioParameters& input_params,
                                       AudioConverter::InputCallback* input) {
  base::AutoLock auto_lock(lock_);
  if (!playing_) {
    playing_ = true;
    last_play_time_ = base::TimeTicks::Now();
    audio_sink_->Play();
  }

  int input_sample_rate = input_params.sample_rate();
  if (is_master_sample_rate(input_sample_rate)) {
    master_converter_.AddInput(input);
  } else {
    AudioConvertersMap::iterator converter =
        converters_.find(input_sample_rate);
    if (converter == converters_.end()) {
      std::pair<AudioConvertersMap::iterator, bool> result =
          converters_.insert(std::make_pair(
              input_sample_rate, base::WrapUnique(
                                     // We expect all InputCallbacks to be
                                     // capable of handling arbitrary buffer
                                     // size requests, disabling FIFO.
                                     new LoopbackAudioConverter(
                                         input_params, output_params_, true))));
      converter = result.first;

      // Add newly-created resampler as an input to the master mixer.
      master_converter_.AddInput(converter->second.get());
    }
    converter->second->AddInput(input);
  }
}

void AudioRendererMixer::RemoveMixerInput(
    const AudioParameters& input_params,
    AudioConverter::InputCallback* input) {
  base::AutoLock auto_lock(lock_);

  int input_sample_rate = input_params.sample_rate();
  if (is_master_sample_rate(input_sample_rate)) {
    master_converter_.RemoveInput(input);
  } else {
    AudioConvertersMap::iterator converter =
        converters_.find(input_sample_rate);
    DCHECK(converter != converters_.end());
    converter->second->RemoveInput(input);
    if (converter->second->empty()) {
      // Remove converter when it's empty.
      master_converter_.RemoveInput(converter->second.get());
      converters_.erase(converter);
    }
  }
}

void AudioRendererMixer::AddErrorCallback(const base::Closure& error_cb) {
  base::AutoLock auto_lock(lock_);
  error_callbacks_.push_back(error_cb);
}

void AudioRendererMixer::RemoveErrorCallback(const base::Closure& error_cb) {
  base::AutoLock auto_lock(lock_);
  for (ErrorCallbackList::iterator it = error_callbacks_.begin();
       it != error_callbacks_.end();
       ++it) {
    if (it->Equals(error_cb)) {
      error_callbacks_.erase(it);
      return;
    }
  }

  // An error callback should always exist when called.
  NOTREACHED();
}

OutputDeviceInfo AudioRendererMixer::GetOutputDeviceInfo() {
  DVLOG(1) << __FUNCTION__;
  base::AutoLock auto_lock(lock_);
  return audio_sink_->GetOutputDeviceInfo();
}

int AudioRendererMixer::Render(AudioBus* audio_bus,
                               uint32_t frames_delayed,
                               uint32_t frames_skipped) {
  base::AutoLock auto_lock(lock_);

  // If there are no mixer inputs and we haven't seen one for a while, pause the
  // sink to avoid wasting resources when media elements are present but remain
  // in the pause state.
  const base::TimeTicks now = base::TimeTicks::Now();
  if (!master_converter_.empty()) {
    last_play_time_ = now;
  } else if (now - last_play_time_ >= pause_delay_ && playing_) {
    audio_sink_->Pause();
    playing_ = false;
  }

  // TODO(chcunningham): Delete this conversion and change ConvertWith delay to
  // expect a count of frames delayed instead of TimeDelta (less precise).
  // See http://crbug.com/587522.
  base::TimeDelta audio_delay = base::TimeDelta::FromMicroseconds(
      std::round(frames_delayed * output_params_.GetMicrosecondsPerFrame()));

  master_converter_.ConvertWithDelay(audio_delay, audio_bus);
  return audio_bus->frames();
}

void AudioRendererMixer::OnRenderError() {
  // Call each mixer input and signal an error.
  base::AutoLock auto_lock(lock_);
  for (const auto& cb : error_callbacks_)
    cb.Run();
}

}  // namespace media
