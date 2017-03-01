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
#include "chromecast/media/cma/backend/alsa/audio_filter_factory.h"
#include "chromecast/media/cma/backend/alsa/stream_mixer_alsa.h"

namespace chromecast {
namespace media {
class AudioBus;

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
              AudioFilterFactory::FilterType filter_type);
  ~FilterGroup();

  // Sets the sample rate and format in the AudioFilter.
  void Initialize(int output_samples_per_second, ::media::SampleFormat format);

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

 private:
  void ResizeBuffersIfNecessary(int chunk_size);
  int BytesPerOutputFormatSample();

  const std::unordered_set<std::string> input_types_;
  std::vector<StreamMixerAlsa::InputQueue*> active_inputs_;

  int output_samples_per_second_;
  ::media::SampleFormat sample_format_;

  // Buffers that hold audio data while it is mixed.
  // These are kept as members of this class to minimize copies and
  // allocations.
  std::unique_ptr<::media::AudioBus> temp_;
  std::unique_ptr<::media::AudioBus> mixed_;
  std::vector<uint8_t> interleaved_;

  std::unique_ptr<AudioFilterInterface> audio_filter_;
  int silence_frames_filtered_;

  DISALLOW_COPY_AND_ASSIGN(FilterGroup);
};

}  // namespace media
}  // namespace chromecast
#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ALSA_FILTER_GROUP_H_
