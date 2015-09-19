// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_FACTORY_H_
#define CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_FACTORY_H_

#include "base/macros.h"
#include "media/audio/audio_manager_factory.h"

namespace chromecast {
namespace media {

class CastAudioManagerFactory : public ::media::AudioManagerFactory {
 public:
  CastAudioManagerFactory();
  ~CastAudioManagerFactory() override;

  // ::media::AudioManagerFactory overrides.
  ::media::AudioManager* CreateInstance(
      ::media::AudioLogFactory* audio_log_factory) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastAudioManagerFactory);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_FACTORY_H_
