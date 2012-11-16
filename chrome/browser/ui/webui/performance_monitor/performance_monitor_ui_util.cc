// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/performance_monitor/performance_monitor_ui_util.h"

#include <algorithm>

#include "base/time.h"
#include "chrome/browser/performance_monitor/metric.h"

namespace performance_monitor {

namespace {

// Sorts the vector and returns the median. We don't need to sort it, but it is
// a by-product of finding the median, and allows us to pass a pointer, rather
// than construct another array.
double SortAndGetMedian(std::vector<double>* values) {
  size_t size = values->size();
  if (!size)
    return 0.0;

  std::sort(values->begin(), values->end());
  return size % 2 == 0 ?
      (values->at(size / 2) + values->at((size / 2) - 1)) / 2.0 :
      values->at(size / 2);
}

scoped_ptr<Database::MetricVector> NoAggregation(
    MetricType type,
    const Database::MetricVector* metrics,
    const base::Time& start) {
  scoped_ptr<Database::MetricVector> results(new Database::MetricVector());

  Database::MetricVector::const_iterator iter = metrics->begin();
  while (iter != metrics->end() && iter->time < start)
    ++iter;

  for (; iter != metrics->end(); ++iter)
    results->push_back(*iter);

  return results.Pass();
}

scoped_ptr<Database::MetricVector> AggregateByMedian(
    MetricType type,
    const Database::MetricVector* metrics,
    const base::Time& start,
    const base::TimeDelta& resolution) {
  scoped_ptr<Database::MetricVector> results(new Database::MetricVector());

  Database::MetricVector::const_iterator iter = metrics->begin();
  while (iter != metrics->end() && iter->time < start)
    ++iter;

  base::Time window_start = start;

  while (iter != metrics->end()) {
    std::vector<double> values;
    while (iter != metrics->end() && iter->time < window_start + resolution) {
      values.push_back(iter->value);
      ++iter;
    }

    if (!values.empty()) {
      results->push_back(Metric(type,
                                window_start + resolution,
                                SortAndGetMedian(&values)));
    }
    window_start += resolution;
  }

  return results.Pass();
}

scoped_ptr<Database::MetricVector> AggregateByMean(
    MetricType type,
    const Database::MetricVector* metrics,
    const base::Time& start,
    const base::TimeDelta& resolution) {
  scoped_ptr<Database::MetricVector> results(new Database::MetricVector());

  Database::MetricVector::const_iterator iter = metrics->begin();
  while (iter != metrics->end() && iter->time < start)
    ++iter;

  while (iter != metrics->end()) {
    // Finds the beginning of the next aggregation window.
    int64 window_offset = (iter->time - start) / resolution;
    base::Time window_start = start + (window_offset * resolution);
    base::Time window_end = window_start + resolution;
    base::Time last_sample_time = window_start;
    double integrated = 0.0;
    double metric_value = 0.0;

    // Aggregate the step function defined by the Metrics in |metrics|.
    while (iter != metrics->end() && iter->time <= window_end) {
      metric_value = iter->value;
      integrated += metric_value * (iter->time - last_sample_time).InSecondsF();
      last_sample_time = iter->time;
      ++iter;
    }
    if (iter != metrics->end())
      metric_value = iter->value;

    // If the window splits an area of the step function, split the aggregation
    // at the end of the window.
    integrated += metric_value * (window_end - last_sample_time).InSecondsF();
    double average = integrated / resolution.InSecondsF();
    results->push_back(Metric(type, window_end, average));
  }
  return results.Pass();
}

}  // namespace

double GetConversionFactor(UnitDetails from, UnitDetails to) {
  if (from.measurement_type != to.measurement_type) {
    LOG(ERROR) << "Invalid conversion requested";
    return 0.0;
  }

  return static_cast<double>(from.amount_in_base_units) /
      static_cast<double>(to.amount_in_base_units);
}

scoped_ptr<Database::MetricVector> AggregateMetric(
    MetricType type,
    const Database::MetricVector* metrics,
    const base::Time& start,
    const base::TimeDelta& resolution,
    AggregationMethod method) {
  if (!metrics)
    return scoped_ptr<Database::MetricVector>(NULL);

  CHECK(resolution > base::TimeDelta());

  switch (method) {
    case AGGREGATION_METHOD_NONE:
      return NoAggregation(type, metrics, start);
    case AGGREGATION_METHOD_MEDIAN:
      return AggregateByMedian(type, metrics, start, resolution);
    case AGGREGATION_METHOD_MEAN:
      return AggregateByMean(type, metrics, start, resolution);
    default:
      NOTREACHED();
      return scoped_ptr<Database::MetricVector>(NULL);
  }
}

}  // namespace performance_monitor
