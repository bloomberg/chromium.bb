// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/post_processing_pipeline_impl.h"

#include <cmath>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/scoped_native_library.h"
#include "base/values.h"
#include "chromecast/base/serializers.h"
#include "chromecast/public/media/audio_post_processor_shlib.h"
#include "chromecast/public/volume_control.h"

namespace chromecast {
namespace media {

namespace {

const int kNoSampleRate = -1;
const char kProcessorKey[] = "processor";
const char kNameKey[] = "name";
}  // namespace

std::unique_ptr<PostProcessingPipeline> PostProcessingPipeline::Create(
    const std::string& name,
    const base::ListValue* filter_description_list,
    int num_channels) {
  return base::MakeUnique<PostProcessingPipelineImpl>(
      name, filter_description_list, num_channels);
}

PostProcessingPipelineImpl::PostProcessingPipelineImpl(
    const std::string& name,
    const base::ListValue* filter_description_list,
    int channels)
    : name_(name), sample_rate_(kNoSampleRate) {
  if (!filter_description_list) {
    return;  // Warning logged.
  }
  for (size_t i = 0; i < filter_description_list->GetSize(); ++i) {
    const base::DictionaryValue* processor_description_dict;
    CHECK(
        filter_description_list->GetDictionary(i, &processor_description_dict));

    std::string processor_name;
    processor_description_dict->GetString(kNameKey, &processor_name);
    std::vector<PostProcessorInfo>::iterator it =
        find_if(processors_.begin(), processors_.end(),
                [&processor_name](PostProcessorInfo& p) {
                  return p.name == processor_name;
                });
    LOG_IF(DFATAL, it != processors_.end())
        << "Duplicate postprocessor name " << processor_name;
    std::string library_path;
    CHECK(processor_description_dict->GetString(kProcessorKey, &library_path));
    if (library_path == "null" || library_path == "none") {
      continue;
    }

    const base::Value* processor_config_val;
    CHECK(processor_description_dict->Get("config", &processor_config_val));
    CHECK(processor_config_val->is_dict() || processor_config_val->is_string());
    std::unique_ptr<std::string> processor_config_string =
        SerializeToJson(*processor_config_val);

    LOG(INFO) << "Creating an instance of " << library_path << "("
              << *processor_config_string << ")";

    processors_.emplace_back(
        PostProcessorInfo{factory_.CreatePostProcessor(
                              library_path, *processor_config_string, channels),
                          processor_name});
  }
}

PostProcessingPipelineImpl::~PostProcessingPipelineImpl() = default;

int PostProcessingPipelineImpl::ProcessFrames(float* data,
                                              int num_frames,
                                              float current_multiplier,
                                              bool is_silence) {
  DCHECK_NE(sample_rate_, kNoSampleRate);
  if (is_silence) {
    if (!IsRinging()) {
      return total_delay_frames_;  // Output will be silence.
    }
    silence_frames_processed_ += num_frames;
  } else {
    silence_frames_processed_ = 0;
  }

  UpdateCastVolume(current_multiplier);

  total_delay_frames_ = 0;
  for (auto& processor : processors_) {
    total_delay_frames_ += processor.ptr->ProcessFrames(
        data, num_frames, cast_volume_, current_dbfs_);
  }
  return total_delay_frames_;
}

bool PostProcessingPipelineImpl::SetSampleRate(int sample_rate) {
  sample_rate_ = sample_rate;
  bool result = true;
  for (auto& processor : processors_) {
    result &= processor.ptr->SetSampleRate(sample_rate_);
  }
  ringing_time_in_frames_ = GetRingingTimeInFrames();
  silence_frames_processed_ = 0;
  return result;
}

bool PostProcessingPipelineImpl::IsRinging() {
  return silence_frames_processed_ < ringing_time_in_frames_;
}

int PostProcessingPipelineImpl::GetRingingTimeInFrames() {
  int memory_frames = 0;
  for (auto& processor : processors_) {
    memory_frames += processor.ptr->GetRingingTimeInFrames();
  }
  return memory_frames;
}

void PostProcessingPipelineImpl::UpdateCastVolume(float multiplier) {
  DCHECK_GE(multiplier, 0.0);

  if (multiplier == current_multiplier_) {
    return;
  }
  current_multiplier_ = multiplier;
  current_dbfs_ = std::log10(multiplier) * 20;
  DCHECK(chromecast::media::VolumeControl::DbFSToVolume);
  cast_volume_ = chromecast::media::VolumeControl::DbFSToVolume(current_dbfs_);
}

// Send string |config| to postprocessor |name|.
void PostProcessingPipelineImpl::SetPostProcessorConfig(
    const std::string& name,
    const std::string& config) {
  DCHECK(!name.empty());
  std::vector<PostProcessorInfo>::iterator it =
      find_if(processors_.begin(), processors_.end(),
              [&name](PostProcessorInfo& p) { return p.name == name; });
  if (it != processors_.end()) {
    it->ptr->UpdateParameters(config);
    LOG(INFO) << "Config string:\n"
              << config << "\nwas delivered to postprocessor " << name;
  }
}

}  // namespace media
}  // namespace chromecast
