// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_renderer_mixer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"

namespace media {

enum { kPauseDelaySeconds = 10 };

AudioRendererMixer::AudioRendererMixer(
    const AudioParameters& input_params, const AudioParameters& output_params,
    const scoped_refptr<AudioRendererSink>& sink)
    : audio_sink_(sink),
      audio_converter_(input_params, output_params, true),
      pause_delay_(base::TimeDelta::FromSeconds(kPauseDelaySeconds)),
      last_play_time_(base::Time::Now()),
      // Initialize |playing_| to true since Start() results in an auto-play.
      playing_(true) {
  audio_sink_->Initialize(output_params, this);
  audio_sink_->Start();
}

AudioRendererMixer::~AudioRendererMixer() {
  // AudioRendererSinks must be stopped before being destructed.
  audio_sink_->Stop();

  // Ensures that all mixer inputs have stopped themselves prior to destruction
  // and have called RemoveMixerInput().
  DCHECK_EQ(mixer_inputs_.size(), 0U);
}

void AudioRendererMixer::AddMixerInput(AudioConverter::InputCallback* input,
                                       const base::Closure& error_cb) {
  base::AutoLock auto_lock(mixer_inputs_lock_);

  if (!playing_) {
    playing_ = true;
    last_play_time_ = base::Time::Now();
    audio_sink_->Play();
  }

  DCHECK(mixer_inputs_.find(input) == mixer_inputs_.end());
  mixer_inputs_[input] = error_cb;
  audio_converter_.AddInput(input);
}

void AudioRendererMixer::RemoveMixerInput(
    AudioConverter::InputCallback* input) {
  base::AutoLock auto_lock(mixer_inputs_lock_);
  audio_converter_.RemoveInput(input);

  DCHECK(mixer_inputs_.find(input) != mixer_inputs_.end());
  mixer_inputs_.erase(input);
}

int AudioRendererMixer::Render(AudioBus* audio_bus,
                               int audio_delay_milliseconds) {
  base::AutoLock auto_lock(mixer_inputs_lock_);

  // If there are no mixer inputs and we haven't seen one for a while, pause the
  // sink to avoid wasting resources when media elements are present but remain
  // in the pause state.
  base::Time now = base::Time::Now();
  if (!mixer_inputs_.empty()) {
    last_play_time_ = now;
  } else if (now - last_play_time_ >= pause_delay_ && playing_) {
    audio_sink_->Pause();
    playing_ = false;
  }

  audio_converter_.ConvertWithDelay(
      base::TimeDelta::FromMilliseconds(audio_delay_milliseconds), audio_bus);
  return audio_bus->frames();
}

void AudioRendererMixer::OnRenderError() {
  base::AutoLock auto_lock(mixer_inputs_lock_);

  // Call each mixer input and signal an error.
  for (AudioRendererMixerInputSet::iterator it = mixer_inputs_.begin();
       it != mixer_inputs_.end(); ++it) {
    it->second.Run();
  }
}

}  // namespace media
