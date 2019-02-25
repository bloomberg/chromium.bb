// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/metric_evaluator_helper_win.h"

#include <windows.h>

#include "base/optional.h"
#include "base/task_runner_util.h"

namespace performance_monitor {

namespace {

const DWORDLONG kMBBytes = 1024 * 1024;

}  // namespace

MetricEvaluatorsHelperWin::MetricEvaluatorsHelperWin()
    : wmi_initialization_sequence_(base::CreateSequencedTaskRunnerWithTraits(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      wmi_refresher_(new win::WMIRefresher(),
                     base::OnTaskRunnerDeleter(wmi_initialization_sequence_)),
      weak_factory_(this) {
  // TODO(sebmarchand): Boost the priority of this task if the WMI refresher is
  // needed before this task had a chance to run.
  base::PostTaskAndReplyWithResult(
      wmi_initialization_sequence_.get(), FROM_HERE,
      base::BindOnce(&win::WMIRefresher::InitializeDiskIdleTimeConfig,
                     base::Unretained(wmi_refresher_.get())),
      base::BindOnce(&MetricEvaluatorsHelperWin::OnWMIRefresherInitialized,
                     weak_factory_.GetWeakPtr()));
}

MetricEvaluatorsHelperWin::~MetricEvaluatorsHelperWin() = default;

base::Optional<int> MetricEvaluatorsHelperWin::GetFreePhysicalMemoryMb() {
  MEMORYSTATUSEX mem_status;
  mem_status.dwLength = sizeof(mem_status);
  if (!::GlobalMemoryStatusEx(&mem_status))
    return base::nullopt;

  return (mem_status.ullAvailPhys / kMBBytes);
}

base::Optional<float> MetricEvaluatorsHelperWin::GetDiskIdleTimePercent() {
  if (wmi_refresher_initialized_)
    return wmi_refresher_->RefreshAndGetDiskIdleTimeInPercent();
  return base::nullopt;
}

}  // namespace performance_monitor
