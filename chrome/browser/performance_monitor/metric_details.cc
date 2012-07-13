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
const char* kMetricTypeNames[] = {
  kSampleMetricName
};
COMPILE_ASSERT(ARRAYSIZE_UNSAFE(kMetricTypeNames) == METRIC_NUMBER_OF_METRICS,
               metric_names_incorrect_size);

}  // namespace

const char* MetricTypeToString(MetricType metric_type) {
  DCHECK_GT(METRIC_NUMBER_OF_METRICS, metric_type);
  return kMetricTypeNames[metric_type];
}

MetricDetails::MetricDetails() {
}

MetricDetails::MetricDetails(const std::string& metric_name,
                             const std::string& metric_description)
    : name(metric_name),
      description(metric_description) {
}

MetricDetails::~MetricDetails() {
}

}  // namespace performance_monitor
