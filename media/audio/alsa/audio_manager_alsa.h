// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_ALSA_AUDIO_MANAGER_ALSA_H_
#define MEDIA_AUDIO_ALSA_AUDIO_MANAGER_ALSA_H_

#include <string>
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "media/audio/audio_manager_base.h"

namespace media {

class AlsaWrapper;

class MEDIA_EXPORT AudioManagerAlsa : public AudioManagerBase {
 public:
  AudioManagerAlsa(AudioLogFactory* audio_log_factory);

  static void ShowLinuxAudioInputSettings();

  // Implementation of AudioManager.
  virtual bool HasAudioOutputDevices() override;
  virtual bool HasAudioInputDevices() override;
  virtual void ShowAudioInputSettings() override;
  virtual void GetAudioInputDeviceNames(
      AudioDeviceNames* device_names) override;
  virtual void GetAudioOutputDeviceNames(
      AudioDeviceNames* device_names) override;
  virtual AudioParameters GetInputStreamParameters(
      const std::string& device_id) override;

  // Implementation of AudioManagerBase.
  virtual AudioOutputStream* MakeLinearOutputStream(
      const AudioParameters& params) override;
  virtual AudioOutputStream* MakeLowLatencyOutputStream(
      const AudioParameters& params,
      const std::string& device_id) override;
  virtual AudioInputStream* MakeLinearInputStream(
      const AudioParameters& params, const std::string& device_id) override;
  virtual AudioInputStream* MakeLowLatencyInputStream(
      const AudioParameters& params, const std::string& device_id) override;

 protected:
  virtual ~AudioManagerAlsa();

  virtual AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) override;

 private:
  enum StreamType {
    kStreamPlayback = 0,
    kStreamCapture,
  };

  // Gets a list of available ALSA devices.
  void GetAlsaAudioDevices(StreamType type,
                           media::AudioDeviceNames* device_names);

  // Gets the ALSA devices' names and ids that support streams of the
  // given type.
  void GetAlsaDevicesInfo(StreamType type,
                          void** hint,
                          media::AudioDeviceNames* device_names);

  // Checks if the specific ALSA device is available.
  static bool IsAlsaDeviceAvailable(StreamType type,
                                    const char* device_name);

  static const char* UnwantedDeviceTypeWhenEnumerating(
      StreamType wanted_type);

  // Returns true if a device is present for the given stream type.
  bool HasAnyAlsaAudioDevice(StreamType stream);

  // Called by MakeLinearOutputStream and MakeLowLatencyOutputStream.
  AudioOutputStream* MakeOutputStream(const AudioParameters& params);

  // Called by MakeLinearInputStream and MakeLowLatencyInputStream.
  AudioInputStream* MakeInputStream(const AudioParameters& params,
                                    const std::string& device_id);

  scoped_ptr<AlsaWrapper> wrapper_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerAlsa);
};

}  // namespace media

#endif  // MEDIA_AUDIO_ALSA_AUDIO_MANAGER_ALSA_H_
