// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_POST_PROCESSOR_FACTORY_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_POST_PROCESSOR_FACTORY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"

namespace base {
class ScopedNativeLibrary;
}  // namespace base

namespace chromecast {
namespace media {

class AudioPostProcessor;

class PostProcessorFactory {
 public:
  PostProcessorFactory();
  ~PostProcessorFactory();

  std::unique_ptr<AudioPostProcessor> CreatePostProcessor(
      const std::string& so_name,
      const std::string& config,
      int channels);

 private:
  // Contains all libraries in use;
  // Functions in shared objects cannot be used once library is closed.
  std::vector<std::unique_ptr<base::ScopedNativeLibrary>> libraries_;

  DISALLOW_COPY_AND_ASSIGN(PostProcessorFactory);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_POST_PROCESSOR_FACTORY_H_
