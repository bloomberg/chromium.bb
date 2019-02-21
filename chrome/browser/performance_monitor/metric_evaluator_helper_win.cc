// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/metric_evaluator_helper_win.h"

#include <windows.h>

#include "base/optional.h"

namespace performance_monitor {

namespace {

const DWORDLONG kMBBytes = 1024 * 1024;

}  // namespace

MetricEvaluatorsHelperWin::MetricEvaluatorsHelperWin() = default;
MetricEvaluatorsHelperWin::~MetricEvaluatorsHelperWin() = default;

base::Optional<int> MetricEvaluatorsHelperWin::GetFreePhysicalMemoryMb() {
  MEMORYSTATUSEX mem_status;
  mem_status.dwLength = sizeof(mem_status);
  if (!::GlobalMemoryStatusEx(&mem_status))
    return base::nullopt;

  return (mem_status.ullAvailPhys / kMBBytes);
}

}  // namespace performance_monitor
