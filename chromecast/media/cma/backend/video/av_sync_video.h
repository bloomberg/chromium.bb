// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_AV_SYNC_VIDEO_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_AV_SYNC_VIDEO_H_

#include <cstdint>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/timer/timer.h"
#include "chromecast/base/statistics/weighted_moving_linear_regression.h"
#include "chromecast/media/cma/backend/av_sync.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace chromecast {
namespace media {
class MediaPipelineBackendForMixer;

class AvSyncVideo : public AvSync {
 public:
  AvSyncVideo(const scoped_refptr<base::SingleThreadTaskRunner> task_runner,
              MediaPipelineBackendForMixer* const backend);

  ~AvSyncVideo() override;

  // AvSync implementation:
  void NotifyAudioBufferPushed(
      int64_t buffer_timestamp,
      MediaPipelineBackend::AudioDecoder::RenderingDelay delay) override;
  void NotifyStart() override;
  void NotifyStop() override;
  void NotifyPause() override;
  void NotifyResume() override;

 private:
  void UpkeepAvSync();
  void StartAvSync();
  void StopAvSync();
  void GatherPlaybackStatistics();

  base::RepeatingTimer upkeep_av_sync_timer_;
  base::RepeatingTimer playback_statistics_timer_;
  bool setup_video_clock_ = false;

  // TODO(almasrymina): having a linear regression for the audio pts is
  // dangerous, because glitches in the audio or intentional changes in the
  // playback rate will propagate to the regression at a delay. Consider
  // reducing the lifetime of data or firing an event to the av sync module
  // that will reset the linear regression model.
  std::unique_ptr<WeightedMovingLinearRegression> audio_pts_;
  std::unique_ptr<WeightedMovingLinearRegression> video_pts_;
  std::unique_ptr<WeightedMovingLinearRegression> error_;
  double current_video_playback_rate_ = 1.0;

  int64_t last_gather_timestamp_us_ = 0;
  int64_t last_repeated_frames_ = 0;
  int64_t last_dropped_frames_ = 0;

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  MediaPipelineBackendForMixer* const backend_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_AV_SYNC_VIDEO_H_
