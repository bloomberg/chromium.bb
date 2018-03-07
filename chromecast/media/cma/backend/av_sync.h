// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_AV_SYNC_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_AV_SYNC_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "chromecast/public/media/media_pipeline_backend.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace chromecast {
namespace media {

class MediaPipelineBackendForMixer;

class AvSync {
 public:
  static std::unique_ptr<AvSync> Create(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      MediaPipelineBackendForMixer* backend);

  virtual ~AvSync() = default;

  virtual void NotifyAudioBufferPushed(
      int64_t buffer_timestamp,
      MediaPipelineBackend::AudioDecoder::RenderingDelay delay) = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_AV_SYNC_H_
