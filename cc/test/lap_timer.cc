// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/lap_timer.h"

#include "base/logging.h"

namespace cc {

LapTimer::LapTimer(int warmup_laps,
                   base::TimeDelta time_limit,
                   int check_interval)
    : warmup_laps_(warmup_laps),
      time_limit_(time_limit),
      check_interval_(check_interval) {
  DCHECK_GT(check_interval, 0);
  Reset();
}

void LapTimer::Reset() {
  accumulator_ = base::TimeDelta();
  num_laps_ = 0;
  accumulated_ = true;
  remaining_warmups_ = warmup_laps_;
  Start();
}

void LapTimer::Start() { start_time_ = base::TimeTicks::HighResNow(); }

bool LapTimer::IsWarmedUp() { return remaining_warmups_ <= 0; }

void LapTimer::NextLap() {
  if (!IsWarmedUp()) {
    --remaining_warmups_;
    if (IsWarmedUp()) {
      Start();
    }
    return;
  }
  ++num_laps_;
  accumulated_ = (num_laps_ % check_interval_) == 0;
  if (accumulated_) {
    base::TimeTicks now = base::TimeTicks::HighResNow();
    accumulator_ += now - start_time_;
    start_time_ = now;
  }
}

bool LapTimer::HasTimeLimitExpired() { return accumulator_ >= time_limit_; }

float LapTimer::MsPerLap() {
  DCHECK(accumulated_);
  return accumulator_.InMillisecondsF() / num_laps_;
}

float LapTimer::LapsPerSecond() {
  DCHECK(accumulated_);
  return num_laps_ / accumulator_.InSecondsF();
}

int LapTimer::NumLaps() { return num_laps_; }

}  // namespace cc
