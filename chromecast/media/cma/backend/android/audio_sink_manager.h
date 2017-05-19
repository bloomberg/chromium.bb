// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_AUDIO_SINK_MANAGER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_AUDIO_SINK_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "chromecast/media/cma/backend/android/audio_sink_android.h"

namespace chromecast {
namespace media {

// Implementation of a manager for a group of audio sinks in order to control
// and configure them (currently for volume control).
class AudioSinkManager {
 public:
  static AudioSinkManager* Get();

  // Adds the given sink instance to the vector.
  void Add(AudioSinkAndroid* sink);

  // Removes the given sink instance from the vector.
  void Remove(AudioSinkAndroid* sink);

  // Sets the volume multiplier for the given content |type|.
  void SetVolume(AudioContentType type, float level);

  // Sets the mute state for the given content |type|.
  void SetMuted(AudioContentType type, bool muted);

  // Sets the volume multiplier limit for the given content |type|.
  void SetOutputLimit(AudioContentType type, float limit);

 protected:
  AudioSinkManager();
  virtual ~AudioSinkManager();

 private:
  // Contains volume control information for an audio content type.
  struct VolumeInfo {
    float GetEffectiveVolume();

    float volume = 0.0f;
    float limit = 1.0f;
    bool muted = false;
  };

  base::Lock lock_;

  std::map<AudioContentType, VolumeInfo> volume_info_;

  std::vector<AudioSinkAndroid*> sinks_;

  DISALLOW_COPY_AND_ASSIGN(AudioSinkManager);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_AUDIO_SINK_MANAGER_H_
