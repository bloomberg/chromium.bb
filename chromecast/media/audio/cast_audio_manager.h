// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_H_
#define CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_H_

#include "base/macros.h"
#include "media/audio/audio_manager_base.h"

namespace chromecast {

class TaskRunnerImpl;

namespace media {

class MediaPipelineBackend;
struct MediaPipelineDeviceParams;

class CastAudioManager : public ::media::AudioManagerBase {
 public:
  explicit CastAudioManager(::media::AudioLogFactory* audio_log_factory);
  ~CastAudioManager() override;

  // AudioManager implementation.
  bool HasAudioOutputDevices() override;
  bool HasAudioInputDevices() override;
  void ShowAudioInputSettings() override;
  void GetAudioInputDeviceNames(
      ::media::AudioDeviceNames* device_names) override;
  ::media::AudioParameters GetInputStreamParameters(
      const std::string& device_id) override;

  // This must be called on cast media thread.
  // See chromecast::media::MediaMessageLoop.
  virtual scoped_ptr<MediaPipelineBackend> CreateMediaPipelineBackend(
      const MediaPipelineDeviceParams& params);

 private:
  // AudioManagerBase implementation.
  ::media::AudioOutputStream* MakeLinearOutputStream(
      const ::media::AudioParameters& params) override;
  ::media::AudioOutputStream* MakeLowLatencyOutputStream(
      const ::media::AudioParameters& params,
      const std::string& device_id) override;
  ::media::AudioInputStream* MakeLinearInputStream(
      const ::media::AudioParameters& params,
      const std::string& device_id) override;
  ::media::AudioInputStream* MakeLowLatencyInputStream(
      const ::media::AudioParameters& params,
      const std::string& device_id) override;
  ::media::AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const ::media::AudioParameters& input_params) override;

  DISALLOW_COPY_AND_ASSIGN(CastAudioManager);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_H_
