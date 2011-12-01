// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DOWNLOAD_RATE_MONITOR_H_
#define MEDIA_BASE_DOWNLOAD_RATE_MONITOR_H_

#include <deque>

#include "base/bind.h"
#include "base/callback.h"
#include "base/time.h"
#include "media/base/media_export.h"

namespace media {

// Measures download speed based on the rate at which a media file is buffering.
// Signals when it believes the media file can be played back without needing to
// pause to buffer.
class MEDIA_EXPORT DownloadRateMonitor {
 public:
  DownloadRateMonitor();
  ~DownloadRateMonitor();

  // Begin measuring download rate. The monitor will run |canplaythrough_cb|
  // when it believes the media can be played back without needing to pause to
  // buffer. |media_bitrate| is the bitrate of the video.
  void Start(const base::Closure& canplaythrough_cb, int media_bitrate,
             bool streaming, bool local_source);

  // Notifies the monitor of the current number of bytes buffered by the media
  // file at what timestamp. The monitor expects subsequent calls to
  // SetBufferedBytes to have monotonically nondecreasing timestamps.
  // Calls to the method are ignored if monitor has not been started or is
  // stopped.
  void SetBufferedBytes(int64 buffered_bytes, const base::Time& timestamp);

  // Notifies the monitor when the media file has paused or continued
  // downloading data.
  void SetNetworkActivity(bool is_downloading_data);

  void set_total_bytes(int64 total_bytes) { total_bytes_ = total_bytes; }

  // Stop monitoring download rate. This does not discard previously learned
  // information, but it will no longer factor incoming information into its
  // canplaythrough estimation.
  void Stop();

  // Resets monitor to uninitialized state.
  void Reset();

 private:
  // Represents a point in time in which the media was buffering data.
  struct BufferingPoint {
    // The number of bytes buffered by the media player at |timestamp|.
    int64 buffered_bytes;
    // Time at which buffering measurement was taken.
    base::Time timestamp;
  };

  // Represents a span of time in which the media was buffering data.
  class Sample {
   public:
    Sample();
    Sample(const BufferingPoint& start, const BufferingPoint& end);
    ~Sample();

    // Set the end point of the data sample.
    void set_end(const BufferingPoint& new_end);

    const BufferingPoint& start() const { return start_; }
    const BufferingPoint& end() const { return end_; }

    // Returns the bytes downloaded per second in this sample. Returns -1.0 if
    // sample is invalid.
    float bytes_per_second() const;

    // Returns the seconds elapsed in this sample. Returns -1.0 if the sample
    // has not begun yet.
    float seconds_elapsed() const;

    // Returns bytes downloaded in this sample. Returns -1.0 if the sample has
    // not begun yet.
    int64 bytes_downloaded() const;

    // Returns true if Sample has not been initialized.
    bool is_null() const;

    // Resets the sample to an uninitialized state.
    void Reset();

    // Restarts the sample to begin its measurement at what was previously its
    // end point.
    void RestartAtEndBufferingPoint();

   private:
    BufferingPoint start_;
    BufferingPoint end_;
  };

  int64 bytes_downloaded_in_window() const;
  float seconds_elapsed_in_window() const;

  // Updates window with latest sample if it is ready.
  void UpdateSampleWindow();

  // Returns an approximation of the current download rate in bytes per second.
  // Returns -1.0 if unknown.
  float ApproximateDownloadByteRate() const;

  // Helper method that returns true if the monitor believes it should fire the
  // |canplaythrough_cb_|.
  bool ShouldNotifyCanPlayThrough();

  // Examines the download rate and fires the |canplaythrough_cb_| callback if
  // the monitor deems it the right time.
  void NotifyCanPlayThroughIfNeeded();

  // Callback to run when the monitor believes the media can play through
  // without needing to pause to buffer.
  base::Closure canplaythrough_cb_;

  // Indicates whether the monitor has run the |canplaythrough_cb_|.
  bool has_notified_can_play_through_;

  // Measurements used to approximate download speed.
  Sample current_sample_;
  std::deque<Sample> sample_window_;

  // True if actively downloading bytes, false otherwise.
  bool is_downloading_data_;

  // Total number of bytes in the media file, 0 if unknown or undefined.
  int64 total_bytes_;

  // Amount of bytes buffered.
  int64 buffered_bytes_;

  // True if the media file is from a local source, e.g. file:// protocol or a
  // webcam stream.
  bool local_source_;

  // Bitrate of the media file, 0 if unknown.
  int bitrate_;

  // True if the monitor has not yet started or has been stopped, false
  // otherwise.
  bool stopped_;

  // True if the data source is a streaming source, false otherwise.
  bool streaming_;

  DISALLOW_COPY_AND_ASSIGN(DownloadRateMonitor);
};

}  // namespace media

#endif  // MEDIA_BASE_DOWNLOAD_RATE_MONITOR_H_
