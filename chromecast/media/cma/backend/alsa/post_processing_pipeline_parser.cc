// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/post_processing_pipeline_parser.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromecast/base/serializers.h"
#include "chromecast/media/base/audio_device_ids.h"
#include "media/audio/audio_device_description.h"

namespace chromecast {
namespace media {

namespace {

const base::FilePath::CharType kPostProcessingPipelineFilePath[] =
    FILE_PATH_LITERAL("/etc/cast_audio.json");

const char kMediaPipelineKey[] = "media";
const char kTtsPipelineKey[] = "tts";

// TODO(bshaya): Use AudioContentType instead.
std::string GetPipelineKey(const std::string& device_id) {
  if (device_id == kTtsAudioDeviceId) {
    return kTtsPipelineKey;
  }
  if (device_id == ::media::AudioDeviceDescription::kDefaultDeviceId) {
    return kMediaPipelineKey;
  }
  NOTREACHED() << "Invalid device_id: " << device_id;
  return "";
}

}  // namespace

PostProcessingPipelineParser::PostProcessingPipelineParser() = default;
PostProcessingPipelineParser::~PostProcessingPipelineParser() = default;

void PostProcessingPipelineParser::Initialize() {
  if (!base::PathExists(base::FilePath(kPostProcessingPipelineFilePath))) {
    LOG(WARNING) << "No post-processing config found in "
                 << kPostProcessingPipelineFilePath << ".";
    return;
  }

  config_dict_ = base::DictionaryValue::From(
      DeserializeJsonFromFile(base::FilePath(kPostProcessingPipelineFilePath)));
  CHECK(config_dict_->GetDictionary("post_processing", &pipeline_dict_))
      << "No \"post_processing\" object found in "
      << kPostProcessingPipelineFilePath;
}

base::ListValue* PostProcessingPipelineParser::GetPipelineByDeviceId(
    const std::string& device_id) {
  if (!pipeline_dict_) {
    return nullptr;
  }

  base::ListValue* out_list;
  std::string key = GetPipelineKey(device_id);
  if (!pipeline_dict_->GetList(key, &out_list)) {
    LOG(WARNING) << "No post-processor description found for \"" << key
                 << "\" in " << kPostProcessingPipelineFilePath
                 << ". Using passthrough.";
    return nullptr;
  }
  return out_list;
}

}  // namepsace media
}  // namespace chromecast
