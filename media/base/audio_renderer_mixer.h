// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_RENDERER_MIXER_H_
#define MEDIA_BASE_AUDIO_RENDERER_MIXER_H_

#include <list>

#include "base/synchronization/lock.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_renderer_mixer_input.h"
#include "media/base/audio_renderer_sink.h"

namespace media {

// Mixes a set of AudioRendererMixerInputs into a single output stream which is
// funneled into a single shared AudioRendererSink; saving a bundle on renderer
// side resources.
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
  virtual int Render(AudioBus* audio_bus,
                     int audio_delay_milliseconds) OVERRIDE;
  virtual void OnRenderError() OVERRIDE;

  // Output sink for this mixer.
  scoped_refptr<AudioRendererSink> audio_sink_;

  // Set of mixer inputs to be mixed by this mixer.  Access is thread-safe
  // through |mixer_inputs_lock_|.
  typedef std::list<scoped_refptr<AudioRendererMixerInput> >
      AudioRendererMixerInputSet;
  AudioRendererMixerInputSet mixer_inputs_;
  base::Lock mixer_inputs_lock_;

  // Handles mixing and resampling between input and output parameters.
  AudioConverter audio_converter_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererMixer);
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_RENDERER_MIXER_H_
