// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/periodic_metrics_recorder.h"

#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/wm/window_state.h"
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

  enum ActiveWindowShowType {
    ACTIVE_WINDOW_SHOW_TYPE_NO_ACTIVE_WINDOW,
    ACTIVE_WINDOW_SHOW_TYPE_OTHER,
    ACTIVE_WINDOW_SHOW_TYPE_MAXIMIZED,
    ACTIVE_WINDOW_SHOW_TYPE_FULLSCREEN,
    ACTIVE_WINDOW_SHOW_TYPE_SNAPPED,
    ACTIVE_WINDOW_SHOW_TYPE_COUNT
  };
  ActiveWindowShowType active_window_show_type =
      ACTIVE_WINDOW_SHOW_TYPE_NO_ACTIVE_WINDOW;
  wm::WindowState* active_window_state = ash::wm::GetActiveWindowState();
  if (active_window_state) {
    switch(active_window_state->window_show_type()) {
      case wm::SHOW_TYPE_MAXIMIZED:
        active_window_show_type = ACTIVE_WINDOW_SHOW_TYPE_MAXIMIZED;
        break;
      case wm::SHOW_TYPE_FULLSCREEN:
        active_window_show_type = ACTIVE_WINDOW_SHOW_TYPE_FULLSCREEN;
        break;
      case wm::SHOW_TYPE_LEFT_SNAPPED:
      case wm::SHOW_TYPE_RIGHT_SNAPPED:
        active_window_show_type = ACTIVE_WINDOW_SHOW_TYPE_SNAPPED;
        break;
      case wm::SHOW_TYPE_DEFAULT:
      case wm::SHOW_TYPE_NORMAL:
      case wm::SHOW_TYPE_MINIMIZED:
      case wm::SHOW_TYPE_INACTIVE:
      case wm::SHOW_TYPE_DETACHED:
      case wm::SHOW_TYPE_END:
      case wm::SHOW_TYPE_AUTO_POSITIONED:
        active_window_show_type = ACTIVE_WINDOW_SHOW_TYPE_OTHER;
        break;
    }
    UMA_HISTOGRAM_ENUMERATION("Ash.ActiveWindowShowTypeOverTime",
                              active_window_show_type,
                              ACTIVE_WINDOW_SHOW_TYPE_COUNT);
  }
}

}  // namespace ash
