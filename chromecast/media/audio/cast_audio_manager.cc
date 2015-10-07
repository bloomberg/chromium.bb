// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_manager.h"

#include "chromecast/media/audio/cast_audio_output_stream.h"
#include "chromecast/media/base/media_message_loop.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/media/media_pipeline_backend.h"

namespace {
// TODO(alokp): Query the preferred value from media backend.
const int kDefaultSampleRate = 48000;

// Define bounds for the output buffer size (in frames).
// Note: These values are copied from AudioManagerPulse implementation.
// TODO(alokp): Query the preferred value from media backend.
static const int kMinimumOutputBufferSize = 512;
static const int kMaximumOutputBufferSize = 8192;
static const int kDefaultOutputBufferSize = 2048;
}  // namespace

namespace chromecast {
namespace media {

CastAudioManager::CastAudioManager(::media::AudioLogFactory* audio_log_factory)
    : AudioManagerBase(audio_log_factory) {}

CastAudioManager::~CastAudioManager() {
  Shutdown();
}

bool CastAudioManager::HasAudioOutputDevices() {
  return true;
}

bool CastAudioManager::HasAudioInputDevices() {
  return false;
}

void CastAudioManager::ShowAudioInputSettings() {
  LOG(WARNING) << "No support for input audio devices";
}

void CastAudioManager::GetAudioInputDeviceNames(
    ::media::AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  LOG(WARNING) << "No support for input audio devices";
}

::media::AudioParameters CastAudioManager::GetInputStreamParameters(
    const std::string& device_id) {
  LOG(WARNING) << "No support for input audio devices";
  // Need to send a valid AudioParameters object even when it will unused.
  return ::media::AudioParameters(
      ::media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      ::media::CHANNEL_LAYOUT_STEREO, 48000, 16, 1024);
}

scoped_ptr<MediaPipelineBackend> CastAudioManager::CreateMediaPipelineBackend(
    const MediaPipelineDeviceParams& params) {
  DCHECK(media::MediaMessageLoop::GetTaskRunner()->BelongsToCurrentThread());

  return scoped_ptr<MediaPipelineBackend>(
      CastMediaShlib::CreateMediaPipelineBackend(params));
}

::media::AudioOutputStream* CastAudioManager::MakeLinearOutputStream(
    const ::media::AudioParameters& params) {
  DCHECK_EQ(::media::AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return new CastAudioOutputStream(params, this);
}

::media::AudioOutputStream* CastAudioManager::MakeLowLatencyOutputStream(
    const ::media::AudioParameters& params,
    const std::string& device_id) {
  DCHECK_EQ(::media::AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return new CastAudioOutputStream(params, this);
}

::media::AudioInputStream* CastAudioManager::MakeLinearInputStream(
    const ::media::AudioParameters& params,
    const std::string& device_id) {
  LOG(WARNING) << "No support for input audio devices";
  return nullptr;
}

::media::AudioInputStream* CastAudioManager::MakeLowLatencyInputStream(
    const ::media::AudioParameters& params,
    const std::string& device_id) {
  LOG(WARNING) << "No support for input audio devices";
  return nullptr;
}

::media::AudioParameters CastAudioManager::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const ::media::AudioParameters& input_params) {
  ::media::ChannelLayout channel_layout = ::media::CHANNEL_LAYOUT_STEREO;
  int sample_rate = kDefaultSampleRate;
  int buffer_size = kDefaultOutputBufferSize;
  int bits_per_sample = 16;
  if (input_params.IsValid()) {
    // Do not change:
    // - the channel layout
    // - the number of bits per sample
    // We support stereo only with 16 bits per sample.
    sample_rate = input_params.sample_rate();
    buffer_size = std::min(
        kMaximumOutputBufferSize,
        std::max(kMinimumOutputBufferSize, input_params.frames_per_buffer()));
  }

  ::media::AudioParameters output_params(
      ::media::AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout,
      sample_rate, bits_per_sample, buffer_size);
  return output_params;
}

}  // namespace media
}  // namespace chromecast
