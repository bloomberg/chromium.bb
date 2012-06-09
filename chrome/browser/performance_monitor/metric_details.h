// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_DETAILS_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_DETAILS_H_
#pragma once

#include <string>

namespace performance_monitor {

struct MetricDetails {
  MetricDetails();
  MetricDetails(const std::string& metric_name,
                const std::string& metric_description);
  ~MetricDetails();

  std::string name;
  std::string description;
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_METRIC_DETAILS_H_
