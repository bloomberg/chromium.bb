// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_PIPELINE_AUDIO_PIPELINE_H_
#define CHROMECAST_MEDIA_CMA_PIPELINE_AUDIO_PIPELINE_H_

#include "base/macros.h"
#include "chromecast/media/cma/pipeline/av_pipeline_client.h"

namespace chromecast {
namespace media {

class AudioPipeline {
 public:
  AudioPipeline();
  virtual ~AudioPipeline();

  // Set the audio client.
  virtual void SetClient(const AvPipelineClient& client) = 0;

  // Set the stream volume.
  // - A value of 1.0 is the neutral value.
  // - |volume|=0.0 mutes the stream.
  virtual void SetVolume(float volume) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioPipeline);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_PIPELINE_AUDIO_PIPELINE_H_
