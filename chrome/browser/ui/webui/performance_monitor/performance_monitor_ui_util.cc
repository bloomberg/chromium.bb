// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/performance_monitor/performance_monitor_ui_util.h"

#include "base/time.h"
#include "chrome/browser/performance_monitor/metric.h"

namespace performance_monitor {

double GetConversionFactor(UnitDetails from, UnitDetails to) {
  if (from.measurement_type != to.measurement_type) {
    LOG(ERROR) << "Invalid conversion requested";
    return 0.0;
  }

  return static_cast<double>(from.amount_in_base_units) /
      static_cast<double>(to.amount_in_base_units);
}

scoped_ptr<Database::MetricVector> AggregateMetric(
    const Database::MetricVector* metrics,
    const base::Time& start,
    const base::TimeDelta& resolution) {
  if (!metrics)
    return scoped_ptr<Database::MetricVector>(NULL);

  scoped_ptr<Database::MetricVector> results(new Database::MetricVector());

  // Ignore all the points before the aggregation start.
  Database::MetricVector::const_iterator it = metrics->begin();
  while (it != metrics->end() && it->time < start)
    ++it;

  while (it != metrics->end()) {
    // Finds the beginning of the next aggregation window.
    int64 window_offset = (it->time - start) / resolution;
    base::Time window_start = start + (window_offset * resolution);
    base::Time window_end = window_start + resolution;
    base::Time last_sample_time = window_start;
    double integrated = 0.0;
    double metric_value = 0.0;

    // Aggregate the step function defined by the Metrics in |metrics|.
    while (it != metrics->end() && it->time <= window_end) {
      metric_value = it->value;
      integrated += metric_value * (it->time - last_sample_time).InSecondsF();
      last_sample_time = it->time;
      ++it;
    }
    if (it != metrics->end())
      metric_value = it->value;

    // If the window splits an area of the step function, split the aggregation
    // at the end of the window.
    integrated += metric_value * (window_end - last_sample_time).InSecondsF();
    double average = integrated / resolution.InSecondsF();
    results->push_back(Metric(window_end, average));
  }
  return results.Pass();
}

}  // namespace performance_monitor
