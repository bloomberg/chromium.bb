// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/filter_group.h"
#include "media/base/audio_bus.h"

namespace chromecast {
namespace media {

namespace {

// How many seconds of silence should be passed to the filters to flush them.
const float kSilenceSecondsToFilter = 1.0f;
const int kNumOutputChannels = 2;

}  // namespace

FilterGroup::FilterGroup(const std::unordered_set<std::string>& input_types,
                         AudioFilterFactory::FilterType filter_type)
    : input_types_(input_types),
      output_samples_per_second_(0),
      sample_format_(::media::SampleFormat::kUnknownSampleFormat),
      audio_filter_(AudioFilterFactory::MakeAudioFilter(filter_type)),
      silence_frames_filtered_(0) {}

FilterGroup::~FilterGroup() = default;

void FilterGroup::Initialize(int output_samples_per_second,
                             ::media::SampleFormat format) {
  output_samples_per_second_ = output_samples_per_second;
  sample_format_ = format;
  if (audio_filter_) {
    audio_filter_->SetSampleRateAndFormat(output_samples_per_second_,
                                          sample_format_);
  }
  silence_frames_filtered_ = 0;
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
  DCHECK_NE(sample_format_, ::media::SampleFormat::kUnknownSampleFormat);
  if (active_inputs_.empty()) {
    int silence_frames_to_filter =
        output_samples_per_second_ * kSilenceSecondsToFilter;
    if (audio_filter_ && silence_frames_filtered_ < silence_frames_to_filter) {
      silence_frames_filtered_ += chunk_size;
    } else {
      return false;  // Output will be silence, no need to mix.
    }
  } else {
    silence_frames_filtered_ = 0;
  }

  ResizeBuffersIfNecessary(chunk_size);

  mixed_->ZeroFramesPartial(0, chunk_size);
  for (StreamMixerAlsa::InputQueue* input : active_inputs_) {
    input->GetResampledData(temp_.get(), chunk_size);
    for (int c = 0; c < kNumOutputChannels; ++c) {
      input->VolumeScaleAccumulate(c, temp_->channel(c), chunk_size,
                                   mixed_->channel(c));
    }
  }

  mixed_->ToInterleaved(chunk_size, BytesPerOutputFormatSample(),
                        interleaved_.data());
  if (audio_filter_) {
    audio_filter_->ProcessInterleaved(interleaved_.data(), chunk_size);
  }

  return true;
}

void FilterGroup::ClearInterleaved(int chunk_size) {
  ResizeBuffersIfNecessary(chunk_size);
  memset(interleaved_.data(), 0, static_cast<size_t>(chunk_size) *
                                     kNumOutputChannels *
                                     BytesPerOutputFormatSample());
}

void FilterGroup::ResizeBuffersIfNecessary(int chunk_size) {
  if (!mixed_ || mixed_->frames() < chunk_size) {
    mixed_ = ::media::AudioBus::Create(kNumOutputChannels, chunk_size);
  }
  if (!temp_ || temp_->frames() < chunk_size) {
    temp_ = ::media::AudioBus::Create(kNumOutputChannels, chunk_size);
  }

  size_t interleaved_size = static_cast<size_t>(chunk_size) *
                            kNumOutputChannels * BytesPerOutputFormatSample();

  if (interleaved_.size() < interleaved_size) {
    interleaved_.resize(interleaved_size);
  }
}

int FilterGroup::BytesPerOutputFormatSample() {
  return ::media::SampleFormatToBytesPerChannel(sample_format_);
}

void FilterGroup::ClearActiveInputs() {
  active_inputs_.clear();
}

}  // namespace media
}  // namespace chromecast
