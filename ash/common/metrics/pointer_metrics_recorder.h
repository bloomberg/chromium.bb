// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ASH_COMMON_METRICS_POINTER_METRICS_RECORDER_H_
#define UI_ASH_COMMON_METRICS_POINTER_METRICS_RECORDER_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/views/pointer_watcher.h"

namespace gfx {
class Point;
}

namespace ui {
class PointerEvent;
}

namespace ash {

// A metrics recorder that records pointer related metrics.
class ASH_EXPORT PointerMetricsRecorder : public views::PointerWatcher {
 public:
  PointerMetricsRecorder();
  ~PointerMetricsRecorder() override;

  // views::PointerWatcher:
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              const gfx::Point& location_in_screen,
                              views::Widget* target) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PointerMetricsRecorder);
};

}  // namespace ash

#endif  // UI_ASH_COMMON_METRICS_POINTER_METRICS_RECORDER_H_
