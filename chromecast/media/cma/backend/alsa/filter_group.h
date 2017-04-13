// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ALSA_FILTER_GROUP_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ALSA_FILTER_GROUP_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/macros.h"
#include "base/values.h"
#include "chromecast/media/cma/backend/alsa/stream_mixer_alsa.h"
#include "chromecast/public/volume_control.h"

namespace media {
class AudioBus;
}  // namespace media

namespace chromecast {
namespace media {

class PostProcessingPipeline;

// FilterGroup contains state for an AudioFilter.
// It takes multiple StreamMixerAlsa::InputQueues,
// mixes them, and processes them.

// ActiveInputs are added with AddActiveInput(), then cleared when
// MixAndFilter() is called (they must be added each time data is queried).
class FilterGroup {
 public:
  // |input_types| is a set of strings that is used as a filter to determine
  // if an input belongs to this group (InputQueue->name() must exactly match an
  // entry in |input_types| to be processed by this group.
  // |filter_type| is passed to AudioFilterFactory to create an AudioFilter.
  FilterGroup(const std::unordered_set<std::string>& input_types,
              AudioContentType content_type,
              int channels,
              const base::ListValue* filter_list);
  ~FilterGroup();

  AudioContentType content_type() const { return content_type_; }

  void set_volume(float volume) { volume_ = volume; }

  // Sets the sample rate of the post-processors.
  void Initialize(int output_samples_per_second);

  // Returns |true| if this FilterGroup is appropriate to process |input|.
  bool CanProcessInput(StreamMixerAlsa::InputQueue* input);

  // Adds |input| to |active_inputs_|.
  void AddActiveInput(StreamMixerAlsa::InputQueue* input);

  // Retrieves a pointer to the output buffer |interleaved_|.
  std::vector<uint8_t>* GetInterleaved();

  // Mixes all active inputs and passes them through the audio filter.
  bool MixAndFilter(int chunk_size);

  // Overwrites |interleaved_| with 0's, ensuring at least
  // |chunk_size| bytes.
  void ClearInterleaved(int chunk_size);

  // Clear all |active_inputs_|. This should be called before AddActiveInputs
  // on each mixing iteration.
  void ClearActiveInputs();

  // Resets the PostProcessingPipeline, removing all AudioPostProcessors.
  void DisablePostProcessingForTest();

 private:
  void ResizeBuffersIfNecessary(int chunk_size);
  int BytesPerOutputFormatSample();

  const std::unordered_set<std::string> input_types_;
  const AudioContentType content_type_;
  const int num_channels_;
  std::vector<StreamMixerAlsa::InputQueue*> active_inputs_;

  int output_samples_per_second_;

  float volume_ = 0.0f;

  // Buffers that hold audio data while it is mixed.
  // These are kept as members of this class to minimize copies and
  // allocations.
  std::unique_ptr<::media::AudioBus> temp_;
  std::unique_ptr<::media::AudioBus> mixed_;
  std::vector<uint8_t> interleaved_;
  std::vector<float*> channels_;

  std::unique_ptr<PostProcessingPipeline> post_processing_pipeline_;

  DISALLOW_COPY_AND_ASSIGN(FilterGroup);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ALSA_FILTER_GROUP_H_
