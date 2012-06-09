// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/metric_details.h"

namespace performance_monitor {

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
