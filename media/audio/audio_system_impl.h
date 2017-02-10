// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_SYSTEM_IMPL_H_
#define MEDIA_AUDIO_AUDIO_SYSTEM_IMPL_H_

#include "media/audio/audio_system.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {
class AudioManager;

class MEDIA_EXPORT AudioSystemImpl : public AudioSystem {
 public:
  static std::unique_ptr<AudioSystem> Create(AudioManager* audio_manager);

  ~AudioSystemImpl() override;

  // AudioSystem implementation.
  void GetInputStreamParameters(
      const std::string& device_id,
      OnAudioParamsCallback on_params_cb) const override;
  void HasInputDevices(OnBoolCallback on_has_devices_cb) const override;
  AudioManager* GetAudioManager() const override;

 protected:
  AudioSystemImpl(AudioManager* audio_manager);

 private:
  base::SingleThreadTaskRunner* GetTaskRunner() const;

  AudioManager* const audio_manager_;

  DISALLOW_COPY_AND_ASSIGN(AudioSystemImpl);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_SYSTEM_IMPL_H_
