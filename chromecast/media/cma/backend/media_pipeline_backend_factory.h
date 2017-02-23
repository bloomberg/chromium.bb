// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_BACKEND_FACTORY_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_BACKEND_FACTORY_H_

#include <memory>
#include <string>

#include "base/macros.h"

namespace chromecast {
namespace media {

class MediaPipelineBackend;
class MediaPipelineBackendManager;
struct MediaPipelineDeviceParams;

// Creates MediaPipelineBackends using a given MediaPipelineBackendManager.
class MediaPipelineBackendFactory {
 public:
  // TODO(slan): Use a static Create method once all of the constructor
  // dependencies are removed from the internal implemenation.
  explicit MediaPipelineBackendFactory(
      MediaPipelineBackendManager* media_pipeline_backend_manager);
  virtual ~MediaPipelineBackendFactory();

  virtual std::unique_ptr<MediaPipelineBackend> CreateBackend(
      const MediaPipelineDeviceParams& params,
      const std::string& audio_device_id);

 protected:
  MediaPipelineBackendManager* media_pipeline_backend_manager() {
    return media_pipeline_backend_manager_;
  }

 private:
  media::MediaPipelineBackendManager* const media_pipeline_backend_manager_;

  DISALLOW_COPY_AND_ASSIGN(MediaPipelineBackendFactory);
};

}  // media
}  // chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_BACKEND_FACTORY_H_
