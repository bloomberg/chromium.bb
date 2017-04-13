// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/post_processing_pipeline.h"

#include <string>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/scoped_native_library.h"
#include "base/values.h"
#include "chromecast/base/serializers.h"
#include "chromecast/public/media/audio_post_processor_shlib.h"

namespace chromecast {
namespace media {

namespace {

const int kNoSampleRate = -1;
const char kSoCreateFunction[] = "AudioPostProcessorShlib_Create";

}  // namespace

using CreatePostProcessor = AudioPostProcessor* (*)(const std::string&, int);

PostProcessingPipeline::PostProcessingPipeline(
    const base::ListValue* filter_description_list,
    int channels)
    : sample_rate_(kNoSampleRate) {
  if (!filter_description_list) {
    return;  // Warning logged.
  }
  for (size_t i = 0; i < filter_description_list->GetSize(); ++i) {
    const base::DictionaryValue* processor_description_dict;
    CHECK(
        filter_description_list->GetDictionary(i, &processor_description_dict));
    std::string library_path;
    CHECK(processor_description_dict->GetString("processor", &library_path));
    if (library_path == "null" || library_path == "none") {
      continue;
    }
    const base::Value* processor_config_val;
    CHECK(processor_description_dict->Get("config", &processor_config_val));
    CHECK(processor_config_val->is_dict() || processor_config_val->is_string());
    auto processor_config_string = SerializeToJson(*processor_config_val);

    LOG(INFO) << "Creating an instance of " << library_path << "("
              << *processor_config_string << ")";
    libraries_.push_back(base::MakeUnique<base::ScopedNativeLibrary>(
        base::FilePath(library_path)));
    CHECK(libraries_.back()->is_valid())
        << "Could not open post processing library " << library_path;
    CreatePostProcessor create = reinterpret_cast<CreatePostProcessor>(
        libraries_.back()->GetFunctionPointer(kSoCreateFunction));

    CHECK(create) << "Could not find " << kSoCreateFunction << "() in "
                  << library_path;
    processors_.push_back(
        base::WrapUnique(create(*processor_config_string, channels)));
  }
}

PostProcessingPipeline::~PostProcessingPipeline() = default;

int PostProcessingPipeline::ProcessFrames(const std::vector<float*>& data,
                                          int num_frames,
                                          float current_volume,
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

  total_delay_frames_ = 0;
  for (auto& processor : processors_) {
    total_delay_frames_ +=
        processor->ProcessFrames(data, num_frames, current_volume);
  }
  return total_delay_frames_;
}

bool PostProcessingPipeline::SetSampleRate(int sample_rate) {
  sample_rate_ = sample_rate;
  bool result = true;
  for (auto& processor : processors_) {
    result &= processor->SetSampleRate(sample_rate_);
  }
  ringing_time_in_frames_ = GetRingingTimeInFrames();
  silence_frames_processed_ = 0;
  return result;
}

bool PostProcessingPipeline::IsRinging() {
  return silence_frames_processed_ < ringing_time_in_frames_;
}

int PostProcessingPipeline::GetRingingTimeInFrames() {
  int memory_frames = 0;
  for (auto& processor : processors_) {
    memory_frames += processor->GetRingingTimeInFrames();
  }
  return memory_frames;
}

}  // namespace media
}  // namespace chromecast
