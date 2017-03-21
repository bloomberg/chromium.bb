// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_SYSTEM_H_
#define MEDIA_AUDIO_AUDIO_SYSTEM_H_

#include "base/callback.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_export.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {
class AudioManager;

// Work in progress: Provides asynchronous interface to AudioManager. All the
// AudioManager clients will be switched to it, in preparation for moving
// to Mojo audio service.
class MEDIA_EXPORT AudioSystem {
 public:
  // Replies are asynchronously sent from audio system thread to the thread the
  // call is issued on. Attention! Since audio system thread may outlive all the
  // others, callbacks must always be bound to weak pointers!
  using OnAudioParamsCallback = base::Callback<void(const AudioParameters&)>;
  using OnBoolCallback = base::Callback<void(bool)>;
  using OnDeviceDescriptionsCallback =
      base::Callback<void(AudioDeviceDescriptions)>;

  static AudioSystem* Get();

  virtual ~AudioSystem();

  // Callback may receive invalid parameters, it means the specified device is
  // not found. This is best-effort: valid parameters do not guarantee existance
  // of the device.
  // TODO(olka,tommi): fix all AudioManager implementations to return invalid
  // parameters if the device is not found.
  virtual void GetInputStreamParameters(
      const std::string& device_id,
      OnAudioParamsCallback on_params_cb) const = 0;

  // If media::AudioDeviceDescription::IsDefaultDevice(device_id) is true,
  // callback will receive the parameters of the default output device.
  // Callback may receive invalid parameters, it means the specified device is
  // not found. This is best-effort: valid parameters do not guarantee existance
  // of the device.
  // TODO(olka,tommi): fix all AudioManager implementations to return invalid
  // parameters if the device is not found.
  virtual void GetOutputStreamParameters(
      const std::string& device_id,
      OnAudioParamsCallback on_params_cb) const = 0;

  virtual void HasInputDevices(OnBoolCallback on_has_devices_cb) const = 0;

  // Replies with device descriptions of input audio devices if |for_input| is
  // true, and of output audio devices otherwise.
  virtual void GetDeviceDescriptions(
      OnDeviceDescriptionsCallback on_descriptions_cp,
      bool for_input) = 0;

  virtual base::SingleThreadTaskRunner* GetTaskRunner() const = 0;

  // Must not be used for anything but stream creation.
  virtual AudioManager* GetAudioManager() const = 0;

 protected:
  // Sets the global AudioSystem pointer to the specified non-null value.
  static void SetInstance(AudioSystem* audio_system);

  // Sets the global AudioSystem pointer to null if it equals the specified one.
  static void ClearInstance(const AudioSystem* audio_system);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_SYSTEM_H_
