// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ALSA_POST_PROCESSING_PIPELINE_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ALSA_POST_PROCESSING_PIPELINE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"

namespace base {
class ListValue;
class ScopedNativeLibrary;
}  // namespace base

namespace chromecast {
namespace media {

class AudioPostProcessor;

// Creates and contains multiple AudioPostProcessors, as specified in ctor.
// Provides convenience methods to access and use the AudioPostProcessors.
class PostProcessingPipeline {
 public:
  PostProcessingPipeline(const base::ListValue* filter_description_list,
                         int channels);
  ~PostProcessingPipeline();

  int ProcessFrames(const std::vector<float*>& data,
                    int num_frames,
                    float current_volume,
                    bool is_silence);
  bool SetSampleRate(int sample_rate);
  bool IsRinging();

 private:
  int GetRingingTimeInFrames();

  int sample_rate_;
  int ringing_time_in_frames_ = 0;
  int silence_frames_processed_ = 0;
  int total_delay_frames_ = 0;

  // Contains all libraries in use;
  // Functions in shared objects cannot be used once library is closed.
  std::vector<std::unique_ptr<base::ScopedNativeLibrary>> libraries_;

  // Must be after libraries_
  std::vector<std::unique_ptr<AudioPostProcessor>> processors_;

  DISALLOW_COPY_AND_ASSIGN(PostProcessingPipeline);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ALSA_POST_PROCESSING_PIPELINE_H_
