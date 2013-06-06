// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/time_ticks_experiment_win.h"

#if defined(OS_WIN)

#include "base/cpu.h"
#include "base/metrics/histogram.h"
#include "base/win/windows_version.h"

#include <windows.h>

namespace chrome {

namespace {

const int kNumIterations = 1000;

}  // anonymous namespace

void CollectTimeTicksStats() {
  // This bit is supposed to indicate that rdtsc is safe across cores. If so, we
  // can use QPC as long as it uses rdtsc.
  // TODO(simonjam): We should look for other signals that QPC might be safe and
  // test them out here.
  base::CPU cpu;
  UMA_HISTOGRAM_BOOLEAN("WinTimeTicks.NonStopTsc",
                        cpu.has_non_stop_time_stamp_counter());
  if (!cpu.has_non_stop_time_stamp_counter()) {
    return;
  }

  DWORD_PTR default_mask;
  DWORD_PTR system_mask;
  if (!GetProcessAffinityMask(GetCurrentProcess(),
                              &default_mask, &system_mask)) {
    return;
  }
  if (!default_mask) {
    return;
  }

  DWORD_PTR current_mask = 1;
  bool failed_to_change_cores = false;

  base::win::OSInfo* info = base::win::OSInfo::GetInstance();
  UMA_HISTOGRAM_ENUMERATION("WinTimeTicks.VersionTotal", info->version(),
                            base::win::VERSION_WIN_LAST);

  LARGE_INTEGER qpc_frequency;
  QueryPerformanceFrequency(&qpc_frequency);

  int min_delta = 1e9;
  LARGE_INTEGER qpc_last;
  QueryPerformanceCounter(&qpc_last);
  for (int i = 0; i < kNumIterations; ++i) {
    LARGE_INTEGER qpc_now;
    QueryPerformanceCounter(&qpc_now);
    int delta = static_cast<int>(qpc_now.QuadPart - qpc_last.QuadPart);
    if (delta != 0) {
      min_delta = std::min(min_delta, delta);
    }
    qpc_last = qpc_now;

    // Change cores every 10 iterations.
    if (i % 10 == 0) {
      DWORD_PTR old_mask = current_mask;
      current_mask <<= 1;
      while ((current_mask & default_mask) == 0) {
        current_mask <<= 1;
        if (!current_mask) {
          current_mask = 1;
        }
        if (current_mask == old_mask) {
          break;
        }
      }
      if (!SetThreadAffinityMask(GetCurrentThread(), current_mask)) {
        failed_to_change_cores = true;
        break;
      }
    }
  }

  SetThreadAffinityMask(GetCurrentThread(), default_mask);
  if (failed_to_change_cores) {
    UMA_HISTOGRAM_ENUMERATION("WinTimeTicks.FailedToChangeCores",
                              info->version(), base::win::VERSION_WIN_LAST);
    return;
  }

  if (min_delta < 0) {
    UMA_HISTOGRAM_ENUMERATION("WinTimeTicks.TickedBackwards", info->version(),
                              base::win::VERSION_WIN_LAST);
    return;
  }

  int min_delta_ns = static_cast<int>(
      min_delta * (1e9 / qpc_frequency.QuadPart));
  UMA_HISTOGRAM_CUSTOM_COUNTS("WinTimeTicks.MinResolutionNanoseconds",
                              min_delta_ns, 1, 1000000, 50);

  bool success = min_delta_ns <= 10000;
  if (success) {
    UMA_HISTOGRAM_ENUMERATION("WinTimeTicks.VersionSuccessful",
                              info->version(), base::win::VERSION_WIN_LAST);
  }
}

}  // namespace chrome

#endif  // defined(OS_WIN)
