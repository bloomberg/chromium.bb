// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_renderer_mixer_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/renderer_audio_output_device.h"
#include "media/base/audio_renderer_mixer.h"
#include "media/base/audio_renderer_mixer_input.h"

namespace content {

AudioRendererMixerManager::AudioRendererMixerManager(int hardware_sample_rate,
                                                     int hardware_buffer_size)
    : hardware_sample_rate_(hardware_sample_rate),
      hardware_buffer_size_(hardware_buffer_size),
      sink_for_testing_(NULL) {
}

AudioRendererMixerManager::~AudioRendererMixerManager() {
  DCHECK(mixers_.empty());
}

media::AudioRendererMixerInput* AudioRendererMixerManager::CreateInput(
    int source_render_view_id) {
  return new media::AudioRendererMixerInput(
      base::Bind(
          &AudioRendererMixerManager::GetMixer, base::Unretained(this),
          source_render_view_id),
      base::Bind(
          &AudioRendererMixerManager::RemoveMixer, base::Unretained(this),
          source_render_view_id));
}

void AudioRendererMixerManager::SetAudioRendererSinkForTesting(
    media::AudioRendererSink* sink) {
  sink_for_testing_ = sink;
}

media::AudioRendererMixer* AudioRendererMixerManager::GetMixer(
    int source_render_view_id,
    const media::AudioParameters& params) {
  const MixerKey key(source_render_view_id, params);
  base::AutoLock auto_lock(mixers_lock_);

  AudioRendererMixerMap::iterator it = mixers_.find(key);
  if (it != mixers_.end()) {
    it->second.ref_count++;
    return it->second.mixer;
  }

  // On Linux and ChromeOS we can rely on the playback device to handle
  // resampling, so don't waste cycles on it here.
#if defined(OS_LINUX)
  int sample_rate = params.sample_rate();
#else
  int sample_rate = hardware_sample_rate_;
#endif

  // Create output parameters based on the audio hardware configuration for
  // passing on to the output sink.  Force to 16-bit output for now since we
  // know that works well for WebAudio and WebRTC.
  media::AudioParameters output_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY, params.channel_layout(),
      sample_rate, 16, hardware_buffer_size_);

  // If we've created invalid output parameters, simply pass on the input params
  // and let the browser side handle automatic fallback.
  if (!output_params.IsValid())
    output_params = params;

  media::AudioRendererMixer* mixer;
  if (sink_for_testing_) {
    mixer = new media::AudioRendererMixer(
        params, output_params, sink_for_testing_);
  } else {
    scoped_refptr<RendererAudioOutputDevice> device =
        AudioDeviceFactory::NewOutputDevice();
    device->SetSourceRenderView(source_render_view_id);
    mixer = new media::AudioRendererMixer(params, output_params, device);
  }

  AudioRendererMixerReference mixer_reference = { mixer, 1 };
  mixers_[key] = mixer_reference;
  return mixer;
}

void AudioRendererMixerManager::RemoveMixer(
    int source_render_view_id,
    const media::AudioParameters& params) {
  const MixerKey key(source_render_view_id, params);
  base::AutoLock auto_lock(mixers_lock_);

  AudioRendererMixerMap::iterator it = mixers_.find(key);
  DCHECK(it != mixers_.end());

  // Only remove the mixer if AudioRendererMixerManager is the last owner.
  it->second.ref_count--;
  if (it->second.ref_count == 0) {
    delete it->second.mixer;
    mixers_.erase(it);
  }
}

}  // namespace content
