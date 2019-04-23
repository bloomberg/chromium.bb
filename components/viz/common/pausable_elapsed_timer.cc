// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/pausable_elapsed_timer.h"

namespace viz {

PausableElapsedTimer::PausableElapsedTimer() {}

base::TimeDelta PausableElapsedTimer::Elapsed() const {
  if (active_timer_)
    return previous_elapsed_ + active_timer_->Elapsed();
  else
    return previous_elapsed_;
}

void PausableElapsedTimer::Pause() {
  if (!active_timer_)
    return;
  previous_elapsed_ += active_timer_->Elapsed();
  active_timer_.reset();
}

void PausableElapsedTimer::Resume() {
  if (!active_timer_)
    active_timer_.emplace();
}

}  // namespace viz
