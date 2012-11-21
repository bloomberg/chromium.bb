// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_renderer_mixer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"

namespace media {

AudioRendererMixer::AudioRendererMixer(
    const AudioParameters& input_params, const AudioParameters& output_params,
    const scoped_refptr<AudioRendererSink>& sink)
    : audio_sink_(sink),
      audio_converter_(input_params, output_params, true) {
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

void AudioRendererMixer::AddMixerInput(
    const scoped_refptr<AudioRendererMixerInput>& input) {
  base::AutoLock auto_lock(mixer_inputs_lock_);
  mixer_inputs_.push_back(input);
  audio_converter_.AddInput(input);
}

void AudioRendererMixer::RemoveMixerInput(
    const scoped_refptr<AudioRendererMixerInput>& input) {
  base::AutoLock auto_lock(mixer_inputs_lock_);
  audio_converter_.RemoveInput(input);
  mixer_inputs_.remove(input);
}

int AudioRendererMixer::Render(AudioBus* audio_bus,
                               int audio_delay_milliseconds) {
  base::AutoLock auto_lock(mixer_inputs_lock_);

  // Set the delay information for each mixer input.
  for (AudioRendererMixerInputSet::iterator it = mixer_inputs_.begin();
       it != mixer_inputs_.end(); ++it) {
    (*it)->set_audio_delay_milliseconds(audio_delay_milliseconds);
  }

  audio_converter_.Convert(audio_bus);
  return audio_bus->frames();
}

void AudioRendererMixer::OnRenderError() {
  base::AutoLock auto_lock(mixer_inputs_lock_);

  // Call each mixer input and signal an error.
  for (AudioRendererMixerInputSet::iterator it = mixer_inputs_.begin();
       it != mixer_inputs_.end(); ++it) {
    (*it)->OnRenderError();
  }
}

}  // namespace media
