// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/post_processor_factory.h"

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/scoped_native_library.h"
#include "chromecast/public/media/audio_post_processor_shlib.h"

namespace chromecast {
namespace media {

namespace {

const char kSoCreateFunction[] = "AudioPostProcessorShlib_Create";

}  // namespace

using CreatePostProcessorFunction = AudioPostProcessor* (*)(const std::string&,
                                                            int);

PostProcessorFactory::PostProcessorFactory() = default;
PostProcessorFactory::~PostProcessorFactory() = default;

std::unique_ptr<AudioPostProcessor> PostProcessorFactory::CreatePostProcessor(
    const std::string& library_path,
    const std::string& config,
    int channels) {
  libraries_.push_back(base::MakeUnique<base::ScopedNativeLibrary>(
      base::FilePath(library_path)));
  CHECK(libraries_.back()->is_valid())
      << "Could not open post processing library " << library_path;
  auto create = reinterpret_cast<CreatePostProcessorFunction>(
      libraries_.back()->GetFunctionPointer(kSoCreateFunction));

  CHECK(create) << "Could not find " << kSoCreateFunction << "() in "
                << library_path;
  return base::WrapUnique(create(config, channels));
}

}  // namespace media
}  // namespace chromecast
