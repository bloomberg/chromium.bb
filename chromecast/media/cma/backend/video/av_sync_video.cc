// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/video/av_sync_video.h"

#include <iomanip>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_for_mixer.h"
#include "chromecast/media/cma/backend/video_decoder_for_mixer.h"

namespace chromecast {
namespace media {

namespace {

// Threshold where the audio and video pts are far enough apart such that we
// want to do a small correction.
const int kSoftCorrectionThresholdUs = 16000;

// Threshold where the audio and video pts are far enough apart such that we
// want to do a hard correction.
const int kHardCorrectionThresholdUs = 200000;

// When doing a soft correction, we will do so by changing the rate of video
// playback. These constants define the multiplier in either direction.
const double kRateReduceMultiplier = 0.9;
const double kRateIncreaseMultiplier = 1.1;

// Length of time after which data is forgotten from our linear regression
// models.
const int kLinearRegressionDataLifetimeUs = 500000;

// Time interval between AV sync upkeeps.
constexpr base::TimeDelta kAvSyncUpkeepInterval =
    base::TimeDelta::FromMilliseconds(10);
}  // namespace

std::unique_ptr<AvSync> AvSync::Create(
    const scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    MediaPipelineBackendForMixer* const backend) {
  return std::make_unique<AvSyncVideo>(task_runner, backend);
}

AvSyncVideo::AvSyncVideo(
    const scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    MediaPipelineBackendForMixer* const backend)
    : audio_pts_(
          new WeightedMovingLinearRegression(kLinearRegressionDataLifetimeUs)),
      video_pts_(
          new WeightedMovingLinearRegression(kLinearRegressionDataLifetimeUs)),
      error_(
          new WeightedMovingLinearRegression(kLinearRegressionDataLifetimeUs)),
      task_runner_(task_runner),
      backend_(backend) {
  DCHECK(backend_);
}

void AvSyncVideo::NotifyAudioBufferPushed(
    int64_t buffer_timestamp,
    MediaPipelineBackend::AudioDecoder::RenderingDelay delay) {
  if (delay.timestamp_microseconds == INT64_MIN ||
      buffer_timestamp == INT64_MAX)
    return;

  audio_pts_->AddSample(delay.timestamp_microseconds,
                        buffer_timestamp - (delay.delay_microseconds), 1.0);
}

// TODO(almasrymina): this code is the core of the av sync logic, and the
// current state is that it seems to work very well in local testing under very
// extreme conditions. Nevertheless, much of the constants here are arbitrary,
// and should be optimized:
// - It's arbitrary to move the rate of the video clock by 0.1 for corrections.
// This value should probably depend on the current error.
// - Hard correction value of 200ms is arbitrary.
// - Current requirements for number of samples in the linear regression is
// arbitrary.
void AvSyncVideo::UpkeepAvSync() {
  if (!backend_->video_decoder()) {
    VLOG(4) << "No video decoder available.";
    return;
  }

  int64_t now = backend_->MonotonicClockNow();  // 'now'...
  int64_t current_apts;
  double error;

  if (!setup_video_clock_) {
    // TODO(almasrymina): If we don't have a valid delay at the start of
    // playback, we should push silence to the mixer to get a valid delay
    // before we start content playback.
    if (audio_pts_->num_samples() > 1) {
      audio_pts_->EstimateY(now, &current_apts, &error);

      LOG(INFO) << "Setting up video clock. current_apts=" << current_apts;

      backend_->video_decoder()->SetCurrentPts(current_apts);
      setup_video_clock_ = true;
    }
    return;
  }

  video_pts_->AddSample(now, backend_->video_decoder()->GetCurrentPts(), 1.0);

  if (video_pts_->num_samples() < 5 || audio_pts_->num_samples() < 20) {
    VLOG(4) << "Linear regression samples too little."
            << " video_pts_->num_samples()=" << video_pts_->num_samples()
            << " audio_pts_->num_samples()=" << audio_pts_->num_samples();
    return;
  }

  int64_t current_vpts;
  double vpts_slope;
  double apts_slope;
  video_pts_->EstimateY(now, &current_vpts, &error);
  audio_pts_->EstimateY(now, &current_apts, &error);
  video_pts_->EstimateSlope(&vpts_slope, &error);
  audio_pts_->EstimateSlope(&apts_slope, &error);

  error_->AddSample(now, current_apts - current_vpts, 1.0);

  if (error_->num_samples() < 5) {
    VLOG(4)
        << "Error linear regression samples too little. error_->num_samples()="
        << error_->num_samples();
    return;
  }

  int64_t difference;
  error_->EstimateY(now, &difference, &error);

  VLOG(4) << "Pts_monitor."
          << " current_apts=" << current_apts / 1000
          << " current_vpts=" << std::setw(5) << current_vpts / 1000
          << " difference=" << std::setw(5) << difference / 1000
          << " wall_time=" << std::setw(5) << now / 1000
          << " apts_slope=" << std::setw(10) << apts_slope
          << " vpts_slope=" << std::setw(10) << vpts_slope;

  // Seems the ideal value here depends on the frame rate.
  if (abs(difference) > kSoftCorrectionThresholdUs) {
    VLOG(2) << "Correction."
            << " current_apts=" << current_apts / 1000
            << " current_vpts=" << std::setw(5) << current_vpts / 1000
            << " difference=" << std::setw(5) << difference / 1000
            << " wall_time=" << std::setw(5) << now / 1000
            << " apts_slope=" << std::setw(10) << apts_slope
            << " vpts_slope=" << std::setw(10) << vpts_slope;

    if (abs(difference) > kHardCorrectionThresholdUs) {
      // Do a hard correction.
      audio_pts_->EstimateY(backend_->MonotonicClockNow(), &current_apts,
                            &error);
      backend_->video_decoder()->SetCurrentPts(current_apts);
      backend_->video_decoder()->SetPlaybackRate(apts_slope);
      current_video_playback_rate_ = apts_slope;
    } else {
      // Do a soft correction.
      double factor = current_vpts > current_apts ? kRateReduceMultiplier
                                                  : kRateIncreaseMultiplier;
      current_video_playback_rate_ *= factor;
      backend_->video_decoder()->SetPlaybackRate(current_video_playback_rate_);
    }
    video_pts_.reset(
        new WeightedMovingLinearRegression(kLinearRegressionDataLifetimeUs));
    error_.reset(
        new WeightedMovingLinearRegression(kLinearRegressionDataLifetimeUs));
  } else {
    // We're in sync. Reset rate.
    // TODO(almasrymina): is this call correct? Probably not for extreme cases
    // where the video clock drifts significantly relative to monotonic_raw.
    // Instead of setting the playback rate to apts_slope, we should aim to
    // find the video playback rate at which vtps_slope == apts_slope. These
    // are slightly different values since the video playback rate is probably
    // not phase locked at all with monotonic_raw.
    backend_->video_decoder()->SetPlaybackRate(apts_slope);
    current_video_playback_rate_ = apts_slope;
  }
}

void AvSyncVideo::StopAvSync() {
  audio_pts_.reset(
      new WeightedMovingLinearRegression(kLinearRegressionDataLifetimeUs));
  video_pts_.reset(
      new WeightedMovingLinearRegression(kLinearRegressionDataLifetimeUs));
  error_.reset(
      new WeightedMovingLinearRegression(kLinearRegressionDataLifetimeUs));
  timer_.Stop();
}

void AvSyncVideo::NotifyStart() {
  timer_.Start(FROM_HERE, kAvSyncUpkeepInterval, this,
               &AvSyncVideo::UpkeepAvSync);
}

void AvSyncVideo::NotifyStop() {
  StopAvSync();
  setup_video_clock_ = false;
}

void AvSyncVideo::NotifyPause() {
  StopAvSync();
}

void AvSyncVideo::NotifyResume() {
  timer_.Start(FROM_HERE, kAvSyncUpkeepInterval, this,
               &AvSyncVideo::UpkeepAvSync);
}

AvSyncVideo::~AvSyncVideo() = default;

}  // namespace media
}  // namespace chromecast
