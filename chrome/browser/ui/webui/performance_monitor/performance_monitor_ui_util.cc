// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/performance_monitor/performance_monitor_ui_util.h"

#include <algorithm>

#include "base/time/time.h"
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

}  // namespace

Aggregator::Aggregator() {
}

Aggregator::~Aggregator() {
}

scoped_ptr<VectorOfMetricVectors> Aggregator::AggregateMetrics(
    MetricType metric_type,
    const Database::MetricVector* metrics,
    const base::Time& start,
    const std::vector<TimeRange>& intervals,
    const base::TimeDelta& resolution) {
  scoped_ptr<VectorOfMetricVectors> results(new VectorOfMetricVectors());

  Database::MetricVector::const_iterator metric = metrics->begin();
  while (metric != metrics->end() && metric->time < start)
    ++metric;

  // For each interval, advance the metric to the start of the interval, and
  // append a metric vector for the aggregated data within that interval,
  // according to the appropriate strategy.
  for (std::vector<TimeRange>::const_iterator interval = intervals.begin();
       interval != intervals.end(); ++interval) {
    while (metric != metrics->end() && metric->time < interval->start)
      ++metric;

    results->push_back(*AggregateInterval(
        metric_type,
      &metric,
      metrics->end(),
      interval == intervals.begin() ? start : interval->start,
      interval->end,
      resolution));
  }

  return results.Pass();
}

scoped_ptr<Database::MetricVector> NoAggregation::AggregateInterval(
    MetricType metric_type,
    Database::MetricVector::const_iterator* metric,
    const Database::MetricVector::const_iterator& metric_end,
    const base::Time& time_start,
    const base::Time& time_end,
    const base::TimeDelta& resolution) {
  scoped_ptr<Database::MetricVector> aggregated_series(
      new Database::MetricVector());

  for (; *metric != metric_end && (*metric)->time <= time_end; ++(*metric))
    aggregated_series->push_back(**metric);

  return aggregated_series.Pass();
}

scoped_ptr<Database::MetricVector> MedianAggregation::AggregateInterval(
    MetricType metric_type,
    Database::MetricVector::const_iterator* metric,
    const Database::MetricVector::const_iterator& metric_end,
    const base::Time& time_start,
    const base::Time& time_end,
    const base::TimeDelta& resolution) {
  scoped_ptr<Database::MetricVector> aggregated_series(
      new Database::MetricVector());
  base::Time window_start = time_start;

  while (*metric != metric_end && (*metric)->time <= time_end) {
    std::vector<double> values;
    while (*metric != metric_end &&
           (*metric)->time <= time_end &&
           (*metric)->time < window_start + resolution) {
      values.push_back((*metric)->value);
      ++(*metric);
    }

    if (!values.empty()) {
      aggregated_series->push_back(Metric(metric_type,
                                          window_start + resolution,
                                          SortAndGetMedian(&values)));
    }
    window_start += resolution;
  }

  return aggregated_series.Pass();
}

scoped_ptr<Database::MetricVector> MeanAggregation::AggregateInterval(
    MetricType metric_type,
    Database::MetricVector::const_iterator* metric,
    const Database::MetricVector::const_iterator& metric_end,
    const base::Time& time_start,
    const base::Time& time_end,
    const base::TimeDelta& resolution) {
  scoped_ptr<Database::MetricVector> aggregated_series(
      new Database::MetricVector());

  while (*metric != metric_end && (*metric)->time <= time_end) {
    // Finds the beginning of the next aggregation window.
    int64 window_offset = ((*metric)->time - time_start) / resolution;
    base::Time window_start = time_start + (window_offset * resolution);
    base::Time window_end = window_start + resolution;
    base::Time last_sample_time = window_start;
    double integrated = 0.0;
    double metric_value = 0.0;

    // Aggregate the step function defined by the Metrics in |metrics|.
    while (*metric != metric_end && (*metric)->time <= window_end) {
      metric_value = (*metric)->value;
      integrated += metric_value *
                    ((*metric)->time - last_sample_time).InSecondsF();
      last_sample_time = (*metric)->time;
      ++(*metric);
    }
    if (*metric != metric_end)
      metric_value = (*metric)->value;

    // If the window splits an area of the step function, split the
    // aggregation at the end of the window.
    integrated += metric_value * (window_end - last_sample_time).InSecondsF();
    double average = integrated / resolution.InSecondsF();
    aggregated_series->push_back(Metric(metric_type, window_end, average));
  }

  return aggregated_series.Pass();
}

double GetConversionFactor(UnitDetails from, UnitDetails to) {
  if (from.measurement_type != to.measurement_type) {
    LOG(ERROR) << "Invalid conversion requested";
    return 0.0;
  }

  return static_cast<double>(from.amount_in_base_units) /
      static_cast<double>(to.amount_in_base_units);
}

scoped_ptr<VectorOfMetricVectors> AggregateMetric(
    MetricType type,
    const Database::MetricVector* metrics,
    const base::Time& start,
    const std::vector<TimeRange>& intervals,
    const base::TimeDelta& resolution,
    AggregationMethod method) {
  if (!metrics || intervals.empty())
    return scoped_ptr<VectorOfMetricVectors>();

  CHECK(resolution > base::TimeDelta());

  switch (method) {
    case AGGREGATION_METHOD_NONE:
      return NoAggregation().AggregateMetrics(
          type, metrics, start, intervals, resolution);
    case AGGREGATION_METHOD_MEDIAN:
      return MedianAggregation().AggregateMetrics(
          type, metrics, start, intervals, resolution);
    case AGGREGATION_METHOD_MEAN:
      return MeanAggregation().AggregateMetrics(
          type, metrics, start, intervals, resolution);
    default:
      NOTREACHED();
      return scoped_ptr<VectorOfMetricVectors>();
  }
}

}  // namespace performance_monitor
