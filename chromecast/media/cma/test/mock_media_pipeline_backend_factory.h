// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_TEST_MOCK_MEDIA_PIPELINE_BACKEND_FACTORY_H_
#define CHROMECAST_MEDIA_CMA_TEST_MOCK_MEDIA_PIPELINE_BACKEND_FACTORY_H_

#include "chromecast/media/cma/backend/media_pipeline_backend_factory.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromecast {
namespace media {

class MockMediaPipelineBackendFactory : public MediaPipelineBackendFactory {
 public:
  MockMediaPipelineBackendFactory();
  ~MockMediaPipelineBackendFactory() override;

  MOCK_METHOD1(
      CreateBackend,
      std::unique_ptr<MediaPipelineBackend>(const MediaPipelineDeviceParams&));
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_TEST_MOCK_MEDIA_PIPELINE_BACKEND_FACTORY_H_
