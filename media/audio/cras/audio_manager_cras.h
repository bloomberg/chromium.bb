// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_CRAS_AUDIO_MANAGER_CRAS_H_
#define MEDIA_AUDIO_CRAS_AUDIO_MANAGER_CRAS_H_

#include <cras_types.h>

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/audio/audio_manager_base.h"

namespace media {

class MEDIA_EXPORT AudioManagerCras : public AudioManagerBase {
 public:
  AudioManagerCras(std::unique_ptr<AudioThread> audio_thread,
                   AudioLogFactory* audio_log_factory);
  ~AudioManagerCras() override;

  // AudioManager implementation.
  bool HasAudioOutputDevices() override;
  bool HasAudioInputDevices() override;
  void ShowAudioInputSettings() override;
  void GetAudioInputDeviceNames(AudioDeviceNames* device_names) override;
  void GetAudioOutputDeviceNames(AudioDeviceNames* device_names) override;
  AudioParameters GetInputStreamParameters(
      const std::string& device_id) override;
  std::string GetAssociatedOutputDeviceID(
      const std::string& input_device_id) override;
  std::string GetDefaultOutputDeviceID() override;
  const char* GetName() override;

  // AudioManagerBase implementation.
  AudioOutputStream* MakeLinearOutputStream(
      const AudioParameters& params,
      const LogCallback& log_callback) override;
  AudioOutputStream* MakeLowLatencyOutputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  AudioInputStream* MakeLinearInputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  AudioInputStream* MakeLowLatencyInputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;

  static snd_pcm_format_t BitsToFormat(int bits_per_sample);

  // Checks if |device_id| corresponds to the default device.
  // Set |is_input| to true for capture devices, false for output.
  bool IsDefault(const std::string& device_id, bool is_input);

 protected:
  AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) override;

 private:
  // Called by MakeLinearOutputStream and MakeLowLatencyOutputStream.
  AudioOutputStream* MakeOutputStream(const AudioParameters& params,
                                      const std::string& device_id);

  // Called by MakeLinearInputStream and MakeLowLatencyInputStream.
  AudioInputStream* MakeInputStream(const AudioParameters& params,
                                    const std::string& device_id);

  // Get minimum output buffer size for this board.
  int GetMinimumOutputBufferSizePerBoard();

  void GetAudioDeviceNamesImpl(bool is_input, AudioDeviceNames* device_names);

  void AddBeamformingDevices(AudioDeviceNames* device_names);

  // Stores the mic positions field from the device.
  std::vector<Point> mic_positions_;

  const char* beamforming_on_device_id_;
  const char* beamforming_off_device_id_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerCras);
};

}  // namespace media

#endif  // MEDIA_AUDIO_CRAS_AUDIO_MANAGER_CRAS_H_
