// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/audio_pipeline_device_default.h"

#include "chromecast/media/cma/backend/media_component_device_default.h"

namespace chromecast {
namespace media {

AudioPipelineDeviceDefault::AudioPipelineDeviceDefault(
    const MediaPipelineDeviceParams& params,
    MediaClockDevice* media_clock_device)
    : pipeline_(new MediaComponentDeviceDefault(params, media_clock_device)) {
  thread_checker_.DetachFromThread();
}

AudioPipelineDeviceDefault::~AudioPipelineDeviceDefault() {
}

void AudioPipelineDeviceDefault::SetClient(Client* client) {
  pipeline_->SetClient(client);
}

MediaComponentDevice::State AudioPipelineDeviceDefault::GetState() const {
  return pipeline_->GetState();
}

bool AudioPipelineDeviceDefault::SetState(State new_state) {
  bool success = pipeline_->SetState(new_state);
  if (!success)
    return false;

  if (new_state == kStateIdle) {
    DCHECK(IsValidConfig(config_));
  }
  if (new_state == kStateUninitialized) {
    config_ = AudioConfig();
  }
  return true;
}

bool AudioPipelineDeviceDefault::SetStartPts(int64_t time_microseconds) {
  return pipeline_->SetStartPts(time_microseconds);
}

MediaComponentDevice::FrameStatus AudioPipelineDeviceDefault::PushFrame(
    DecryptContext* decrypt_context,
    CastDecoderBuffer* buffer,
    FrameStatusCB* completion_cb) {
  return pipeline_->PushFrame(decrypt_context, buffer, completion_cb);
}

AudioPipelineDeviceDefault::RenderingDelay
AudioPipelineDeviceDefault::GetRenderingDelay() const {
  return pipeline_->GetRenderingDelay();
}

bool AudioPipelineDeviceDefault::SetConfig(const AudioConfig& config) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!IsValidConfig(config))
    return false;
  config_ = config;
  if (config.extra_data_size > 0)
    config_extra_data_.assign(config.extra_data,
                              config.extra_data + config.extra_data_size);
  else
    config_extra_data_.clear();
  return true;
}

void AudioPipelineDeviceDefault::SetStreamVolumeMultiplier(float multiplier) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool AudioPipelineDeviceDefault::GetStatistics(Statistics* stats) const {
  return pipeline_->GetStatistics(stats);
}

}  // namespace media
}  // namespace chromecast
