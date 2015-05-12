// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/two_step_edge_cycler.h"

#include <cstdlib>

namespace ash {
namespace {

// We cycle to the second mode if any of the following happens while the mouse
// is on the edge of the workspace:
// . The user stops moving the mouse for |kMaxDelay| and then moves the mouse
//   again.
// . The mouse moves |kMaxPixels| horizontal pixels.
// . The mouse is moved |kMaxMoves| times.
const int kMaxDelay = 500;
const int kMaxPixels = 100;
const int kMaxMoves = 25;

}  // namespace

TwoStepEdgeCycler::TwoStepEdgeCycler(const gfx::Point& start)
    : second_mode_(false),
      time_last_move_(base::TimeTicks::Now()),
      num_moves_(0),
      start_x_(start.x()) {
}

TwoStepEdgeCycler::~TwoStepEdgeCycler() {
}

void TwoStepEdgeCycler::OnMove(const gfx::Point& location) {
  if (second_mode_)
    return;

  ++num_moves_;
  second_mode_ =
      (base::TimeTicks::Now() - time_last_move_).InMilliseconds() >
          kMaxDelay ||
      std::abs(location.x() - start_x_) >= kMaxPixels ||
      num_moves_ >= kMaxMoves;
  time_last_move_ = base::TimeTicks::Now();
}

}  // namespace ash
