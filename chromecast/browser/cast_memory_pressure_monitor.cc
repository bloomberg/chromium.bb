// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_memory_pressure_monitor.h"

#include <sys/sysinfo.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/thread_task_runner_handle.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"

namespace chromecast {
namespace {

// This memory threshold is set for Chromecast. See the UMA histogram
// Platform.MeminfoMemFree when tuning.
// TODO(halliwell): Allow these to be customised, also take buffers and
// cache memory into account.
const int kCriticalMinFreeMemMB = 24;
const int kPollingIntervalMS = 5000;

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
  struct sysinfo sys;

  if (sysinfo(&sys) == -1) {
    LOG(ERROR) << "platform_poll_freemem(): sysinfo failed";
  } else {
    int free_mem_mb =
        static_cast<int64_t>(sys.freeram) * sys.mem_unit / (1024 * 1024);

    if (free_mem_mb <= kCriticalMinFreeMemMB) {
      level = base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
    }
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
