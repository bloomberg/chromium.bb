// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_EVALUATOR_HELPER_WIN_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_EVALUATOR_HELPER_WIN_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "chrome/browser/performance_monitor/system_monitor.h"

namespace performance_monitor {

class MetricEvaluatorsHelperWin : public MetricEvaluatorsHelper {
 public:
  ~MetricEvaluatorsHelperWin() override;

  // MetricEvaluatorsHelper:
  base::Optional<int> GetFreePhysicalMemoryMb() override;

 private:
  friend class MetricEvaluatorsHelperWinTest;
  friend class SystemMonitor;

  // The constructor is made private to enforce that there's only one instance
  // of this class existing at the same time. In practice this instance is meant
  // to be instantiated by the SystemMonitor global instance.
  MetricEvaluatorsHelperWin();

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(MetricEvaluatorsHelperWin);
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_EVALUATOR_HELPER_WIN_H_
