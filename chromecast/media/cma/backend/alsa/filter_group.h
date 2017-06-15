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
#include "base/memory/aligned_memory.h"
#include "base/values.h"
#include "chromecast/media/cma/backend/alsa/stream_mixer_alsa.h"
#include "chromecast/public/volume_control.h"

namespace media {
class AudioBus;
}  // namespace media

namespace chromecast {
namespace media {

class PostProcessingPipeline;

// FilterGroup mixes StreamMixerAlsa::InputQueues and/or FilterGroups,
// mixes their outputs, and applies DSP to them.

// FilterGroups are added at construction. These cannot be removed.

// InputQueues are added with AddActiveInput(), then cleared when
// MixAndFilter() is called (they must be added each time data is queried).
class FilterGroup {
 public:
  // |num_channels| indicates number of input audio channels.
  // |mix_to_mono| enables mono mixing in the pipeline. The number of audio
  //    output channels will be 1 if it is set to true, otherwise it remains
  //    same as |num_channels|.
  // |name| is used for debug printing
  // |filter_list| is a list of {"processor": LIBRARY_NAME, "configs": CONFIG}
  //    that is used to create PostProcessingPipeline.
  // |device_ids| is a set of strings that is used as a filter to determine
  //   if an InputQueue belongs to this group (InputQueue->name() must exactly
  //   match an entry in |device_ids| to be processed by this group).
  // |mixed_inputs| are FilterGroups that will be mixed into this FilterGroup.
  //   ex: the final mix ("mix") FilterGroup mixes all other filter groups.
  // FilterGroups currently use either InputQueues OR FilterGroups as inputs,
  //   but there is no technical limitation preventing mixing input classes.
  FilterGroup(int num_channels,
              bool mix_to_mono,
              const std::string& name,
              const base::ListValue* filter_list,
              const std::unordered_set<std::string>& device_ids,
              const std::vector<FilterGroup*>& mixed_inputs);

  ~FilterGroup();

  // Sets the sample rate of the post-processors.
  void Initialize(int output_samples_per_second);

  // Returns |true| if this FilterGroup is appropriate to process |input|.
  bool CanProcessInput(StreamMixerAlsa::InputQueue* input);

  // Adds |input| to |active_inputs_|.
  void AddActiveInput(StreamMixerAlsa::InputQueue* input);

  // Mixes all active inputs and passes them through the audio filter.
  // Returns the largest volume of all streams with data.
  //         return value will be zero IFF there is no data and
  //         the PostProcessingPipeline is not ringing.
  float MixAndFilter(int chunk_size);

  // Gets the current delay of this filter group's AudioPostProcessors.
  // (Not recursive).
  int64_t GetRenderingDelayMicroseconds();

  // Clear all |active_inputs_|. This should be called before AddActiveInputs
  // on each mixing iteration.
  void ClearActiveInputs();

  // Retrieves a pointer to the output buffer.
  float* interleaved() { return interleaved_.get(); }

  // Get the last used volume.
  float last_volume() const { return last_volume_; }

  std::string name() const { return name_; }

  // Returns number of audio output channels from the filter group.
  int GetOutputChannelCount() const;

  // Sends configuration string |config| to all post processors with the given
  // |name|.
  void SetPostProcessorConfig(const std::string& name,
                              const std::string& config);

 private:
  void ResizeBuffersIfNecessary(int chunk_size);

  const int num_channels_;
  bool mix_to_mono_;
  const std::string name_;
  const std::unordered_set<std::string> device_ids_;
  std::vector<FilterGroup*> mixed_inputs_;
  std::vector<StreamMixerAlsa::InputQueue*> active_inputs_;

  int output_samples_per_second_;
  int frames_zeroed_ = 0;
  float last_volume_ = 0.0f;
  int64_t delay_frames_ = 0;

  // Buffers that hold audio data while it is mixed.
  // These are kept as members of this class to minimize copies and
  // allocations.
  std::unique_ptr<::media::AudioBus> temp_;
  std::unique_ptr<::media::AudioBus> mixed_;

  // Interleaved data must be aligned to 16 bytes.
  std::unique_ptr<float, base::AlignedFreeDeleter> interleaved_;

  std::unique_ptr<PostProcessingPipeline> post_processing_pipeline_;

  DISALLOW_COPY_AND_ASSIGN(FilterGroup);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ALSA_FILTER_GROUP_H_
