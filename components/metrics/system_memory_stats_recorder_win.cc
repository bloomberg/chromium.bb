// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/system_memory_stats_recorder.h"

#include <windows.h>

#include "base/metrics/histogram_macros.h"
#include "base/process/process_metrics.h"

namespace metrics {

void RecordMemoryStats(RecordMemoryStatsType type) {
  MEMORYSTATUSEX mem_status;
  mem_status.dwLength = sizeof(mem_status);
  if (!::GlobalMemoryStatusEx(&mem_status))
    return;

  switch (type) {
    case RECORD_MEMORY_STATS_TAB_DISCARDED: {
      UMA_HISTOGRAM_CUSTOM_COUNTS("Memory.Stats.Win.MemoryLoad",
                                  mem_status.dwMemoryLoad, 0, 100, 101);
      UMA_HISTOGRAM_LARGE_MEMORY_MB("Memory.Stats.Win.TotalPhys",
                                    mem_status.ullTotalPhys);
      UMA_HISTOGRAM_LARGE_MEMORY_MB("Memory.Stats.Win.AvailPhys",
                                    mem_status.ullAvailPhys);
      UMA_HISTOGRAM_LARGE_MEMORY_MB("Memory.Stats.Win.TotalPageFile",
                                    mem_status.ullTotalPageFile);
      UMA_HISTOGRAM_LARGE_MEMORY_MB("Memory.Stats.Win.AvailPageFile",
                                    mem_status.ullAvailPageFile);
      UMA_HISTOGRAM_LARGE_MEMORY_MB("Memory.Stats.Win.TotalVirtual",
                                    mem_status.ullTotalVirtual);
      UMA_HISTOGRAM_LARGE_MEMORY_MB("Memory.Stats.Win.AvailVirtual",
                                    mem_status.ullAvailVirtual);
      break;
    }
    default:
      NOTREACHED() << L"Received unexpected notification";
      break;
  }
}

}  // namespace metrics
