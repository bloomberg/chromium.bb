// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/filter_group.h"

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chromecast/media/cma/backend/alsa/post_processing_pipeline.h"
#include "media/base/audio_bus.h"

namespace chromecast {
namespace media {
FilterGroup::FilterGroup(const std::unordered_set<std::string>& input_types,
                         AudioContentType content_type,
                         int num_channels,
                         const base::ListValue* filter_list)
    : input_types_(input_types),
      content_type_(content_type),
      num_channels_(num_channels),
      output_samples_per_second_(0),
      channels_(num_channels_),
      post_processing_pipeline_(
          base::MakeUnique<PostProcessingPipeline>(filter_list,
                                                   num_channels_)) {}

FilterGroup::~FilterGroup() = default;

void FilterGroup::Initialize(int output_samples_per_second) {
  output_samples_per_second_ = output_samples_per_second;
  post_processing_pipeline_->SetSampleRate(output_samples_per_second);
}

bool FilterGroup::CanProcessInput(StreamMixerAlsa::InputQueue* input) {
  return !(input_types_.find(input->device_id()) == input_types_.end());
}

void FilterGroup::AddActiveInput(StreamMixerAlsa::InputQueue* input) {
  active_inputs_.push_back(input);
}

std::vector<uint8_t>* FilterGroup::GetInterleaved() {
  return &interleaved_;
}

bool FilterGroup::MixAndFilter(int chunk_size) {
  DCHECK_NE(output_samples_per_second_, 0);
  if (active_inputs_.empty() && !post_processing_pipeline_->IsRinging()) {
    return false;  // Output will be silence, no need to mix.
  }

  ResizeBuffersIfNecessary(chunk_size);

  mixed_->ZeroFramesPartial(0, chunk_size);
  for (StreamMixerAlsa::InputQueue* input : active_inputs_) {
    input->GetResampledData(temp_.get(), chunk_size);
    for (int c = 0; c < num_channels_; ++c) {
      DCHECK(channels_[c]);
      input->VolumeScaleAccumulate(c != 0, temp_->channel(c), chunk_size,
                                   channels_[c]);
    }
  }

  post_processing_pipeline_->ProcessFrames(channels_, chunk_size, volume_,
                                           active_inputs_.empty());
  mixed_->ToInterleaved(chunk_size, BytesPerOutputFormatSample(),
                        interleaved_.data());
  return true;
}

void FilterGroup::ClearInterleaved(int chunk_size) {
  ResizeBuffersIfNecessary(chunk_size);
  memset(interleaved_.data(), 0,
         static_cast<size_t>(chunk_size) * num_channels_ *
             BytesPerOutputFormatSample());
}

void FilterGroup::ResizeBuffersIfNecessary(int chunk_size) {
  if (!mixed_ || mixed_->frames() < chunk_size) {
    mixed_ = ::media::AudioBus::Create(num_channels_, chunk_size);
    for (int c = 0; c < num_channels_; ++c) {
      channels_[c] = mixed_->channel(c);
    }
  }
  if (!temp_ || temp_->frames() < chunk_size) {
    temp_ = ::media::AudioBus::Create(num_channels_, chunk_size);
  }

  size_t interleaved_size = static_cast<size_t>(chunk_size) * num_channels_ *
                            BytesPerOutputFormatSample();

  if (interleaved_.size() < interleaved_size) {
    interleaved_.resize(interleaved_size);
  }
}

int FilterGroup::BytesPerOutputFormatSample() {
  return sizeof(int32_t);
}

void FilterGroup::ClearActiveInputs() {
  active_inputs_.clear();
}

void FilterGroup::DisablePostProcessingForTest() {
  post_processing_pipeline_ =
      base::MakeUnique<PostProcessingPipeline>(nullptr, num_channels_);
}

}  // namespace media
}  // namespace chromecast
