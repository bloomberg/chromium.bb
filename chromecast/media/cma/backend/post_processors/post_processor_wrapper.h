// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_POST_PROCESSORS_POST_PROCESSOR_WRAPPER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_POST_PROCESSORS_POST_PROCESSOR_WRAPPER_H_

// Provides a wrapper for AudioPostProcessor to allow using it as an
// AudioPostProcessor2. This works by simply fowarding the input buffer as the
// output buffer, since the original AudioPostProcessor API has equal numbers of
// input and output channels.

#include <memory>
#include <string>

#include "base/macros.h"
#include "chromecast/public/media/audio_post_processor2_shlib.h"

namespace chromecast {
namespace media {

class AudioPostProcessor;

class AudioPostProcessorWrapper : public AudioPostProcessor2 {
 public:
  // AudioPostProcessorWrapper owns |pp|.
  AudioPostProcessorWrapper(std::unique_ptr<AudioPostProcessor> pp,
                            int channels);
  ~AudioPostProcessorWrapper() override;

 private:
  // AudioPostProcessor2 implementation:
  bool SetSampleRate(int sample_rate) override;
  int ProcessFrames(float* data,
                    int frames,
                    float system_volume,
                    float volume_dbfs) override;
  float* GetOutputBuffer() override;
  int GetRingingTimeInFrames() override;
  void UpdateParameters(const std::string& message) override;
  void SetContentType(AudioContentType content_type) override;
  void SetPlayoutChannel(int channel) override;
  int NumOutputChannels() override;

  std::unique_ptr<AudioPostProcessor> pp_;
  int channels_;
  float* output_buffer_;

  DISALLOW_COPY_AND_ASSIGN(AudioPostProcessorWrapper);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_POST_PROCESSORS_POST_PROCESSOR_WRAPPER_H_
