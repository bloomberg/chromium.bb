// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_SYSTEM_H_
#define MEDIA_AUDIO_AUDIO_SYSTEM_H_

#include <string>

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
  // call is issued on. Attention! Audio system thread may outlive the client
  // objects; bind callbacks with care.
  using OnAudioParamsCallback =
      base::OnceCallback<void(const AudioParameters&)>;
  using OnBoolCallback = base::OnceCallback<void(bool)>;
  using OnDeviceDescriptionsCallback =
      base::OnceCallback<void(AudioDeviceDescriptions)>;
  using OnDeviceIdCallback = base::OnceCallback<void(const std::string&)>;
  using OnInputDeviceInfoCallback = base::OnceCallback<
      void(const AudioParameters&, const AudioParameters&, const std::string&)>;

  // Must not be called on audio system thread if it differs from the one
  // AudioSystem is destroyed on. See http://crbug.com/705455.
  static AudioSystem* Get();

  virtual ~AudioSystem();

  // Callback may receive invalid parameters, it means the specified device is
  // not found. This is best-effort: valid parameters do not guarantee existence
  // of the device.
  // TODO(olka,tommi): fix all AudioManager implementations to return invalid
  // parameters if the device is not found.
  virtual void GetInputStreamParameters(
      const std::string& device_id,
      OnAudioParamsCallback on_params_cb) const = 0;

  // If media::AudioDeviceDescription::IsDefaultDevice(device_id) is true,
  // callback will receive the parameters of the default output device.
  // Callback may receive invalid parameters, it means the specified device is
  // not found. This is best-effort: valid parameters do not guarantee existence
  // of the device.
  // TODO(olka,tommi): fix all AudioManager implementations to return invalid
  // parameters if the device is not found.
  virtual void GetOutputStreamParameters(
      const std::string& device_id,
      OnAudioParamsCallback on_params_cb) const = 0;

  virtual void HasInputDevices(OnBoolCallback on_has_devices_cb) const = 0;

  virtual void HasOutputDevices(OnBoolCallback on_has_devices_cb) const = 0;

  // Replies with device descriptions of input audio devices if |for_input| is
  // true, and of output audio devices otherwise.
  virtual void GetDeviceDescriptions(
      OnDeviceDescriptionsCallback on_descriptions_cb,
      bool for_input) = 0;

  // Replies with an empty string if there is no associated output device found.
  virtual void GetAssociatedOutputDeviceID(
      const std::string& input_device_id,
      OnDeviceIdCallback on_device_id_cb) = 0;

  // Replies with audio parameters for the specified input device and audio
  // parameters and device ID of the associated output device, if any (otherwise
  // it's AudioParameters() and an empty string).
  virtual void GetInputDeviceInfo(
      const std::string& input_device_id,
      OnInputDeviceInfoCallback on_input_device_info_cb) = 0;

  virtual base::SingleThreadTaskRunner* GetTaskRunner() const = 0;

 protected:
  // Sets the global AudioSystem pointer to the specified non-null value.
  static void SetInstance(AudioSystem* audio_system);

  // Sets the global AudioSystem pointer to null if it equals the specified one.
  static void ClearInstance(const AudioSystem* audio_system);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_SYSTEM_H_
