// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_H_
#define CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_H_

#include "base/macros.h"
#include "media/audio/audio_manager_base.h"

namespace chromecast {

namespace media {

class CastAudioMixer;
class MediaPipelineBackend;
class MediaPipelineBackendManager;
struct MediaPipelineDeviceParams;

class CastAudioManager : public ::media::AudioManagerBase {
 public:
  CastAudioManager(std::unique_ptr<::media::AudioThread> audio_thread,
                   ::media::AudioLogFactory* audio_log_factory,
                   MediaPipelineBackendManager* backend_manager);
  CastAudioManager(std::unique_ptr<::media::AudioThread> audio_thread,
                   ::media::AudioLogFactory* audio_log_factory,
                   MediaPipelineBackendManager* backend_manager,
                   CastAudioMixer* audio_mixer);
  ~CastAudioManager() override;

  // AudioManager implementation.
  bool HasAudioOutputDevices() override;
  bool HasAudioInputDevices() override;
  void ShowAudioInputSettings() override;
  void GetAudioInputDeviceNames(
      ::media::AudioDeviceNames* device_names) override;
  ::media::AudioParameters GetInputStreamParameters(
      const std::string& device_id) override;
  const char* GetName() override;

  // AudioManagerBase implementation
  void ReleaseOutputStream(::media::AudioOutputStream* stream) override;

  // This must be called on audio thread.
  virtual std::unique_ptr<MediaPipelineBackend> CreateMediaPipelineBackend(
      const MediaPipelineDeviceParams& params);

 private:
  // AudioManagerBase implementation.
  ::media::AudioOutputStream* MakeLinearOutputStream(
      const ::media::AudioParameters& params,
      const ::media::AudioManager::LogCallback& log_callback) override;
  ::media::AudioOutputStream* MakeLowLatencyOutputStream(
      const ::media::AudioParameters& params,
      const std::string& device_id,
      const ::media::AudioManager::LogCallback& log_callback) override;
  ::media::AudioInputStream* MakeLinearInputStream(
      const ::media::AudioParameters& params,
      const std::string& device_id,
      const ::media::AudioManager::LogCallback& log_callback) override;
  ::media::AudioInputStream* MakeLowLatencyInputStream(
      const ::media::AudioParameters& params,
      const std::string& device_id,
      const ::media::AudioManager::LogCallback& log_callback) override;
  ::media::AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const ::media::AudioParameters& input_params) override;

  // Generates a CastAudioOutputStream for |mixer_|.
  ::media::AudioOutputStream* MakeMixerOutputStream(
      const ::media::AudioParameters& params);

  MediaPipelineBackendManager* const backend_manager_;
  std::unique_ptr<::media::AudioOutputStream> mixer_output_stream_;
  std::unique_ptr<CastAudioMixer> mixer_;

  DISALLOW_COPY_AND_ASSIGN(CastAudioManager);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_H_
