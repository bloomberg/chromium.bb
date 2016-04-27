// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_renderer_mixer_manager.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "build/build_config.h"
#include "content/renderer/media/audio_device_factory.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_hardware_config.h"
#include "media/base/audio_renderer_mixer.h"
#include "media/base/audio_renderer_mixer_input.h"

namespace content {

AudioRendererMixerManager::AudioRendererMixerManager() {}

AudioRendererMixerManager::~AudioRendererMixerManager() {
  // References to AudioRendererMixers may be owned by garbage collected
  // objects.  During process shutdown they may be leaked, so, transitively,
  // |mixers_| may leak (i.e., may be non-empty at this time) as well.
}

media::AudioRendererMixerInput* AudioRendererMixerManager::CreateInput(
    int source_render_frame_id,
    int session_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  // base::Unretained() is safe since AudioRendererMixerManager lives on the
  // renderer thread and is destroyed on renderer thread destruction.
  return new media::AudioRendererMixerInput(
      base::Bind(&AudioRendererMixerManager::GetMixer, base::Unretained(this),
                 source_render_frame_id),
      base::Bind(&AudioRendererMixerManager::RemoveMixer,
                 base::Unretained(this), source_render_frame_id),
      media::AudioDeviceDescription::UseSessionIdToSelectDevice(session_id,
                                                                device_id)
          ? AudioDeviceFactory::GetOutputDeviceInfo(
                source_render_frame_id, session_id, device_id, security_origin)
                .device_id()
          : device_id,
      security_origin);
}

media::AudioRendererMixer* AudioRendererMixerManager::GetMixer(
    int source_render_frame_id,
    const media::AudioParameters& params,
    const std::string& device_id,
    const url::Origin& security_origin,
    media::OutputDeviceStatus* device_status) {
  // Effects are not passed through to output creation, so ensure none are set.
  DCHECK_EQ(params.effects(), media::AudioParameters::NO_EFFECTS);

  const MixerKey key(source_render_frame_id, params, device_id,
                     security_origin);
  base::AutoLock auto_lock(mixers_lock_);

  AudioRendererMixerMap::iterator it = mixers_.find(key);
  if (it != mixers_.end()) {
    if (device_status)
      *device_status = media::OUTPUT_DEVICE_STATUS_OK;

    it->second.ref_count++;
    return it->second.mixer;
  }

  scoped_refptr<media::AudioRendererSink> sink =
      AudioDeviceFactory::NewAudioRendererMixerSink(source_render_frame_id, 0,
                                                    device_id, security_origin);

  const media::OutputDeviceInfo& device_info = sink->GetOutputDeviceInfo();
  if (device_status)
    *device_status = device_info.device_status();
  if (device_info.device_status() != media::OUTPUT_DEVICE_STATUS_OK) {
    sink->Stop();
    return nullptr;
  }

  // On ChromeOS as well as when a fake device is used, we can rely on the
  // playback device to handle resampling, so don't waste cycles on it here.
  int sample_rate = params.sample_rate();
  int buffer_size =
      media::AudioHardwareConfig::GetHighLatencyBufferSize(sample_rate, 0);

#if !defined(OS_CHROMEOS)
  const media::AudioParameters& hardware_params = device_info.output_params();

  // If we have valid, non-fake hardware parameters, use them.  Otherwise, pass
  // on the input params and let the browser side handle automatic fallback.
  if (hardware_params.format() != media::AudioParameters::AUDIO_FAKE &&
      hardware_params.IsValid()) {
    sample_rate = hardware_params.sample_rate();
    buffer_size = media::AudioHardwareConfig::GetHighLatencyBufferSize(
        sample_rate, hardware_params.frames_per_buffer());
  }
#endif

  // Create output parameters based on the audio hardware configuration for
  // passing on to the output sink.  Force to 16-bit output for now since we
  // know that works everywhere; ChromeOS does not support other bit depths.
  media::AudioParameters output_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY, params.channel_layout(),
      sample_rate, 16, buffer_size);
  DCHECK(output_params.IsValid());

  media::AudioRendererMixer* mixer =
      new media::AudioRendererMixer(output_params, sink);
  AudioRendererMixerReference mixer_reference = { mixer, 1 };
  mixers_[key] = mixer_reference;
  return mixer;
}

void AudioRendererMixerManager::RemoveMixer(
    int source_render_frame_id,
    const media::AudioParameters& params,
    const std::string& device_id,
    const url::Origin& security_origin) {
  const MixerKey key(source_render_frame_id, params, device_id,
                     security_origin);
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

AudioRendererMixerManager::MixerKey::MixerKey(
    int source_render_frame_id,
    const media::AudioParameters& params,
    const std::string& device_id,
    const url::Origin& security_origin)
    : source_render_frame_id(source_render_frame_id),
      params(params),
      device_id(device_id),
      security_origin(security_origin) {}

AudioRendererMixerManager::MixerKey::MixerKey(const MixerKey& other) = default;

}  // namespace content
