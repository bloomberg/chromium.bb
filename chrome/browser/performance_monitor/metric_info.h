// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_INFO_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_INFO_H_
#pragma once

#include <string>

#include "base/time.h"

namespace performance_monitor {

struct MetricInfo {
  MetricInfo();
  MetricInfo(const base::Time& metric_time, double metric_value);
  MetricInfo(const std::string& metric_time, const std::string& metric_value);
  ~MetricInfo();

  base::Time time;
  double value;
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_INFO_H_
