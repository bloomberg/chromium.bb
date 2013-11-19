// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PERIODIC_METRICS_RECORDER_H_
#define ASH_PERIODIC_METRICS_RECORDER_H_

#include "base/timer/timer.h"

namespace ash {

// Periodic Metrics Recorder provides a repeating callback (RecordMetrics)
// on a timer to allow recording of state data over time to the UMA records.
// Any additional states (in ash) that require monitoring can be added to
// this class.
class PeriodicMetricsRecorder {
 public:
  PeriodicMetricsRecorder();
  ~PeriodicMetricsRecorder();

 private:
  void RecordMetrics();

  base::RepeatingTimer<PeriodicMetricsRecorder> timer_;
};

}  // namespace ash

#endif  // ASH_PERIODIC_METRICS_RECORDER_H_
