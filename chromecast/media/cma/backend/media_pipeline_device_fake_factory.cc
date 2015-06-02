// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_pipeline_device_fake.h"

#include "base/memory/scoped_ptr.h"

namespace chromecast {
namespace media {

scoped_ptr<MediaPipelineDevice> CreateMediaPipelineDevice(
    const MediaPipelineDeviceParams& params) {
  return scoped_ptr<MediaPipelineDevice>(new MediaPipelineDeviceFake());
}

}  // namespace media
}  // namespace chromecast
