// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/periodic_metrics_recorder.h"

#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "base/metrics/histogram.h"

namespace ash {

// Time in seconds between calls to "RecordMetrics".
const int kAshPeriodicMetricsTimeInSeconds = 30 * 60;

PeriodicMetricsRecorder::PeriodicMetricsRecorder() {
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromSeconds(kAshPeriodicMetricsTimeInSeconds),
               this,
               &PeriodicMetricsRecorder::RecordMetrics);
}

PeriodicMetricsRecorder::~PeriodicMetricsRecorder() {
  timer_.Stop();
}

void PeriodicMetricsRecorder::RecordMetrics() {
  internal::ShelfLayoutManager* manager =
      internal::ShelfLayoutManager::ForLauncher(Shell::GetPrimaryRootWindow());
  if (manager) {
    UMA_HISTOGRAM_ENUMERATION("Ash.ShelfAlignmentOverTime",
        manager->SelectValueForShelfAlignment(
            internal::SHELF_ALIGNMENT_UMA_ENUM_VALUE_BOTTOM,
            internal::SHELF_ALIGNMENT_UMA_ENUM_VALUE_LEFT,
            internal::SHELF_ALIGNMENT_UMA_ENUM_VALUE_RIGHT,
            -1),
        internal::SHELF_ALIGNMENT_UMA_ENUM_VALUE_COUNT);
  }
}

}  // namespace ash
