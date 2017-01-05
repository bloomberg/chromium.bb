// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_SINK_DEFAULT_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_SINK_DEFAULT_H_

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/time/default_tick_clock.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "media/base/time_delta_interpolator.h"

namespace chromecast {
namespace media {

class MediaSinkDefault {
 public:
  MediaSinkDefault(MediaPipelineBackend::Decoder::Delegate* delegate,
                   base::TimeDelta start_pts);
  ~MediaSinkDefault();

  void SetPlaybackRate(float rate);
  base::TimeDelta GetCurrentPts();
  MediaPipelineBackend::BufferStatus PushBuffer(CastDecoderBuffer* buffer);

 private:
  void ScheduleEndOfStreamTask();

  MediaPipelineBackend::Decoder::Delegate* delegate_;
  base::DefaultTickClock tick_clock_;
  ::media::TimeDeltaInterpolator time_interpolator_;
  float playback_rate_;
  base::TimeDelta last_frame_pts_;
  bool received_eos_;
  base::CancelableClosure eos_task_;

  DISALLOW_COPY_AND_ASSIGN(MediaSinkDefault);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_SINK_DEFAULT_H_
