// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/metric_info.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"

namespace performance_monitor {

MetricInfo::MetricInfo() {
  value = 0.0;
}

MetricInfo::MetricInfo(const base::Time& metric_time, double metric_value)
    : time(metric_time),
      value(metric_value) {
}

MetricInfo::MetricInfo(const std::string& metric_time,
                       const std::string& metric_value) {
  int64 conversion = 0;
  base::StringToInt64(metric_time, &conversion);
  time = base::Time::FromInternalValue(conversion);
  CHECK(base::StringToDouble(metric_value, &value));
}

MetricInfo::~MetricInfo() {
}

}  // namespace performance_monitor
