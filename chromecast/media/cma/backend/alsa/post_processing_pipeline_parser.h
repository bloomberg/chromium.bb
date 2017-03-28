// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ALSA_POST_PROCESSING_PIPELINE_PARSER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ALSA_POST_PROCESSING_PIPELINE_PARSER_H_

#include <memory>
#include <string>

#include "base/macros.h"

namespace base {
class DictionaryValue;
class ListValue;
}  // namespace base

namespace chromecast {
namespace media {

// Helper class to parse post-processing pipeline descriptor file.
class PostProcessingPipelineParser {
 public:
  PostProcessingPipelineParser();
  ~PostProcessingPipelineParser();

  // Reads the pipeline descriptor file and does preliminary parsing.
  // Crashes with fatal log if parsing fails.
  void Initialize();

  // Gets the list of processors for a given stream type.
  // The format will be:
  // [
  //   {"processor": "PATH_TO_SHARED_OBJECT",
  //    "config": "CONFIGURATION_STRING"},
  //   {"processor": "PATH_TO_SHARED_OBJECT",
  //    "config": "CONFIGURATION_STRING"},
  //    ...
  // ]
  base::ListValue* GetPipelineByDeviceId(const std::string& device_id);

 private:
  std::unique_ptr<base::DictionaryValue> config_dict_;
  base::DictionaryValue* pipeline_dict_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PostProcessingPipelineParser);
};

}  // namepsace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ALSA_POST_PROCESSING_PIPELINE_PARSER_H_
