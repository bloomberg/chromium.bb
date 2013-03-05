// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_CRAS_AUDIO_MANAGER_CRAS_H_
#define MEDIA_AUDIO_CRAS_AUDIO_MANAGER_CRAS_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "media/audio/audio_manager_base.h"

namespace media {

class MEDIA_EXPORT AudioManagerCras : public AudioManagerBase {
 public:
  AudioManagerCras();

  // AudioManager implementation.
  virtual bool HasAudioOutputDevices() OVERRIDE;
  virtual bool HasAudioInputDevices() OVERRIDE;
  virtual void ShowAudioInputSettings() OVERRIDE;
  virtual void GetAudioInputDeviceNames(media::AudioDeviceNames* device_names)
      OVERRIDE;
  virtual AudioParameters GetInputStreamParameters(
      const std::string& device_id) OVERRIDE;

  // AudioManagerBase implementation.
  virtual AudioOutputStream* MakeLinearOutputStream(
      const AudioParameters& params) OVERRIDE;
  virtual AudioOutputStream* MakeLowLatencyOutputStream(
      const AudioParameters& params) OVERRIDE;
  virtual AudioInputStream* MakeLinearInputStream(
      const AudioParameters& params, const std::string& device_id) OVERRIDE;
  virtual AudioInputStream* MakeLowLatencyInputStream(
      const AudioParameters& params, const std::string& device_id) OVERRIDE;

 protected:
  virtual ~AudioManagerCras();

  virtual AudioParameters GetPreferredOutputStreamParameters(
      const AudioParameters& input_params) OVERRIDE;

 private:
  // Gets a list of available cras input devices.
  void GetCrasAudioInputDevices(media::AudioDeviceNames* device_names);

  // Called by MakeLinearOutputStream and MakeLowLatencyOutputStream.
  AudioOutputStream* MakeOutputStream(const AudioParameters& params);

  // Called by MakeLinearInputStream and MakeLowLatencyInputStream.
  AudioInputStream* MakeInputStream(const AudioParameters& params,
                                    const std::string& device_id);

  DISALLOW_COPY_AND_ASSIGN(AudioManagerCras);
};

}  // namespace media

#endif  // MEDIA_AUDIO_CRAS_AUDIO_MANAGER_CRAS_H_
