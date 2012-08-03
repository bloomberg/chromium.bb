// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/metric_details.h"

#include "base/logging.h"
#include "chrome/browser/performance_monitor/constants.h"

namespace performance_monitor {
namespace {

// Keep this array synced with MetricTypes in the header file.
// TODO(mtytel): i18n.
const MetricDetails kMetricDetailsList[] = {
  {
    kMetricCPUUsageName,
    kMetricCPUUsageDescription,
    kMetricCPUUsageUnits,
    kMetricCPUUsageTickSize,
  },
  {
    kMetricPrivateMemoryUsageName,
    kMetricPrivateMemoryUsageDescription,
    kMetricPrivateMemoryUsageUnits,
    kMetricPrivateMemoryUsageTickSize
  },
};
COMPILE_ASSERT(ARRAYSIZE_UNSAFE(kMetricDetailsList) == METRIC_NUMBER_OF_METRICS,
               metric_names_incorrect_size);

}  // namespace

const MetricDetails* GetMetricDetails(MetricType metric_type) {
  DCHECK_GT(METRIC_NUMBER_OF_METRICS, metric_type);
  return &kMetricDetailsList[metric_type];
}

}  // namespace performance_monitor
