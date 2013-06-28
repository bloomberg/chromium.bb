// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/metric.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/performance_monitor/constants.h"

namespace performance_monitor {

namespace {

// For certain metrics (for instance, bytes read), it is possible that there is
// no maximum value which we can safely assume.
const double kNoMaximum = -1.0;

// These constants are designed to keep metrics reasonable. However, due to the
// variety of system configurations which can run chrome, these values may not
// catch *all* erroneous values. For instance, on a one-CPU machine, any CPU
// usage > 100 is erroneous, but on a 16-CPU machine, it's perfectly normal.
// These are "best-guesses" in order to weed out obviously-false values. A
// metric is valid if it is greater than or equal to the minimum and less than
// the maximum, i.e. if it falls in the range [min, max).
const double kMinUndefined = 0.0;
const double kMaxUndefined = 0.0;  // No undefined metric is valid.
const double kMinCpuUsage = 0.0;
const double kMaxCpuUsage = 100000.0;  // 100% on a 1000-CPU machine.
const double kMinPrivateMemoryUsage = 0.0;
const double kMaxPrivateMemoryUsage = kBytesPerTerabyte;
const double kMinSharedMemoryUsage = 0.0;
const double kMaxSharedMemoryUsage = kBytesPerTerabyte;
const double kMinStartupTime = 0.0;
const double kMaxStartupTime = base::Time::kMicrosecondsPerMinute * 15.0;
const double kMinTestStartupTime = 0.0;
const double kMaxTestStartupTime = base::Time::kMicrosecondsPerMinute * 15.0;
const double kMinSessionRestoreTime = 0.0;
const double kMaxSessionRestoreTime = base::Time::kMicrosecondsPerMinute * 15.0;
const double kMinPageLoadTime = 0.0;
const double kMaxPageLoadTime = base::Time::kMicrosecondsPerMinute * 15.0;
const double kMinNetworkBytesRead = 0.0;
const double kMaxNetworkBytesRead = kNoMaximum;

struct MetricBound {
  double min;
  double max;
};

const MetricBound kMetricBounds[] = {
 { kMinUndefined, kMaxUndefined },
 { kMinCpuUsage, kMaxCpuUsage },
 { kMinPrivateMemoryUsage, kMaxPrivateMemoryUsage },
 { kMinSharedMemoryUsage, kMaxSharedMemoryUsage },
 { kMinStartupTime, kMaxStartupTime },
 { kMinTestStartupTime, kMaxTestStartupTime },
 { kMinSessionRestoreTime, kMaxSessionRestoreTime },
 { kMinPageLoadTime, kMaxPageLoadTime },
 { kMinNetworkBytesRead, kMaxNetworkBytesRead },
};

COMPILE_ASSERT(ARRAYSIZE_UNSAFE(kMetricBounds) == METRIC_NUMBER_OF_METRICS,
               metric_bounds_size_doesnt_match_metric_count);

}  // namespace

Metric::Metric() : type(METRIC_UNDEFINED), value(0.0) {
}

Metric::Metric(MetricType metric_type,
               const base::Time& metric_time,
               const double metric_value)
    : type(metric_type), time(metric_time), value(metric_value) {
}

Metric::Metric(MetricType metric_type,
               const std::string& metric_time,
               const std::string& metric_value) : type(metric_type) {
  int64 conversion = 0;
  base::StringToInt64(metric_time, &conversion);
  time = base::Time::FromInternalValue(conversion);
  CHECK(base::StringToDouble(metric_value, &value));
}

Metric::~Metric() {
}

bool Metric::IsValid() const {
  return type < METRIC_NUMBER_OF_METRICS &&
      (value < kMetricBounds[type].max ||
          kMetricBounds[type].max == kNoMaximum) &&
      value >= kMetricBounds[type].min;
}

std::string Metric::ValueAsString() const {
  return base::DoubleToString(value);
}

}  // namespace performance_monitor
