// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_memory_pressure_monitor.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/process/process_metrics.h"
#include "base/thread_task_runner_handle.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"

namespace chromecast {
namespace {

// Memory thresholds (as fraction of total memory) for memory pressure levels.
// See more detailed description of pressure heuristic in PollPressureLevel.
// TODO(halliwell): tune thresholds based on data.
constexpr float kCriticalMemoryThreshold = 0.2f;
constexpr float kModerateMemoryThreshold = 0.3f;
constexpr int kPollingIntervalMS = 5000;

}  // namespace

CastMemoryPressureMonitor::CastMemoryPressureMonitor()
    : current_level_(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE),
      weak_ptr_factory_(this) {
  PollPressureLevel();
}

CastMemoryPressureMonitor::~CastMemoryPressureMonitor() {}

CastMemoryPressureMonitor::MemoryPressureLevel
CastMemoryPressureMonitor::GetCurrentPressureLevel() const {
  return current_level_;
}

void CastMemoryPressureMonitor::PollPressureLevel() {
  MemoryPressureLevel level =
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;

  base::SystemMemoryInfoKB info;
  if (!base::GetSystemMemoryInfo(&info)) {
    LOG(ERROR) << "GetSystemMemoryInfo failed";
  } else {
    // 'buffers' and 'cached' memory is included in free memory calculation,
    // because the kernel can typically free some of it when necessary;
    // using just 'free' memory would lead to us being in memory pressure state
    // nearly all of the time.
    // However, some cached memory cannot be reclaimed, and even when it can,
    // it can cause performance problems.  To counter this, the memory pressure
    // thresholds should be high enough that we take action well before running
    // out of all available memory.
    // TODO(halliwell): provide 'system reserved' amount for OEMs to configure.
    const float max_free = info.free + info.buffers + info.cached;
    const float total = info.total;

    if ((max_free / total) < kCriticalMemoryThreshold)
      level = base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
    else if ((max_free / total) < kModerateMemoryThreshold)
      level = base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
  }

  UpdateMemoryPressureLevel(level);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&CastMemoryPressureMonitor::PollPressureLevel,
                            weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kPollingIntervalMS));
}

void CastMemoryPressureMonitor::UpdateMemoryPressureLevel(
    MemoryPressureLevel new_level) {
  if (new_level != base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE)
    base::MemoryPressureListener::NotifyMemoryPressure(new_level);

  if (new_level == current_level_)
    return;

  current_level_ = new_level;
  metrics::CastMetricsHelper::GetInstance()->RecordApplicationEventWithValue(
      "Memory.Pressure.LevelChange", new_level);
}

}  // namespace chromecast
