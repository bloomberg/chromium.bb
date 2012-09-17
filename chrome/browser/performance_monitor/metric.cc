// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/metric.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"

namespace performance_monitor {

Metric::Metric() {
  value = 0.0;
}

Metric::Metric(const base::Time& metric_time, const double metric_value)
    : time(metric_time), value(metric_value) {
}

Metric::Metric(const std::string& metric_time,
               const std::string& metric_value) {
  int64 conversion = 0;
  base::StringToInt64(metric_time, &conversion);
  time = base::Time::FromInternalValue(conversion);
  CHECK(base::StringToDouble(metric_value, &value));
}

Metric::~Metric() {
}

}  // namespace performance_monitor
