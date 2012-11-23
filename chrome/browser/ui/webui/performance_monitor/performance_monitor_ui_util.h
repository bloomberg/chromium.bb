// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PERFORMANCE_MONITOR_PERFORMANCE_MONITOR_UI_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_PERFORMANCE_MONITOR_PERFORMANCE_MONITOR_UI_UTIL_H_

#include "chrome/browser/performance_monitor/database.h"
#include "chrome/browser/ui/webui/performance_monitor/performance_monitor_ui_constants.h"

namespace performance_monitor {

typedef std::vector<Database::MetricVector> VectorOfMetricVectors;

class PerformanceMonitorUtilTest;

// This is an interface for the various aggregation functions. The base class
// will handle the separation of the metrics vector into the various intervals,
// and each derived class represents a different method of aggregation.
class Aggregator {
 public:
  Aggregator();
  virtual ~Aggregator();

  // Aggregate a full metric vector into a vector of metric vectors, with each
  // metric vector representing the aggregation for an active interval. |start|
  // is the start time of the metrics to aggregate, |intervals| is the vector
  // of all intervals for which the database was active for the span to
  // aggregate, and |resolution| is the time distance between aggregated points.
  scoped_ptr<VectorOfMetricVectors> AggregateMetrics(
      MetricType metric_type,
      const Database::MetricVector* metrics,
      const base::Time& start,
      const std::vector<TimeRange>& intervals,
      const base::TimeDelta& resolution);

 protected:
  friend class PerformanceMonitorUtilTest;

  // Aggregate only a single interval, advancing the |metric| iterator to the
  // end of the active interval (or the end of the vector). |metric| points to
  // the start of the section to be aggregated, |metric_end| is the hard-limit
  // for the end of the vector (so we don't advance past it), |time_start| and
  // |time_end| represent the interval to aggregate, and |resolution| is the
  // time distance between aggregated points.
  virtual scoped_ptr<Database::MetricVector> AggregateInterval(
      MetricType metric_type,
      Database::MetricVector::const_iterator* metric,
      const Database::MetricVector::const_iterator& metric_end,
      const base::Time& time_start,
      const base::Time& time_end,
      const base::TimeDelta& resolution) = 0;
};

// Aggregation method classes.
class NoAggregation : public Aggregator {
 private:
  virtual scoped_ptr<Database::MetricVector> AggregateInterval(
      MetricType metric_type,
      Database::MetricVector::const_iterator* metric,
      const Database::MetricVector::const_iterator& metric_end,
      const base::Time& time_start,
      const base::Time& time_end,
      const base::TimeDelta& resolution) OVERRIDE;
};

class MedianAggregation : public Aggregator {
 private:
  virtual scoped_ptr<Database::MetricVector> AggregateInterval(
      MetricType metric_type,
      Database::MetricVector::const_iterator* metric,
      const Database::MetricVector::const_iterator& metric_end,
      const base::Time& time_start,
      const base::Time& time_end,
      const base::TimeDelta& resolution) OVERRIDE;
};

class MeanAggregation : public Aggregator {
 private:
  virtual scoped_ptr<Database::MetricVector> AggregateInterval(
      MetricType metric_type,
      Database::MetricVector::const_iterator* metric,
      const Database::MetricVector::const_iterator& metric_end,
      const base::Time& time_start,
      const base::Time& time_end,
      const base::TimeDelta& resolution) OVERRIDE;
};

// Return the factor by which all metrics should be multiplied in order to be in
// the preferred unit (e.g., memory usage is in bytes, but we display it in
// megabytes, so we return 1/1024^2).
double GetConversionFactor(UnitDetails from, UnitDetails to);

// Metric data can be either dense or sporadic, so AggregateMetric() normalizes
// the metric data in time. |metrics| must be sorted in increasing time.
// AggregateMetrics() will perform the aggregation according to the |strategy|
// provided.
//
// Median: Use the median of sample values from the window, ignoring
//         irregularity in sample timings.
// Mean: Use the mean value within the window.
// None: Return all samples from the window.
//
// In the methods which do aggregation, sampling windows start and end at an
// integer multiple of |resolution| away from |start| and data points are
// omitted if there are no points to resample. The time associated with each
// slice is the time at the end of the slice. This aggregation is performed for
// each interval within |intervals|.
//
// Returns a pointer to a vector of MetricVectors, with one MetricVector per
// interval given, NULL if |metrics| is empty.
scoped_ptr<std::vector<Database::MetricVector> > AggregateMetric(
    MetricType type,
    const Database::MetricVector* metrics,
    const base::Time& start,
    const std::vector<TimeRange>& intervals,
    const base::TimeDelta& resolution,
    AggregationMethod method);

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_UI_WEBUI_PERFORMANCE_MONITOR_PERFORMANCE_MONITOR_UI_UTIL_H_
