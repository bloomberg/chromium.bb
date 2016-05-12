// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_memory_pressure_monitor.h"

#include <algorithm>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"

namespace chromecast {
namespace {

// Memory thresholds (as fraction of total memory) for memory pressure levels.
// See more detailed description of pressure heuristic in PollPressureLevel.
// TODO(halliwell): tune thresholds based on data.
constexpr float kCriticalMemoryThreshold = 0.2f;
constexpr float kModerateMemoryThreshold = 0.3f;
constexpr int kPollingIntervalMS = 5000;

int GetSystemReservedKb() {
  int rtn_kb_ = 0;
  const base::CommandLine* command_line(base::CommandLine::ForCurrentProcess());
  base::StringToInt(
      command_line->GetSwitchValueASCII(switches::kMemPressureSystemReservedKb),
      &rtn_kb_);
  DCHECK(rtn_kb_ >= 0);
  return std::max(rtn_kb_, 0);
}

}  // namespace

CastMemoryPressureMonitor::CastMemoryPressureMonitor()
    : current_level_(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE),
      system_reserved_kb_(GetSystemReservedKb()),
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
    // 'system reserved kb' allows OEMs to customise when memory pressure is
    // triggered.
    const float max_free =
        info.free + info.buffers + info.cached - system_reserved_kb_;
    const float total = info.total - system_reserved_kb_;
    DCHECK(total > 0);

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
