// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_BACKEND_FACTORY_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_BACKEND_FACTORY_H_

#include <memory>

namespace chromecast {
namespace media {

class MediaPipelineBackend;
struct MediaPipelineDeviceParams;

// Abstract base class to create MediaPipelineBackend.
class MediaPipelineBackendFactory {
 public:
  virtual ~MediaPipelineBackendFactory() {}

  virtual std::unique_ptr<MediaPipelineBackend> CreateBackend(
      const MediaPipelineDeviceParams& params) = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_BACKEND_FACTORY_H_
