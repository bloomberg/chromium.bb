// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/video/av_sync_video.h"

#include <algorithm>
#include <iomanip>
#include <limits>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromecast/media/cma/backend/audio_decoder_for_mixer.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_for_mixer.h"
#include "chromecast/media/cma/backend/video_decoder_for_mixer.h"

#define LIMITED_CAST_MEDIA_LOG(level, count, max)                             \
  LOG_IF(level, (count) < (max) && ((count)++ || true))                       \
      << (((count) == (max)) ? "(Log limit reached. Further similar entries " \
                               "may be suppressed): "                         \
                             : "")

namespace chromecast {
namespace media {

namespace {

// Threshold where the audio and video pts are far enough apart such that we
// want to do a small correction.
const int kSoftCorrectionThresholdUs = 32000;

// Threshold where the audio and video pts are close enough to each other that
// we want to execute an in sync correction.
const int kInSyncCorrectionThresholdUs = 5000;

// When doing a soft correction, we will do so by changing the rate of video
// playback. These constants define the multiplier in either direction.
const double kRateReduceMultiplier = 0.998;
const double kRateIncreaseMultiplier = 1.002;

// Length of time after which data is forgotten from our linear regression
// models.
const int kLinearRegressionDataLifetimeUs =
    base::TimeDelta::FromMinutes(1).InMicroseconds();

// Time interval between AV sync upkeeps.
constexpr base::TimeDelta kAvSyncUpkeepInterval =
    base::TimeDelta::FromMilliseconds(16);

// Time interval between checking playbacks statistics.
constexpr base::TimeDelta kPlaybackStatisticsCheckInterval =
    base::TimeDelta::FromSeconds(5);

// The amount of time we wait after a correction before we start upkeeping the
// AV sync again.
const int kMinimumWaitAfterCorrectionUs = 200 * 1000;  // 200ms.

// This is the threshold for which we consider the rate of playback variation
// to be valid. If we measure a rate of playback variation worse than this, we
// consider the linear regression measurement invalid, we flush the linear
// regression and let AvSync collect samples all over again.
const double kExpectedSlopeVariance = 0.005;

// The threshold after which LIMITED_CAST_MEDIA_LOG will no longer write the
// logs.
const int kCastMediaLogThreshold = 3;

// We don't AV sync content with frame rate less than this. This low framerate
// indicates that the content happens to be audio-centric, with a dummy video
// stream.
const int kAvSyncFpsThreshold = 10;
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
      backend_(backend) {
  DCHECK(backend_);
}

void AvSyncVideo::UpkeepAvSync() {
  if (backend_->MonotonicClockNow() - kAvSyncUpkeepInterval.InMicroseconds() <
      playback_start_timestamp_us_) {
    return;
  }

  if (!backend_->video_decoder() || !backend_->audio_decoder()) {
    VLOG(4) << "No video decoder available.";
    return;
  }

  // Currently the audio pipeline doesn't seem to return valid values for the
  // PTS after changing the playback rate.
  if (last_correction_timestamp_us != INT64_MIN &&
      backend_->MonotonicClockNow() - last_correction_timestamp_us <
          kMinimumWaitAfterCorrectionUs) {
    return;
  }

  int64_t now = backend_->MonotonicClockNow();
  int64_t current_apts = 0;
  double error = 0.0;

  int64_t new_current_vpts = 0;
  int64_t new_vpts_timestamp = 0;
  if (!backend_->video_decoder()->GetCurrentPts(&new_vpts_timestamp,
                                                &new_current_vpts)) {
    LIMITED_CAST_MEDIA_LOG(ERROR, spammy_log_count_, kCastMediaLogThreshold)
        << "Failed to get VPTS.";
    return;
  }

  if (new_current_vpts != last_vpts_value_recorded_) {
    video_pts_->AddSample(new_vpts_timestamp, new_current_vpts, 1.0);
    last_vpts_value_recorded_ = new_current_vpts;
  }

  int64_t new_current_apts = 0;
  int64_t new_apts_timestamp = 0;

  if (!backend_->audio_decoder()->GetTimestampedPts(&new_apts_timestamp,
                                                    &new_current_apts)) {
    LIMITED_CAST_MEDIA_LOG(ERROR, spammy_log_count_, kCastMediaLogThreshold)
        << "Failed to get APTS.";
    return;
  }

  audio_pts_->AddSample(new_apts_timestamp, new_current_apts, 1.0);

  if (video_pts_->num_samples() < 10 || audio_pts_->num_samples() < 20) {
    VLOG(4) << "Linear regression samples too little."
            << " video_pts_->num_samples()=" << video_pts_->num_samples()
            << " audio_pts_->num_samples()=" << audio_pts_->num_samples();
    return;
  }

  int64_t current_vpts = 0;
  double vpts_slope = 0.0;
  double apts_slope = 0.0;
  double vpts_slope_error = 0.0;
  if (!video_pts_->EstimateY(now, &current_vpts, &error) ||
      !audio_pts_->EstimateY(now, &current_apts, &error) ||
      !video_pts_->EstimateSlope(&vpts_slope, &vpts_slope_error) ||
      !audio_pts_->EstimateSlope(&apts_slope, &error)) {
    VLOG(3) << "Failed to get linear regression estimate.";
    return;
  }

  if (vpts_slope_error > 0.0002) {
    VLOG(3) << "vpts slope estimate error too big.";
    return;
  }

  if (abs(vpts_slope - current_video_playback_rate_) > kExpectedSlopeVariance) {
    LOG(ERROR) << "Calculated bad vpts_slope=" << vpts_slope
               << ". Expected value close to=" << current_video_playback_rate_
               << ". Flushing...";
    FlushVideoPts();
    return;
  }

  if (abs(apts_slope - current_audio_playback_rate_) > kExpectedSlopeVariance) {
    LOG(ERROR) << "Calculated bad apts_slope=" << apts_slope
               << ". Expected value close to=" << current_audio_playback_rate_
               << ". Flushing...";
    FlushAudioPts();
    return;
  }

  if (!first_video_pts_received_) {
    LOG(INFO) << "Video starting at difference="
              << (new_vpts_timestamp - new_current_vpts) -
                     (playback_start_timestamp_us_ - playback_start_pts_us_);
    first_video_pts_received_ = true;
  }

  if (!first_audio_pts_received_) {
    LOG(INFO) << "Audio starting at difference="
              << (new_apts_timestamp - new_current_apts) -
                     (playback_start_timestamp_us_ - playback_start_pts_us_);
    first_audio_pts_received_ = true;
  }

  int64_t timestamp_for_0_vpts = new_vpts_timestamp - new_current_vpts;
  int64_t timestamp_for_0_apts = new_apts_timestamp - new_current_apts;
  int64_t difference = timestamp_for_0_vpts - timestamp_for_0_apts;

  VLOG(3) << "Pts_monitor."
          << " difference=" << difference / 1000 << " apts_slope=" << apts_slope
          << " current_audio_playback_rate_=" << current_audio_playback_rate_
          << " current_vpts=" << new_current_vpts
          << " current_apts=" << new_current_apts
          << " current_time=" << backend_->MonotonicClockNow()
          << " vpts_slope=" << vpts_slope
          << " vpts_slope_error=" << vpts_slope_error
          << " video_pts_->num_samples()=" << video_pts_->num_samples()
          << " dropped_frames=" << backend_->video_decoder()->GetDroppedFrames()
          << " repeated_frames="
          << backend_->video_decoder()->GetRepeatedFrames();

  av_sync_difference_sum_ += difference;
  ++av_sync_difference_count_;

  if (GetContentFrameRate() < kAvSyncFpsThreshold) {
    VLOG(3) << "Content frame rate=" << GetContentFrameRate()
            << ". Not AV syncing.";
  }

  if (abs(difference) > kSoftCorrectionThresholdUs) {
    SoftCorrection(now, current_vpts, current_apts, apts_slope, vpts_slope,
                   difference);
  } else if (abs(difference) < kInSyncCorrectionThresholdUs) {
    InSyncCorrection(now, current_vpts, current_apts, apts_slope, vpts_slope,
                     difference);
  }
}

int AvSyncVideo::GetContentFrameRate() {
  const std::deque<WeightedMovingLinearRegression::Sample>& video_samples =
      video_pts_->samples();

  if (video_pts_->num_samples() < 2) {
    return INT_MAX;
  }
  int duration = video_samples.back().x - video_samples.front().x;

  if (duration <= 0) {
    return INT_MAX;
  }

  return std::round(static_cast<float>(video_pts_->num_samples() * 1000000) /
                    static_cast<float>(duration));
}

void AvSyncVideo::FlushAudioPts() {
  audio_pts_.reset(
      new WeightedMovingLinearRegression(kLinearRegressionDataLifetimeUs));
}

void AvSyncVideo::FlushVideoPts() {
  video_pts_.reset(
      new WeightedMovingLinearRegression(kLinearRegressionDataLifetimeUs));
}

void AvSyncVideo::SoftCorrection(int64_t now,
                                 int64_t current_vpts,
                                 int64_t current_apts,
                                 double apts_slope,
                                 double vpts_slope,
                                 int64_t difference) {
  if (audio_pts_->num_samples() < 50) {
    VLOG(4) << "Not enough apts samples=" << audio_pts_->num_samples();
    return;
  }

  if (in_soft_correction_ &&
      std::abs(difference) < difference_at_start_of_correction_) {
    VLOG(4) << " difference=" << difference / 1000
            << " difference_at_start_of_correction_="
            << difference_at_start_of_correction_ / 1000;
    return;
  }

  double factor = current_apts > current_vpts ? kRateReduceMultiplier
                                              : kRateIncreaseMultiplier;
  current_audio_playback_rate_ *= (vpts_slope * factor / apts_slope);

  current_audio_playback_rate_ =
      backend_->audio_decoder()->SetPlaybackRate(current_audio_playback_rate_);

  number_of_soft_corrections_++;
  in_soft_correction_ = true;
  difference_at_start_of_correction_ = abs(difference);

  audio_pts_.reset(
      new WeightedMovingLinearRegression(kLinearRegressionDataLifetimeUs));

  LOG(INFO) << "Soft Correction."
            << " difference=" << difference / 1000
            << " apts_slope=" << apts_slope << " vpts_slope=" << vpts_slope
            << " current_apts=" << current_apts
            << " current_vpts=" << current_vpts
            << " current_audio_playback_rate_=" << current_audio_playback_rate_;

  last_correction_timestamp_us = backend_->MonotonicClockNow();
}

// This method only does anything if in_soft_correction_ == true, which is the
// case if the last correction we've executed is a soft_correction.
//
// The soft correction will aim to bridge the gap between the audio and video,
// and so after the soft correction is executed, the audio and video rate of
// playback will not be equal.
//
// This 'correction' gets executed when the audio and video PTS are
// sufficiently close to each other, and we no longer need to bridge a gap
// between them. This method will have it so that vpts_slope == apts_slope, and
// the content should continue to play in sync from here on out.
void AvSyncVideo::InSyncCorrection(int64_t now,
                                   int64_t current_vpts,
                                   int64_t current_apts,
                                   double apts_slope,
                                   double vpts_slope,
                                   int64_t difference) {
  if (audio_pts_->num_samples() < 50 || !in_soft_correction_) {
    return;
  }

  current_audio_playback_rate_ *= vpts_slope / apts_slope;
  current_audio_playback_rate_ =
      backend_->audio_decoder()->SetPlaybackRate(current_audio_playback_rate_);
  in_soft_correction_ = false;
  difference_at_start_of_correction_ = 0;

  audio_pts_.reset(
      new WeightedMovingLinearRegression(kLinearRegressionDataLifetimeUs));

  LOG(INFO) << "In sync Correction."
            << " difference=" << difference / 1000
            << " apts_slope=" << apts_slope << " vpts_slope=" << vpts_slope
            << " current_apts=" << current_apts
            << " current_vpts=" << current_vpts
            << " current_audio_playback_rate_=" << current_audio_playback_rate_;

  last_correction_timestamp_us = backend_->MonotonicClockNow();
}

void AvSyncVideo::GatherPlaybackStatistics() {
  DCHECK(backend_);
  if (!backend_->video_decoder()) {
    return;
  }

  int64_t frame_rate_difference =
      (backend_->video_decoder()->GetCurrentContentRefreshRate() -
       backend_->video_decoder()->GetOutputRefreshRate()) /
      1000;

  int64_t expected_dropped_frames_per_second =
      std::max<int64_t>(frame_rate_difference, 0);

  int64_t expected_repeated_frames_per_second =
      std::max<int64_t>(-frame_rate_difference, 0);

  int64_t current_time = backend_->MonotonicClockNow();

  int64_t expected_dropped_frames =
      std::round(static_cast<float>(expected_dropped_frames_per_second) *
                 (static_cast<float>(current_time - last_gather_timestamp_us_) /
                  1000000.0));

  int64_t expected_repeated_frames =
      std::round(static_cast<float>(expected_repeated_frames_per_second) *
                 (static_cast<float>(current_time - last_gather_timestamp_us_) /
                  1000000.0));

  int64_t dropped_frames = backend_->video_decoder()->GetDroppedFrames();
  int64_t repeated_frames = backend_->video_decoder()->GetRepeatedFrames();

  int64_t unexpected_dropped_frames =
      (dropped_frames - last_dropped_frames_) - expected_dropped_frames;
  int64_t unexpected_repeated_frames =
      (repeated_frames - last_repeated_frames_) - expected_repeated_frames;

  double average_av_sync_difference = 0.0;

  if (av_sync_difference_count_ != 0) {
    average_av_sync_difference = static_cast<double>(av_sync_difference_sum_) /
                                 static_cast<double>(av_sync_difference_count_);
  }
  av_sync_difference_sum_ = 0;
  av_sync_difference_count_ = 0;

  int64_t accurate_vpts = 0;
  int64_t accurate_vpts_timestamp = 0;
  backend_->video_decoder()->GetCurrentPts(&accurate_vpts_timestamp,
                                           &accurate_vpts);

  int64_t accurate_apts = 0;
  int64_t accurate_apts_timestamp = 0;
  backend_->video_decoder()->GetCurrentPts(&accurate_apts_timestamp,
                                           &accurate_apts);

  LOG(INFO) << "Playback diagnostics:"
            << " average_av_sync_difference="
            << average_av_sync_difference / 1000
            << " content fps=" << GetContentFrameRate()
            << " unexpected_dropped_frames=" << unexpected_dropped_frames
            << " unexpected_repeated_frames=" << unexpected_repeated_frames;

  int64_t current_vpts = 0;
  int64_t current_apts = 0;
  double error = 0.0;
  if (!video_pts_->EstimateY(current_time, &current_vpts, &error) ||
      !audio_pts_->EstimateY(current_time, &current_apts, &error)) {
    VLOG(3) << "Failed to get linear regression estimate.";
    return;
  }

  if (delegate_) {
    delegate_->NotifyAvSyncPlaybackStatistics(
        unexpected_dropped_frames, unexpected_repeated_frames,
        average_av_sync_difference, current_vpts, current_vpts,
        number_of_soft_corrections_, number_of_hard_corrections_);
  }

  last_gather_timestamp_us_ = current_time;
  last_repeated_frames_ = repeated_frames;
  last_dropped_frames_ = dropped_frames;
  number_of_soft_corrections_ = 0;
  number_of_hard_corrections_ = 0;
}

void AvSyncVideo::NotifyStart(int64_t timestamp, int64_t pts) {
  playback_start_timestamp_us_ = timestamp;
  playback_start_pts_us_ = pts;
  LOG(INFO) << __func__
            << " playback_start_timestamp_us_=" << playback_start_timestamp_us_
            << " playback_start_pts_us_=" << playback_start_pts_us_;

  StartAvSync();
}

void AvSyncVideo::NotifyStop() {
  LOG(INFO) << __func__;
  StopAvSync();
  playback_start_timestamp_us_ = INT64_MAX;
  playback_start_pts_us_ = INT64_MIN;
}

void AvSyncVideo::NotifyPause() {
  StopAvSync();
}

void AvSyncVideo::NotifyResume() {
  int64_t now = backend_->MonotonicClockNow();

  // If for some reason we get a resume before we hit the playback start time,
  // we need to retain it.
  if (now > playback_start_timestamp_us_) {
    playback_start_timestamp_us_ = now;
  }
  LOG(INFO) << __func__
            << " playback_start_timestamp_us_=" << playback_start_timestamp_us_;
  StartAvSync();
}

void AvSyncVideo::NotifyPlaybackRateChange(float rate) {
  current_audio_playback_rate_ =
      backend_->audio_decoder()->SetPlaybackRate(rate);

  current_video_playback_rate_ = rate;

  in_soft_correction_ = false;
  difference_at_start_of_correction_ = 0;

  FlushAudioPts();
  FlushVideoPts();
}

void AvSyncVideo::StartAvSync() {
  audio_pts_.reset(
      new WeightedMovingLinearRegression(kLinearRegressionDataLifetimeUs));
  video_pts_.reset(
      new WeightedMovingLinearRegression(kLinearRegressionDataLifetimeUs));

  number_of_soft_corrections_ = 0;
  number_of_hard_corrections_ = 0;
  in_soft_correction_ = false;
  difference_at_start_of_correction_ = 0;
  first_audio_pts_received_ = false;
  first_video_pts_received_ = false;

  spammy_log_count_ = 0;

  upkeep_av_sync_timer_.Start(FROM_HERE, kAvSyncUpkeepInterval, this,
                              &AvSyncVideo::UpkeepAvSync);
  // TODO(almasrymina): if this logic turns out to be useful for metrics
  // recording, keep it and remove this TODO. Otherwise remove it.
  playback_statistics_timer_.Start(FROM_HERE, kPlaybackStatisticsCheckInterval,
                                   this,
                                   &AvSyncVideo::GatherPlaybackStatistics);
}

void AvSyncVideo::StopAvSync() {
  upkeep_av_sync_timer_.Stop();
  playback_statistics_timer_.Stop();
}

AvSyncVideo::~AvSyncVideo() = default;

}  // namespace media
}  // namespace chromecast
