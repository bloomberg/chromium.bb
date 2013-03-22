// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_TRACKING_SYNCHRONIZER_OBSERVER_H_
#define CHROME_BROWSER_METRICS_TRACKING_SYNCHRONIZER_OBSERVER_H_

namespace tracked_objects {
struct ProcessDataSnapshot;
}

namespace chrome_browser_metrics {

// Observer for notifications from the TrackingSynchronizer class.
class TrackingSynchronizerObserver {
 public:
  // Received |profiler_data| from a single process of |process_type|.
  // The observer should assume there might be more data coming until
  // |FinishedReceivingData()| is called.
  virtual void ReceivedProfilerData(
      const tracked_objects::ProcessDataSnapshot& profiler_data,
      int process_type) = 0;

  // The observer should not expect any more calls to |ReceivedProfilerData()|
  // (without re-registering).  This is sent either when data from all processes
  // has been gathered, or when the request times out.
  virtual void FinishedReceivingProfilerData() {}

 protected:
  TrackingSynchronizerObserver() {}
  virtual ~TrackingSynchronizerObserver() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TrackingSynchronizerObserver);
};

}  // namespace chrome_browser_metrics

#endif  // CHROME_BROWSER_METRICS_TRACKING_SYNCHRONIZER_OBSERVER_H_
