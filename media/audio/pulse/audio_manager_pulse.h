// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_PULSE_AUDIO_MANAGER_PULSE_H_
#define MEDIA_AUDIO_PULSE_AUDIO_MANAGER_PULSE_H_

#include <pulse/pulseaudio.h>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "media/audio/audio_manager_base.h"

namespace media {

class MEDIA_EXPORT AudioManagerPulse : public AudioManagerBase {
 public:
  AudioManagerPulse();
  virtual ~AudioManagerPulse();

  static AudioManager* Create();

  // Implementation of AudioManager.
  virtual bool HasAudioOutputDevices() OVERRIDE;
  virtual bool HasAudioInputDevices() OVERRIDE;
  virtual void ShowAudioInputSettings() OVERRIDE;
  virtual void GetAudioInputDeviceNames(media::AudioDeviceNames* device_names)
      OVERRIDE;

  // Implementation of AudioManagerBase.
  virtual AudioOutputStream* MakeLinearOutputStream(
      const AudioParameters& params) OVERRIDE;
  virtual AudioOutputStream* MakeLowLatencyOutputStream(
      const AudioParameters& params) OVERRIDE;
  virtual AudioInputStream* MakeLinearInputStream(
      const AudioParameters& params, const std::string& device_id) OVERRIDE;
  virtual AudioInputStream* MakeLowLatencyInputStream(
      const AudioParameters& params, const std::string& device_id) OVERRIDE;
  virtual AudioParameters GetPreferredLowLatencyOutputStreamParameters(
      const AudioParameters& input_params) OVERRIDE;

  int GetNativeSampleRate();

 private:
  bool Init();
  void DestroyPulse();

  // Callback to get the devices' info like names, used by GetInputDevices().
  static void DevicesInfoCallback(pa_context* context,
                                  const pa_source_info* info,
                                  int error, void* user_data);

  // Callback to get the native sample rate of PulseAudio, used by
  // GetNativeSampleRate().
  static void SampleRateInfoCallback(pa_context* context,
                                     const pa_server_info* info,
                                     void* user_data);

  // Called by MakeLinearOutputStream and MakeLowLatencyOutputStream.
  AudioOutputStream* MakeOutputStream(const AudioParameters& params);

  // Called by MakeLinearInputStream and MakeLowLatencyInputStream.
  AudioInputStream* MakeInputStream(const AudioParameters& params,
                                    const std::string& device_id);

  pa_threaded_mainloop* input_mainloop_;
  pa_context* input_context_;
  AudioDeviceNames* devices_;
  int native_input_sample_rate_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerPulse);
};

}  // namespace media

#endif  // MEDIA_AUDIO_PULSE_AUDIO_MANAGER_PULSE_H_
