// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/filtered_memory_pressure_calculator.h"

#include "base/time/tick_clock.h"

namespace memory_pressure {

#if defined(MEMORY_PRESSURE_IS_POLLING)

// 100ms (10Hz) allows a relatively fast respsonse time for rapidly increasing
// memory usage, but limits the amount of work done in the calculator.
const int FilteredMemoryPressureCalculator::kMinimumTimeBetweenSamplesMs = 100;

// These values were experimentally obtained during the initial ChromeOS only
// implementation of this feature. By spending a significant cooldown period in
// at a higher pressure level more time is dedicated to freeing resources and
// less churn occurs in the MemoryPressureListener event stream.
const int FilteredMemoryPressureCalculator::kCriticalPressureCooldownPeriodMs =
    5000;
const int FilteredMemoryPressureCalculator::kModeratePressureCooldownPeriodMs =
    5000;

FilteredMemoryPressureCalculator::FilteredMemoryPressureCalculator(
    MemoryPressureCalculator* pressure_calculator,
    base::TickClock* tick_clock)
    : pressure_calculator_(pressure_calculator),
      tick_clock_(tick_clock),
      current_pressure_level_(
          MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE),
      samples_taken_(false),
      cooldown_in_progress_(false) {
  DCHECK(pressure_calculator);
  DCHECK(tick_clock);
}

FilteredMemoryPressureCalculator::~FilteredMemoryPressureCalculator() {
}

FilteredMemoryPressureCalculator::MemoryPressureLevel
FilteredMemoryPressureCalculator::CalculateCurrentPressureLevel() {
  base::TimeTicks now = tick_clock_->NowTicks();

  // If its too soon to take a sample then return the precalculated value.
  if (samples_taken_ &&
      (now - last_sample_time_) <
          base::TimeDelta::FromMilliseconds(kMinimumTimeBetweenSamplesMs)) {
    return current_pressure_level_;
  }

  // Take a sample.
  samples_taken_ = true;
  last_sample_time_ = now;
  MemoryPressureLevel level =
      pressure_calculator_->CalculateCurrentPressureLevel();

  // The pressure hasn't changed or has gone up. In either case this is the end
  // of a cooldown period if one was in progress.
  if (level >= current_pressure_level_) {
    cooldown_in_progress_ = false;
    current_pressure_level_ = level;
    return level;
  }

  // The pressure has gone down, so apply cooldown hysteresis.
  if (cooldown_in_progress_) {
    cooldown_high_tide_ = std::max(cooldown_high_tide_, level);

    // Get the cooldown period for the current level.
    int cooldown_ms = kCriticalPressureCooldownPeriodMs;
    if (current_pressure_level_ ==
        MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE) {
      cooldown_ms = kModeratePressureCooldownPeriodMs;
    }
    base::TimeDelta cooldown_period =
        base::TimeDelta::FromMilliseconds(cooldown_ms);

    if (now - cooldown_start_time_ >= cooldown_period) {
      // The cooldown has completed successfully, so transition the pressure
      // level.
      cooldown_in_progress_ = false;
      current_pressure_level_ = cooldown_high_tide_;
    }
  } else {
    // Start a new cooldown period.
    cooldown_in_progress_ = true;
    cooldown_start_time_ = now;
    cooldown_high_tide_ = level;
  }

  return current_pressure_level_;
}

#endif  // defined(MEMORY_PRESSURE_IS_POLLING)

}  // namespace memory_pressure
