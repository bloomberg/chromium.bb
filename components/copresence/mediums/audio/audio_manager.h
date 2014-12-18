// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_MEDIUMS_AUDIO_AUDIO_MANAGER_H_
#define COMPONENTS_COPRESENCE_MEDIUMS_AUDIO_AUDIO_MANAGER_H_

#include <string>

#include "base/callback.h"
#include "components/copresence/public/copresence_constants.h"

namespace copresence {

class WhispernetClient;

class AudioManager {
 public:
  virtual ~AudioManager() {}

  // Initializes the object. Do not use this object before calling this method.
  virtual void Initialize(WhispernetClient* whispernet_client,
                          const TokensCallback& tokens_cb) = 0;

  virtual void StartPlaying(AudioType type) = 0;
  virtual void StopPlaying(AudioType type) = 0;

  virtual void StartRecording(AudioType type) = 0;
  virtual void StopRecording(AudioType type) = 0;

  virtual void SetToken(AudioType type,
                        const std::string& url_unsafe_token) = 0;

  virtual const std::string GetToken(AudioType type) = 0;

  virtual bool IsPlayingTokenHeard(AudioType type) = 0;
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_MEDIUMS_AUDIO_AUDIO_MANAGER_H_
