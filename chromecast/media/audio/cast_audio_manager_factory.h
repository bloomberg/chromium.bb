// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_FACTORY_H_
#define CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_FACTORY_H_

#include "base/macros.h"
#include "media/audio/audio_manager_factory.h"

namespace chromecast {
namespace media {

class MediaPipelineBackendManager;

class CastAudioManagerFactory : public ::media::AudioManagerFactory {
 public:
  explicit CastAudioManagerFactory(
      MediaPipelineBackendManager* backend_manager);
  ~CastAudioManagerFactory() override;

  // ::media::AudioManagerFactory overrides.
  scoped_ptr<::media::AudioManager, ::media::AudioManagerDeleter>
  CreateInstance(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                 scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner,
                 ::media::AudioLogFactory* audio_log_factory) override;

 private:
  MediaPipelineBackendManager* const backend_manager_;

  DISALLOW_COPY_AND_ASSIGN(CastAudioManagerFactory);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_FACTORY_H_
