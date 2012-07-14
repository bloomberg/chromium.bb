// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_renderer_mixer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "media/audio/audio_util.h"
#include "media/base/limits.h"

namespace media {

AudioRendererMixer::AudioRendererMixer(
    const AudioParameters& input_params, const AudioParameters& output_params,
    const scoped_refptr<AudioRendererSink>& sink)
    : audio_sink_(sink),
      current_audio_delay_milliseconds_(0) {
  // Sanity check sample rates.
  DCHECK_LE(input_params.sample_rate(), limits::kMaxSampleRate);
  DCHECK_GE(input_params.sample_rate(), limits::kMinSampleRate);
  DCHECK_LE(output_params.sample_rate(), limits::kMaxSampleRate);
  DCHECK_GE(output_params.sample_rate(), limits::kMinSampleRate);

  // Only resample if necessary since it's expensive.
  if (input_params.sample_rate() != output_params.sample_rate()) {
    resampler_.reset(new MultiChannelResampler(
        output_params.channels(),
        input_params.sample_rate() / static_cast<double>(
            output_params.sample_rate()),
        base::Bind(&AudioRendererMixer::ProvideInput, base::Unretained(this))));
  }

  audio_sink_->Initialize(output_params, this);
  audio_sink_->Start();
}

AudioRendererMixer::~AudioRendererMixer() {
  // AudioRendererSinks must be stopped before being destructed.
  audio_sink_->Stop();

  // Clean up |mixer_input_audio_data_|.
  for (size_t i = 0; i < mixer_input_audio_data_.size(); ++i)
    delete [] mixer_input_audio_data_[i];
  mixer_input_audio_data_.clear();

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
  current_audio_delay_milliseconds_ = audio_delay_milliseconds;

  if (resampler_.get())
    resampler_->Resample(audio_data, number_of_frames);
  else
    ProvideInput(audio_data, number_of_frames);

  // Always return the full number of frames requested, ProvideInput() will pad
  // with silence if it wasn't able to acquire enough data.
  return number_of_frames;
}

void AudioRendererMixer::ProvideInput(const std::vector<float*>& audio_data,
                                      int number_of_frames) {
  base::AutoLock auto_lock(mixer_inputs_lock_);

  // Allocate staging area for each mixer input's audio data on first call.  We
  // won't know how much to allocate until here because of resampling.
  if (mixer_input_audio_data_.size() == 0) {
    // TODO(dalecurtis): If we switch to AVX/SSE optimization, we'll need to
    // allocate these on 32-byte boundaries and ensure they're sized % 32 bytes.
    mixer_input_audio_data_.reserve(audio_data.size());
    for (size_t i = 0; i < audio_data.size(); ++i)
      mixer_input_audio_data_.push_back(new float[number_of_frames]);
    mixer_input_audio_data_size_ = number_of_frames;
  }

  // Sanity check our inputs.
  DCHECK_LE(number_of_frames, mixer_input_audio_data_size_);
  DCHECK_EQ(audio_data.size(), mixer_input_audio_data_.size());

  // Zero |audio_data| so we're mixing into a clean buffer and return silence if
  // we couldn't get enough data from our inputs.
  for (size_t i = 0; i < audio_data.size(); ++i)
    memset(audio_data[i], 0, number_of_frames * sizeof(*audio_data[i]));

  // Have each mixer render its data into an output buffer then mix the result.
  for (AudioRendererMixerInputSet::iterator it = mixer_inputs_.begin();
       it != mixer_inputs_.end(); ++it) {
    const scoped_refptr<AudioRendererMixerInput>& input = *it;

    double volume;
    input->GetVolume(&volume);

    // Nothing to do if the input isn't playing.
    if (!input->playing())
      continue;

    int frames_filled = input->callback()->Render(
        mixer_input_audio_data_, number_of_frames,
        current_audio_delay_milliseconds_);
    if (frames_filled == 0)
      continue;

    // Volume adjust and mix each mixer input into |audio_data| after rendering.
    // TODO(dalecurtis): Optimize with NEON/SSE/AVX vector_fmac from FFmpeg.
    for (size_t j = 0; j < audio_data.size(); ++j) {
      float* dest = audio_data[j];
      float* source = mixer_input_audio_data_[j];
      for (int k = 0; k < frames_filled; ++k)
        dest[k] += source[k] * static_cast<float>(volume);
    }

    // No need to clamp values as InterleaveFloatToInt() will take care of this
    // for us later when data is transferred to the browser process.
  }
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
