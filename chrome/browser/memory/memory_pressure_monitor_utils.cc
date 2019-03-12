// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/memory_pressure_monitor_utils.h"

#include "base/logging.h"

namespace memory {

FreeMemoryObservationWindow::FreeMemoryObservationWindow(const Config& config)
    : ObservationWindow(config.window_length), config_(config) {}

FreeMemoryObservationWindow::~FreeMemoryObservationWindow() = default;

bool FreeMemoryObservationWindow::MemoryIsUnderEarlyLimit() {
  return MemoryIsUnderLimitImpl(sample_below_early_limit_count_);
}

bool FreeMemoryObservationWindow::MemoryIsUnderCriticalLimit() {
  return MemoryIsUnderLimitImpl(sample_below_critical_limit_count_);
}

void FreeMemoryObservationWindow::OnSampleAdded(const int& sample) {
  if (sample < config_.low_memory_early_limit_mb)
    ++sample_below_early_limit_count_;
  if (sample < config_.low_memory_critical_limit_mb)
    ++sample_below_critical_limit_count_;
}

void FreeMemoryObservationWindow::OnSampleRemoved(const int& sample) {
  if (sample < config_.low_memory_early_limit_mb)
    --sample_below_early_limit_count_;
  if (sample < config_.low_memory_critical_limit_mb)
    --sample_below_critical_limit_count_;
}

bool FreeMemoryObservationWindow::MemoryIsUnderLimitImpl(
    size_t sample_under_limit_cnt) {
  size_t sample_count = SampleCount();
  if (sample_count < config_.min_sample_count)
    return false;
  return (static_cast<float>(sample_under_limit_cnt) / sample_count) >=
         config_.sample_ratio_to_be_positive;
}

}  // namespace memory
