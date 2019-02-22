// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_EVALUATOR_HELPER_WIN_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_EVALUATOR_HELPER_WIN_H_

#include "chrome/browser/performance_monitor/system_monitor.h"

namespace performance_monitor {

class MetricEvaluatorsHelperWin : public MetricEvaluatorsHelper {
 public:
  MetricEvaluatorsHelperWin();
  ~MetricEvaluatorsHelperWin() override;

  // MetricEvaluatorsHelper:
  base::Optional<int> GetFreePhysicalMemoryMb() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MetricEvaluatorsHelperWin);
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_EVALUATOR_HELPER_WIN_H_
