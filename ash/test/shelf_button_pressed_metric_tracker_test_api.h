// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_SHELF_BUTTON_PRESSED_METRIC_TRACKER_TEST_API_H_
#define ASH_TEST_SHELF_BUTTON_PRESSED_METRIC_TRACKER_TEST_API_H_

#include "ash/common/shelf/shelf_button_pressed_metric_tracker.h"

#include <memory>

#include "base/macros.h"
#include "ui/events/event.h"

namespace base {
class TickClock;
}  // namespace base

namespace ash {
namespace test {

class ShelfButtonPressedMetricTrackerTestAPI {
 public:
  explicit ShelfButtonPressedMetricTrackerTestAPI(
      ShelfButtonPressedMetricTracker* shelf_button_pressed_metric_tracker);
  ~ShelfButtonPressedMetricTrackerTestAPI();

  // Set's the |tick_clock_| on the internal ShelfButtonPressedMetricTracker.
  void SetTickClock(std::unique_ptr<base::TickClock> tick_clock);

 private:
  ShelfButtonPressedMetricTracker* shelf_button_pressed_metric_tracker_;

  DISALLOW_COPY_AND_ASSIGN(ShelfButtonPressedMetricTrackerTestAPI);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_SHELF_BUTTON_PRESSED_METRIC_TRACKER_TEST_API _H_
