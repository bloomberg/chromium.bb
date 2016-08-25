// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_STYLUS_METRICS_RECORDER_H_
#define ASH_WM_STYLUS_METRICS_RECORDER_H_

#include <memory>

#include "base/macros.h"
#include "ui/events/event_handler.h"

namespace ash {

// An event recorder that record stylus related metrics.
class StylusMetricsRecorder : public ui::EventHandler {
 public:
  StylusMetricsRecorder();
  ~StylusMetricsRecorder() override;

 private:
  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

  void RecordUMA(ui::EventPointerType type);

  DISALLOW_COPY_AND_ASSIGN(StylusMetricsRecorder);
};

}  // namespace ash

#endif  // ASH_WM_STYLUS_METRICS_RECORDER_H_
