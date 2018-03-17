// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_BACKEND_FACTORY_IMPL_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_BACKEND_FACTORY_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_factory.h"

namespace chromecast {
namespace media {

class CmaBackend;
class MediaPipelineBackendManager;
struct MediaPipelineDeviceParams;

// Creates CmaBackends using a given MediaPipelineBackendManager.
class MediaPipelineBackendFactoryImpl : public MediaPipelineBackendFactory {
 public:
  // TODO(slan): Use a static Create method once all of the constructor
  // dependencies are removed from the internal implemenation.
  explicit MediaPipelineBackendFactoryImpl(
      MediaPipelineBackendManager* media_pipeline_backend_manager);
  ~MediaPipelineBackendFactoryImpl() override;

  std::unique_ptr<CmaBackend> CreateBackend(
      const MediaPipelineDeviceParams& params) override;

 protected:
  MediaPipelineBackendManager* media_pipeline_backend_manager() {
    return media_pipeline_backend_manager_;
  }

 private:
  media::MediaPipelineBackendManager* const media_pipeline_backend_manager_;

  DISALLOW_COPY_AND_ASSIGN(MediaPipelineBackendFactoryImpl);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_BACKEND_FACTORY_IMPL_H_
