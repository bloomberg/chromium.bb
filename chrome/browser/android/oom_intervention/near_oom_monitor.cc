// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/oom_intervention/near_oom_monitor.h"

namespace {

// Default SwapFree/SwapTotal ratio for detecting near-OOM situation.
// TODO(bashi): Confirm that this is appropriate.
const float kDefaultSwapFreeThresholdRatio = 0.20;

// Default interval to check memory stats.
constexpr base::TimeDelta kDefaultMonitoringDelta =
    base::TimeDelta::FromSeconds(1);

}  // namespace

NearOomMonitor::NearOomMonitor()
    : monitoring_interval_(kDefaultMonitoringDelta), swapfree_threshold_(0) {}

NearOomMonitor::NearOomMonitor(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : NearOomMonitor() {
  timer_.SetTaskRunner(task_runner);
}

NearOomMonitor::~NearOomMonitor() = default;

bool NearOomMonitor::Start() {
  base::SystemMemoryInfoKB memory_info;
  if (!GetSystemMemoryInfo(&memory_info))
    return false;

  // If there is no swap (zram) we don't start the monitor because we use
  // SwapFree as the tracking metric.
  if (memory_info.swap_total == 0)
    return false;

  swapfree_threshold_ = static_cast<int64_t>(memory_info.swap_total *
                                             kDefaultSwapFreeThresholdRatio);

  timer_.Start(FROM_HERE, monitoring_interval_, this, &NearOomMonitor::Check);
  return true;
}

bool NearOomMonitor::IsRunning() const {
  return timer_.IsRunning();
}

std::unique_ptr<NearOomMonitor::Subscription> NearOomMonitor::RegisterCallback(
    base::Closure callback) {
  return callbacks_.Add(std::move(callback));
}

bool NearOomMonitor::GetSystemMemoryInfo(
    base::SystemMemoryInfoKB* memory_info) {
  DCHECK(memory_info);
  return base::GetSystemMemoryInfo(memory_info);
}

void NearOomMonitor::Check() {
  base::SystemMemoryInfoKB memory_info;
  if (!GetSystemMemoryInfo(&memory_info)) {
    LOG(WARNING) << "Failed to get system memory info and stop monitoring.";
    timer_.Stop();
    return;
  }

  if (memory_info.swap_free <= swapfree_threshold_) {
    callbacks_.Notify();
  }
}
