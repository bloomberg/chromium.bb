// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_pipeline_backend_factory.h"

#include "chromecast/media/cma/backend/media_pipeline_backend_manager.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/media_pipeline_device_params.h"

namespace chromecast {
namespace media {

MediaPipelineBackendFactory::MediaPipelineBackendFactory(
    MediaPipelineBackendManager* media_pipeline_backend_manager)
    : media_pipeline_backend_manager_(media_pipeline_backend_manager) {
  DCHECK(media_pipeline_backend_manager_);
}

MediaPipelineBackendFactory::~MediaPipelineBackendFactory() {}

std::unique_ptr<MediaPipelineBackend>
MediaPipelineBackendFactory::CreateBackend(
    const MediaPipelineDeviceParams& params,
    const std::string& audio_device_id) {
  return media_pipeline_backend_manager_->CreateMediaPipelineBackend(params);
}

}  // namespace media
}  // namespace chromecast
