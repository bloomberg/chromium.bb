// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_RENDERER_MIXER_H_
#define MEDIA_BASE_AUDIO_RENDERER_MIXER_H_

#include <set>
#include <vector>

#include "base/synchronization/lock.h"
#include "media/base/audio_renderer_mixer_input.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/multi_channel_resampler.h"

namespace media {

// Mixes a set of AudioRendererMixerInputs into a single output stream which is
// funneled into a single shared AudioRendererSink; saving a bundle on renderer
// side resources.  Resampling is done post-mixing as it is the most expensive
// process.  If the input sample rate matches the audio hardware sample rate, no
// resampling is done.
class MEDIA_EXPORT AudioRendererMixer
    : NON_EXPORTED_BASE(public AudioRendererSink::RenderCallback) {
 public:
  AudioRendererMixer(const AudioParameters& input_params,
                     const AudioParameters& output_params,
                     const scoped_refptr<AudioRendererSink>& sink);
  virtual ~AudioRendererMixer();

  // Add or remove a mixer input from mixing; called by AudioRendererMixerInput.
  void AddMixerInput(const scoped_refptr<AudioRendererMixerInput>& input);
  void RemoveMixerInput(const scoped_refptr<AudioRendererMixerInput>& input);

 private:
  // AudioRendererSink::RenderCallback implementation.
  virtual int Render(const std::vector<float*>& audio_data,
                     int number_of_frames,
                     int audio_delay_milliseconds) OVERRIDE;
  virtual void OnRenderError() OVERRIDE;

  // Handles mixing and volume adjustment.  Renders |number_of_frames| into
  // |audio_data|.  When resampling is necessary, ProvideInput() will be called
  // by MultiChannelResampler when more data is necessary.
  void ProvideInput(const std::vector<float*>& audio_data,
                    int number_of_frames);

  // Output sink for this mixer.
  scoped_refptr<AudioRendererSink> audio_sink_;

  // Set of mixer inputs to be mixed by this mixer.  Access is thread-safe
  // through |mixer_inputs_lock_|.
  typedef std::set< scoped_refptr<AudioRendererMixerInput> >
      AudioRendererMixerInputSet;
  AudioRendererMixerInputSet mixer_inputs_;
  base::Lock mixer_inputs_lock_;

  // Vector for rendering audio data from each mixer input.
  int mixer_input_audio_data_size_;
  std::vector<float*> mixer_input_audio_data_;

  // Handles resampling post-mixing.
  scoped_ptr<MultiChannelResampler> resampler_;

  // The audio delay in milliseconds received by the last Render() call.
  int current_audio_delay_milliseconds_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererMixer);
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_RENDERER_MIXER_H_
