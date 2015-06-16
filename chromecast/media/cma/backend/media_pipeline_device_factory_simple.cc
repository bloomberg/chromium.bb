// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_pipeline_device_factory_default.h"

namespace chromecast {
namespace media {

// Creates default (no-op) media pipeline device elements.
scoped_ptr<MediaPipelineDeviceFactory> GetMediaPipelineDeviceFactory(
    const MediaPipelineDeviceParams& params) {
  return make_scoped_ptr(new MediaPipelineDeviceFactoryDefault());
}

}  // namespace media
}  // namespace chromecast
