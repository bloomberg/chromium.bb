// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/filter_group.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromecast/media/cma/backend/post_processing_pipeline.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"
#include "media/base/vector_math.h"

namespace chromecast {
namespace media {

FilterGroup::FilterGroup(int num_channels,
                         bool mix_to_mono,
                         const std::string& name,
                         std::unique_ptr<PostProcessingPipeline> pipeline,
                         const std::unordered_set<std::string>& device_ids,
                         const std::vector<FilterGroup*>& mixed_inputs)
    : num_channels_(num_channels),
      mix_to_mono_(mix_to_mono),
      playout_channel_(kChannelAll),
      name_(name),
      device_ids_(device_ids),
      mixed_inputs_(mixed_inputs),
      output_samples_per_second_(0),
      post_processing_pipeline_(std::move(pipeline)) {
  for (auto* const m : mixed_inputs)
    DCHECK_EQ(m->GetOutputChannelCount(), num_channels);
  // Don't need mono mixer if input is single channel.
  if (num_channels == 1)
    mix_to_mono_ = false;
}

FilterGroup::~FilterGroup() = default;

void FilterGroup::Initialize(int output_samples_per_second) {
  output_samples_per_second_ = output_samples_per_second;
  post_processing_pipeline_->SetSampleRate(output_samples_per_second);
}

bool FilterGroup::CanProcessInput(StreamMixer::InputQueue* input) {
  return !(device_ids_.find(input->device_id()) == device_ids_.end());
}

void FilterGroup::AddActiveInput(StreamMixer::InputQueue* input) {
  active_inputs_.push_back(input);
}

float FilterGroup::MixAndFilter(int chunk_size) {
  DCHECK_NE(output_samples_per_second_, 0);

  ResizeBuffersIfNecessary(chunk_size);

  float volume = 0.0f;

  // Recursively mix inputs.
  for (auto* filter_group : mixed_inputs_) {
    volume = std::max(volume, filter_group->MixAndFilter(chunk_size));
  }

  // |volume| can only be 0 if no |mixed_inputs_| have data.
  // This is true because FilterGroup can only return 0 if:
  // a) It has no data and its PostProcessorPipeline is not ringing.
  //    (early return, below) or
  // b) The output volume is 0 and has NEVER been non-zero,
  //    since FilterGroup will use last_volume_ if volume is 0.
  //    In this case, there was never any data in the pipeline.
  if (active_inputs_.empty() && volume == 0.0f &&
      !post_processing_pipeline_->IsRinging()) {
    if (frames_zeroed_ < chunk_size) {
      // Ensure interleaved_ is zeros.
      std::fill_n(interleaved(), chunk_size * num_channels_, 0);
      frames_zeroed_ = chunk_size;
    }
    return 0.0f;  // Output will be silence, no need to mix.
  }

  frames_zeroed_ = 0;

  // Mix InputQueues
  mixed_->ZeroFramesPartial(0, chunk_size);
  for (StreamMixer::InputQueue* input : active_inputs_) {
    input->GetResampledData(temp_.get(), chunk_size);
    for (int c = 0; c < num_channels_; ++c) {
      input->VolumeScaleAccumulate(c != 0, temp_->channel(c), chunk_size,
                                   mixed_->channel(c));
    }
    volume = std::max(volume, input->EffectiveVolume());
  }

  mixed_->ToInterleaved<::media::FloatSampleTypeTraits<float>>(chunk_size,
                                                               interleaved());

  // Mix FilterGroups
  for (FilterGroup* group : mixed_inputs_) {
    if (group->last_volume() > 0.0f) {
      ::media::vector_math::FMAC(group->interleaved(), 1.0f,
                                 chunk_size * num_channels_, interleaved());
    }
  }

  bool is_silence = (volume == 0.0f);

  // Allow paused streams to "ring out" at the last valid volume.
  // If the stream volume is actually 0, this doesn't matter, since the
  // data is 0's anyway.
  if (!is_silence) {
    last_volume_ = volume;
  }

  delay_frames_ = post_processing_pipeline_->ProcessFrames(
      interleaved(), chunk_size, last_volume_, is_silence);

  // Copy the active channel to all channels.
  if (playout_channel_ != kChannelAll) {
    DCHECK_GE(playout_channel_, 0);
    DCHECK_LT(playout_channel_, num_channels_);

    for (int frame = 0; frame < chunk_size; ++frame) {
      float s = interleaved()[frame * num_channels_ + playout_channel_];
      for (int c = 0; c < num_channels_; ++c)
        interleaved()[frame * num_channels_ + c] = s;
    }
  }

  // Mono mixing after all processing if needed.
  if (mix_to_mono_) {
    for (int frame = 0; frame < chunk_size; ++frame) {
      float sum = 0;
      for (int c = 0; c < num_channels_; ++c)
        sum += interleaved()[frame * num_channels_ + c];
      for (int c = 0; c < num_channels_; ++c)
        interleaved()[frame * num_channels_ + c] = sum / num_channels_;
    }
  }

  return last_volume_;
}

int64_t FilterGroup::GetRenderingDelayMicroseconds() {
  return delay_frames_ * base::Time::kMicrosecondsPerSecond /
         output_samples_per_second_;
}

void FilterGroup::ClearActiveInputs() {
  active_inputs_.clear();
}

int FilterGroup::GetOutputChannelCount() const {
  return num_channels_;
}

void FilterGroup::ResizeBuffersIfNecessary(int chunk_size) {
  if (!mixed_ || mixed_->frames() < chunk_size) {
    mixed_ = ::media::AudioBus::Create(num_channels_, chunk_size);
    temp_ = ::media::AudioBus::Create(num_channels_, chunk_size);
    interleaved_.reset(static_cast<float*>(
        base::AlignedAlloc(chunk_size * num_channels_ * sizeof(float),
                           ::media::AudioBus::kChannelAlignment)));
  }
}

void FilterGroup::SetPostProcessorConfig(const std::string& name,
                                         const std::string& config) {
  post_processing_pipeline_->SetPostProcessorConfig(name, config);
}

void FilterGroup::SetMixToMono(bool mix_to_mono) {
  mix_to_mono_ = (num_channels_ != 1 && mix_to_mono);
}

void FilterGroup::UpdatePlayoutChannel(int playout_channel) {
  LOG(INFO) << __FUNCTION__ << " channel=" << playout_channel;
  if (playout_channel >= num_channels_) {
    LOG(ERROR) << "only " << num_channels_ << " present, wanted channel #"
               << playout_channel;
    return;
  }
  playout_channel_ = playout_channel;
}

}  // namespace media
}  // namespace chromecast
