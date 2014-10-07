// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_DEVICE_PARAMS_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_DEVICE_PARAMS_H_

#include "base/macros.h"

namespace chromecast {
namespace media {

class MediaPipelineDeviceParams {
 public:
  MediaPipelineDeviceParams();
  ~MediaPipelineDeviceParams();

  // When set to true, synchronization is disabled and audio/video frames are
  // rendered "right away":
  // - for audio, frames are still rendered based on the sampling frequency
  // - for video, frames are rendered as soon as available at the output of
  //   the video decoder.
  //   The assumption is that no B frames are used when synchronization is
  //   disabled, otherwise B frames would always be skipped.
  bool disable_synchronization;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_DEVICE_PARAMS_H_
