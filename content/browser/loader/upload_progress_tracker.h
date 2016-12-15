// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_UPLOAD_PROGRESS_TRACKER_H_
#define CONTENT_BROWSER_LOADER_UPLOAD_PROGRESS_TRACKER_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace tracked_objects {
class Location;
}

namespace net {
class URLRequest;
}

namespace content {

// UploadProgressTracker watches the upload progress of a URL loading, and sends
// the progress to the client in a suitable granularity and frequency.
class UploadProgressTracker final {
 public:
  using UploadProgressReportCallback =
      base::RepeatingCallback<void(int64_t, int64_t)>;

  UploadProgressTracker(const tracked_objects::Location& location,
                        UploadProgressReportCallback report_progress,
                        net::URLRequest* request);
  ~UploadProgressTracker();

  void OnAckReceived();
  void OnUploadCompleted();

 private:
  void ReportUploadProgressIfNeeded();

  net::URLRequest* request_;  // Not owned.

  uint64_t last_upload_position_ = 0;
  bool waiting_for_upload_progress_ack_ = false;
  base::TimeTicks last_upload_ticks_;
  base::RepeatingTimer progress_timer_;

  UploadProgressReportCallback report_progress_;

  DISALLOW_COPY_AND_ASSIGN(UploadProgressTracker);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_UPLOAD_PROGRESS_TRACKER_H_
