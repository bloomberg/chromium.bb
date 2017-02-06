// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_SYSTEM_H_
#define MEDIA_AUDIO_AUDIO_SYSTEM_H_

#include "base/callback.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_export.h"

namespace media {
class AudioManager;

// Work in progress: Provides asynchronous interface to AudioManager. All the
// AudioManager clients will be switched to it, in preparation for moving
// to Mojo audio service.
class MEDIA_EXPORT AudioSystem {
 public:
  // Replies are asynchronously sent to the thread the call is issued on.
  using OnAudioParamsCallback = base::Callback<void(const AudioParameters&)>;

  virtual ~AudioSystem(){};

  // Callback will receive invalid parameters if the device is not found.
  virtual void GetInputStreamParameters(
      const std::string& device_id,
      OnAudioParamsCallback on_params_cb) const = 0;

  // Must not be used for anything but stream creation.
  virtual AudioManager* GetAudioManager() const = 0;
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_SYSTEM_H_
