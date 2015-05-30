// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/shelf_button_pressed_metric_tracker_test_api.h"

#include "base/time/tick_clock.h"

namespace ash {
namespace test {

ShelfButtonPressedMetricTrackerTestAPI::ShelfButtonPressedMetricTrackerTestAPI(
    ShelfButtonPressedMetricTracker* shelf_button_pressed_metric_tracker)
    : shelf_button_pressed_metric_tracker_(
          shelf_button_pressed_metric_tracker) {
}

ShelfButtonPressedMetricTrackerTestAPI::
    ~ShelfButtonPressedMetricTrackerTestAPI() {
}

void ShelfButtonPressedMetricTrackerTestAPI::SetTickClock(
    scoped_ptr<base::TickClock> tick_clock) {
  shelf_button_pressed_metric_tracker_->tick_clock_.reset(tick_clock.release());
}

}  // namespace test
}  // namespace ash
