// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/download_rate_monitor.h"

#include "base/bind.h"
#include "base/time.h"

namespace media {

// Number of samples to use to collect and average for each measurement of
// download rate.
static const size_t kNumberOfSamples = 5;

// Minimum number of seconds represented in a sample period.
static const float kSamplePeriod = 1.0;

DownloadRateMonitor::Sample::Sample() {
  Reset();
}

DownloadRateMonitor::Sample::~Sample() { }

DownloadRateMonitor::Sample::Sample(
    const BufferingPoint& start, const BufferingPoint& end) {
  Reset();
  start_ = start;
  set_end(end);
}

void DownloadRateMonitor::Sample::set_end(const BufferingPoint& new_end) {
  DCHECK(!start_.timestamp.is_null());
  DCHECK(new_end.buffered_bytes >= start_.buffered_bytes);
  DCHECK(new_end.timestamp >= start_.timestamp);
  end_ = new_end;
}

float DownloadRateMonitor::Sample::bytes_per_second() const {
  if (seconds_elapsed() > 0.0 && bytes_downloaded() >= 0)
    return bytes_downloaded() / seconds_elapsed();
  return -1.0;
}

float DownloadRateMonitor::Sample::seconds_elapsed() const {
  if (start_.timestamp.is_null() || end_.timestamp.is_null())
    return -1.0;
  return (end_.timestamp - start_.timestamp).InSecondsF();
}

int64 DownloadRateMonitor::Sample::bytes_downloaded() const {
  if (start_.timestamp.is_null() || end_.timestamp.is_null())
    return -1.0;
  return end_.buffered_bytes - start_.buffered_bytes;
}

bool DownloadRateMonitor::Sample::is_null() const {
  return start_.timestamp.is_null() && end_.timestamp.is_null();
}

void DownloadRateMonitor::Sample::Reset() {
  start_ = BufferingPoint();
  end_ = BufferingPoint();
}

void DownloadRateMonitor::Sample::RestartAtEndBufferingPoint() {
  start_ = end_;
  end_ = BufferingPoint();
}

DownloadRateMonitor::DownloadRateMonitor() {
  Reset();
}

void DownloadRateMonitor::Start(
    const base::Closure& canplaythrough_cb, int media_bitrate) {
  DCHECK(stopped_);
  canplaythrough_cb_ = canplaythrough_cb;
  stopped_ = false;
  bitrate_ = media_bitrate;
  current_sample_.Reset();
  buffered_bytes_ = 0;

  NotifyCanPlayThroughIfNeeded();
}

void DownloadRateMonitor::SetBufferedBytes(
    int64 buffered_bytes, const base::Time& timestamp) {
  if (stopped_)
    return;

  is_downloading_data_ = true;

  // Check monotonically nondecreasing constraint.
  base::Time previous_time;
  if (!current_sample_.is_null())
    previous_time = current_sample_.end().timestamp;
  else if (!sample_window_.empty())
    previous_time = sample_window_.back().end().timestamp;

  // If we go backward in time, dismiss the sample.
  if (!previous_time.is_null() && timestamp < previous_time)
    return;

  // If the buffer level has dropped, invalidate current sample.
  if (buffered_bytes < buffered_bytes_)
    current_sample_.Reset();
  buffered_bytes_ = buffered_bytes;

  BufferingPoint latest_point = { buffered_bytes, timestamp };
  if (current_sample_.is_null())
    current_sample_ = Sample(latest_point, latest_point);
  else
    current_sample_.set_end(latest_point);

  UpdateSampleWindow();
  NotifyCanPlayThroughIfNeeded();
}

void DownloadRateMonitor::SetNetworkActivity(bool is_downloading_data) {
  if (is_downloading_data == is_downloading_data_)
    return;
  // Invalidate the current sample if downloading is going from start to stopped
  // or vice versa.
  current_sample_.Reset();
  is_downloading_data_ = is_downloading_data;
}

void DownloadRateMonitor::Stop() {
  stopped_ = true;
  current_sample_.Reset();
  buffered_bytes_ = 0;
}

void DownloadRateMonitor::Reset() {
  canplaythrough_cb_.Reset();
  has_notified_can_play_through_ = false;
  current_sample_.Reset();
  sample_window_.clear();
  is_downloading_data_ = false;
  total_bytes_ = -1;
  buffered_bytes_ = 0;
  loaded_ = false;
  bitrate_ = 0;
  stopped_ = true;
}

DownloadRateMonitor::~DownloadRateMonitor() { }

int64 DownloadRateMonitor::bytes_downloaded_in_window() const {
  // There are max |kNumberOfSamples| so we might as well recompute each time.
  int64 total = 0;
  for (size_t i = 0; i < sample_window_.size(); ++i)
    total += sample_window_[i].bytes_downloaded();
  return total;
}

float DownloadRateMonitor::seconds_elapsed_in_window() const {
  // There are max |kNumberOfSamples| so we might as well recompute each time.
  float total = 0.0;
  for (size_t i = 0; i < sample_window_.size(); ++i)
    total += sample_window_[i].seconds_elapsed();
  return total;
}

void DownloadRateMonitor::UpdateSampleWindow() {
  if (current_sample_.seconds_elapsed() < kSamplePeriod)
    return;

  // Add latest sample and remove oldest sample.
  sample_window_.push_back(current_sample_);
  if (sample_window_.size() > kNumberOfSamples)
    sample_window_.pop_front();

  // Prepare for next measurement.
  current_sample_.RestartAtEndBufferingPoint();
}

float DownloadRateMonitor::ApproximateDownloadByteRate() const {
  // Compute and return the average download byte rate from within the sample
  // window.
  // NOTE: In the unlikely case where the data is arriving really bursty-ly,
  // say getting a big chunk of data every 5 seconds, then with this
  // implementation it will take 25 seconds until bitrate is calculated.
  if (sample_window_.size() >= kNumberOfSamples &&
      seconds_elapsed_in_window() > 0.0) {
    return bytes_downloaded_in_window() / seconds_elapsed_in_window();
  }

  // Could not determine approximate download byte rate.
  return -1.0;
}

bool DownloadRateMonitor::ShouldNotifyCanPlayThrough() {
  if (stopped_)
    return false;

  // Only notify CanPlayThrough once for now.
  if (has_notified_can_play_through_)
    return false;

  // If the media is from a local file (|loaded_|) or if all bytes are
  // buffered, fire CanPlayThrough.
  if (loaded_ || buffered_bytes_ == total_bytes_)
    return true;

  // Cannot approximate when the media can play through if bitrate is unknown.
  if (bitrate_ <= 0)
    return false;

  float bytes_needed_per_second = bitrate_ / 8;
  float download_rate = ApproximateDownloadByteRate();

  // If we are downloading at or faster than the media's bitrate, then we can
  // play through to the end of the media without stopping to buffer.
  if (download_rate > 0)
    return download_rate >= bytes_needed_per_second;

  // If download rate is unknown, it may be because the media is being
  // downloaded so fast that it cannot collect an adequate number of samples
  // before the download gets deferred.
  //
  // To catch this case, we also look at how much data is being downloaded
  // immediately after the download begins.
  if (sample_window_.size() < kNumberOfSamples) {
    int64 bytes_downloaded_since_start =
        bytes_downloaded_in_window() + current_sample_.bytes_downloaded();
    float seconds_elapsed_since_start =
        seconds_elapsed_in_window() + current_sample_.seconds_elapsed();

    // If we download 4 seconds of data in less than 2 seconds of time, we're
    // probably downloading at a fast enough rate that we can play through.
    // This is an arbitrary metric that will likely need tweaking.
    if (seconds_elapsed_since_start < 2.0 &&
        bytes_downloaded_since_start > 4.0 * bytes_needed_per_second) {
      return true;
    }
  }

  return false;
}

void DownloadRateMonitor::NotifyCanPlayThroughIfNeeded() {
  if (ShouldNotifyCanPlayThrough() && !canplaythrough_cb_.is_null()) {
    canplaythrough_cb_.Run();
    has_notified_can_play_through_ = true;
  }
}

}  // namespace media
