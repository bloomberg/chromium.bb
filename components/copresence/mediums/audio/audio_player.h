// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_MEDIUMS_AUDIO_AUDIO_PLAYER_H_
#define COMPONENTS_COPRESENCE_MEDIUMS_AUDIO_AUDIO_PLAYER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace media {
class AudioBusRefCounted;
}

namespace copresence {

// The AudioPlayerImpl class will play a set of samples till it is told to stop.
class AudioPlayer {
 public:
  // Initializes the object. Do not use this object before calling this method.
  virtual void Initialize() = 0;

  // Play the given samples. These samples will keep on being played in a loop
  // till we explicitly tell the player to stop playing. If we are already
  // playing, this call will be ignored.
  virtual void Play(
      const scoped_refptr<media::AudioBusRefCounted>& samples) = 0;

  // Stop playing. If we're already stopped, this call will be ignored.
  virtual void Stop() = 0;

  // Cleans up and deletes this object. Do not use object after this call.
  virtual void Finalize() = 0;

 protected:
  virtual ~AudioPlayer() {}
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_MEDIUMS_AUDIO_AUDIO_PLAYER_H_
