// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_renderer_mixer.h"

#include "base/logging.h"

namespace media {

AudioRendererMixer::AudioRendererMixer(
    const AudioParameters& params, const scoped_refptr<AudioRendererSink>& sink)
    : audio_parameters_(params),
      audio_sink_(sink) {
  // TODO(dalecurtis): Once we have resampling we'll need to pass on a different
  // set of AudioParameters than the ones we're given.
  audio_sink_->Initialize(audio_parameters_, this);
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
  mixer_inputs_.insert(input);
}

void AudioRendererMixer::RemoveMixerInput(
    const scoped_refptr<AudioRendererMixerInput>& input) {
  base::AutoLock auto_lock(mixer_inputs_lock_);
  mixer_inputs_.erase(input);
}

int AudioRendererMixer::Render(const std::vector<float*>& audio_data,
                               int number_of_frames,
                               int audio_delay_milliseconds) {
  base::AutoLock auto_lock(mixer_inputs_lock_);

  // Zero |audio_data| so we're mixing into a clean buffer and return silence if
  // we couldn't get enough data from our inputs.
  for (int i = 0; i < audio_parameters_.channels(); ++i)
    memset(audio_data[i], 0, number_of_frames * sizeof(*audio_data[i]));

  // Have each mixer render its data into an output buffer then mix the result.
  for (AudioRendererMixerInputSet::iterator it = mixer_inputs_.begin();
       it != mixer_inputs_.end(); ++it) {
    const scoped_refptr<AudioRendererMixerInput>& input = *it;

    double volume;
    input->GetVolume(&volume);

    // Nothing to do if the input isn't playing or the volume is zero.
    if (!input->playing() || volume == 0.0f)
      continue;

    const std::vector<float*>& mixer_input_audio_data = input->audio_data();

    int frames_filled = input->callback()->Render(
        mixer_input_audio_data, number_of_frames, audio_delay_milliseconds);
    if (frames_filled == 0)
      continue;

    // TODO(dalecurtis): Resample audio data.

    // Volume adjust and mix each mixer input into |audio_data| after rendering.
    // TODO(dalecurtis): Optimize with NEON/SSE/AVX vector_fmac from FFmpeg.
    for (int j = 0; j < audio_parameters_.channels(); ++j) {
      float* dest = audio_data[j];
      float* source = mixer_input_audio_data[j];
      for (int k = 0; k < frames_filled; ++k)
        dest[k] += source[k] * static_cast<float>(volume);
    }

    // No need to clamp values as InterleaveFloatToInt() will take care of this
    // for us later when data is transferred to the browser process.
  }

  // Always return the full number of frames requested, padded with silence if
  // we couldn't get enough data.
  return number_of_frames;
}

void AudioRendererMixer::OnRenderError() {
  base::AutoLock auto_lock(mixer_inputs_lock_);

  // Call each mixer input and signal an error.
  for (AudioRendererMixerInputSet::iterator it = mixer_inputs_.begin();
       it != mixer_inputs_.end(); ++it) {
    (*it)->callback()->OnRenderError();
  }
}

}  // namespace media
