// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_pipeline_device_params.h"

namespace chromecast {
namespace media {

MediaPipelineDeviceParams::MediaPipelineDeviceParams()
    : sync_type(kModeSyncPts) {
}

MediaPipelineDeviceParams::~MediaPipelineDeviceParams() {
}

}  // namespace media
}  // namespace chromecast
