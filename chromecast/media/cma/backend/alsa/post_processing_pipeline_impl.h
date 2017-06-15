// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ALSA_POST_PROCESSING_PIPELINE_IMPL_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ALSA_POST_PROCESSING_PIPELINE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chromecast/media/cma/backend/alsa/post_processing_pipeline.h"

namespace base {
class ListValue;
class ScopedNativeLibrary;
}  // namespace base

namespace chromecast {
namespace media {

class AudioPostProcessor;

// Creates and contains multiple AudioPostProcessors, as specified in ctor.
// Provides convenience methods to access and use the AudioPostProcessors.
class PostProcessingPipelineImpl : public PostProcessingPipeline {
 public:
  PostProcessingPipelineImpl(const std::string& name,
                             const base::ListValue* filter_description_list,
                             int channels);
  ~PostProcessingPipelineImpl() override;

  int ProcessFrames(float* data,
                    int num_frames,
                    float current_volume,
                    bool is_silence) override;

  bool SetSampleRate(int sample_rate) override;
  bool IsRinging() override;

  // Send string |config| to post processor |name|.
  void SetPostProcessorConfig(const std::string& name,
                              const std::string& config) override;

 private:
  int GetRingingTimeInFrames();
  void UpdateCastVolume(float multiplier);

  std::string name_;
  int sample_rate_;
  int ringing_time_in_frames_ = 0;
  int silence_frames_processed_ = 0;
  int total_delay_frames_ = 0;
  float current_multiplier_;
  float cast_volume_;

  // Contains all libraries in use;
  // Functions in shared objects cannot be used once library is closed.
  std::vector<std::unique_ptr<base::ScopedNativeLibrary>> libraries_;

  // Must be after libraries_
  // Note: typedef is used to silence chromium-style mandatory constructor in
  // structs.
  typedef struct {
    std::unique_ptr<AudioPostProcessor> ptr;
    std::string name;
  } PostProcessorInfo;

  std::vector<PostProcessorInfo> processors_;

  DISALLOW_COPY_AND_ASSIGN(PostProcessingPipelineImpl);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ALSA_POST_PROCESSING_PIPELINE_IMPL_H_
