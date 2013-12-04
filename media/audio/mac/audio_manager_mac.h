// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_AUDIO_MANAGER_MAC_H_
#define MEDIA_AUDIO_MAC_AUDIO_MANAGER_MAC_H_

#include <CoreAudio/AudioHardware.h>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop_proxy.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/mac/aggregate_device_manager.h"
#include "media/audio/mac/audio_device_listener_mac.h"

namespace media {

// Mac OS X implementation of the AudioManager singleton. This class is internal
// to the audio output and only internal users can call methods not exposed by
// the AudioManager class.
class MEDIA_EXPORT AudioManagerMac : public AudioManagerBase {
 public:
  AudioManagerMac(AudioLogFactory* audio_log_factory);

  // Implementation of AudioManager.
  virtual bool HasAudioOutputDevices() OVERRIDE;
  virtual bool HasAudioInputDevices() OVERRIDE;
  virtual void GetAudioInputDeviceNames(
      AudioDeviceNames* device_names) OVERRIDE;
  virtual void GetAudioOutputDeviceNames(
      AudioDeviceNames* device_names) OVERRIDE;
  virtual AudioParameters GetInputStreamParameters(
      const std::string& device_id) OVERRIDE;
  virtual std::string GetAssociatedOutputDeviceID(
      const std::string& input_device_id) OVERRIDE;

  // Implementation of AudioManagerBase.
  virtual AudioOutputStream* MakeLinearOutputStream(
      const AudioParameters& params) OVERRIDE;
  virtual AudioOutputStream* MakeLowLatencyOutputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const std::string& input_device_id) OVERRIDE;
  virtual AudioInputStream* MakeLinearInputStream(
      const AudioParameters& params, const std::string& device_id) OVERRIDE;
  virtual AudioInputStream* MakeLowLatencyInputStream(
      const AudioParameters& params, const std::string& device_id) OVERRIDE;
  virtual std::string GetDefaultOutputDeviceID() OVERRIDE;

  static bool GetDefaultInputDevice(AudioDeviceID* device);
  static bool GetDefaultOutputDevice(AudioDeviceID* device);
  static bool GetDefaultDevice(AudioDeviceID* device, bool input);

  static bool GetDefaultOutputChannels(int* channels);

  static bool GetDeviceChannels(AudioDeviceID device,
                                AudioObjectPropertyScope scope,
                                int* channels);

  static int HardwareSampleRateForDevice(AudioDeviceID device_id);
  static int HardwareSampleRate();

 protected:
  virtual ~AudioManagerMac();

  virtual AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) OVERRIDE;

 private:
  bool HasUnifiedDefaultIO();

  // Helper methods for constructing AudioDeviceListenerMac on the audio thread.
  void CreateDeviceListener();
  void DestroyDeviceListener();

  int ChooseBufferSize(int output_sample_rate);

  // Notify streams of a device change if the default output device or its
  // sample rate has changed, otherwise does nothing.
  void HandleDeviceChanges();

  scoped_ptr<AudioDeviceListenerMac> output_device_listener_;

  // Track the output sample-rate and the default output device
  // so we can intelligently handle device notifications only when necessary.
  int current_sample_rate_;
  AudioDeviceID current_output_device_;

  AggregateDeviceManager aggregate_device_manager_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerMac);
};

}  // namespace media

#endif  // MEDIA_AUDIO_MAC_AUDIO_MANAGER_MAC_H_
